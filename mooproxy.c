/*
 *
 *  mooproxy - a buffering proxy for moo-connections
 *  Copyright (C) 2001-2007 Marcel L. Moreaux <marcelm@luon.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 dated June, 1991.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */



#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "global.h"
#include "daemon.h"
#include "config.h"
#include "world.h"
#include "network.h"
#include "timer.h"
#include "log.h"
#include "mcp.h"
#include "misc.h"
#include "command.h"
#include "resolve.h"
#include "crypt.h"
#include "line.h"



static void die( World*, char * );
static void mainloop( World * );
static void handle_flags( World * );
static void print_help_text( void );
static void print_version_text( void );
static void print_license_text( void );



int main( int argc, char **argv )
{
	Config config;
	World *world = NULL;
	char *err = NULL, *warn = NULL;
	pid_t pid;
	int i;

	/* Parse commandline options */
	parse_command_line_options( argc, argv, &config );

	/* Ok, what is it the user wants us to do? */
	switch( config.action )
	{
		case PARSEOPTS_ERROR:
			die( world, config.error );
			exit( EXIT_FAILURE );
		case PARSEOPTS_HELP:
			print_help_text();
			exit( EXIT_SUCCESS );
		case PARSEOPTS_VERSION:
			print_version_text();
			exit( EXIT_SUCCESS );
		case PARSEOPTS_LICENSE:
			print_license_text();
			exit( EXIT_SUCCESS );
		case PARSEOPTS_MD5CRYPT:
			prompt_to_md5hash();
			exit( EXIT_SUCCESS );
	}

	/* Create the configuration dir hierarchy. */
	if( create_configdirs( &err ) != 0 )
		die( world, err );

	/* Check if we received a world name. */
	if( config.worldname == NULL || config.worldname[0] == '\0' )
		die( world, xstrdup( "You must supply a world name." ) );

	/* Announce that we are starting up. */
	printf( "Starting mooproxy " VERSIONSTR " at %s.\n",
			time_string( time( NULL ), FULL_TIME ) );

	/* Register startup time. */
	uptime_started_now();

	/* Register the signal handlers (duh). */
	set_up_signal_handlers();

	/* Create the uninitialized world now. */
	world = world_create( config.worldname );
	world_configfile_from_name( world );

	/* Check the permissions on the configuration dirs/files, and
	 * warn the user if they're too weak. */
	if( check_configdir_perms( &warn, &err ) != 0 )
		die( world, err );
	if( warn )
	{
		printf( "%s", warn );
		free( warn );
	}

	/* Make sure this world isn't open yet. */
	if( world_acquire_lock_file( world, &err ) )
		die( world, err );

	/* Load the world's configuration. */
	printf( "Opening world %s.\n", config.worldname );
	if( world_load_config( world, &err ) != 0 )
		die( world, err );

	/* Refuse to start if the authentication string is absent. */
	if( world->auth_md5hash == NULL )
		die( world, xstrdup( "No authentication string given in "
				"configuration file. Refusing to start." ) );

	/* Bind to network port */
	printf( "Binding to port.\n" );
	world_bind_port( world, world->listenport );

	/* Fatal error, abort. */
	if( world->bindresult->fatal )
		die( world, xstrdup( world->bindresult->fatal ) );

	/* Print the result for each address family. */
	for( i = 0; i < world->bindresult->af_count; i++ )
		printf( "  %s %s\n", world->bindresult->af_success[i] ?
				" " : "!", world->bindresult->af_msg[i] );

	/* We need success on at least one address family. */
	if( world->bindresult->af_success_count == 0 )
		die( world, xstrdup( world->bindresult->conclusion ) );

	printf( "%s\n", world->bindresult->conclusion );

	/* Move the listening FDs to the world. */
	world->listen_fds = world->bindresult->listen_fds;
	world->bindresult->listen_fds = NULL;

	/* Stay on foreground, or daemonize. */
	if( config.no_daemon )
	{
		pid = getpid();
		world_write_pid_to_file( world, pid );
		printf( "Mooproxy successfully started. Staying in "
				"foreground (pid %li).\n", (long) pid );
	} else {
		pid = daemonize( &err );
		/* Handle errors. */
		if( pid == -1 )
			die( world, err );

		/* If pid > 0, we're the parent. Say goodbye to the user! */
		if( pid > 0 )
		{
			world_write_pid_to_file( world, pid );
			printf( "Mooproxy successfully started in PID %li.\n",
					(long) pid );
			launch_parent_exit( EXIT_SUCCESS );
		}

		/* If pid == 0, we continue as the child. */
	}

	/* Initialization done, enter the main loop. */
	mainloop( world );

	/* Clean up a bit. */
	world_remove_lockfile( world );
	world_sync_logdata( world );
	world_log_link_remove( world );
	world_destroy( world );

	exit( EXIT_SUCCESS );
}



/* If wld is not NULL, destroy it. Print err. Terminate with failure. */
static void die( World *wld, char *err )
{
	if( wld != NULL && wld->lockfile != NULL )
		world_remove_lockfile( wld );

	if( wld != NULL )
		world_destroy( wld );

	fprintf( stderr, "%s\n", err );
	free( err );

	exit( EXIT_FAILURE );
}



/* The main loop which ties everything together. */
static void mainloop( World *wld )
{
	time_t last_checked = time( NULL ), ltime;
	Line *line;

	/* Initialize the time administration. */
	world_timer_init( wld, last_checked );
	set_current_time( last_checked );

	/* Loop forever. */
	for(;;)
	{

	/* Wait for input from network to be placed in rx queues. */
	wait_for_network( wld );

	/* See if another second elapsed */
	ltime = time( NULL );
	if( ltime != last_checked )
		world_timer_tick( wld, ltime );
	last_checked = ltime;

	/* Dispatch lines from the server */
	while( ( line = linequeue_pop( wld->server_rxqueue ) ) )
	{
		if( world_is_mcp( line->str ) )
		{
			/* MCP */
			world_do_mcp_server( wld, line );
		} else {
			/* Regular */
			linequeue_append( wld->client_toqueue, line );
		}
	}

	/* Dispatch lines from the client */
	while( ( line = linequeue_pop( wld->client_rxqueue ) ) )
	{
		if( world_do_command( wld, line->str ) )
		{
			/* Command (those are activating). */
			wld->flags |= WLD_ACTIVATED;
			free( line );
		}
		else if( world_is_mcp( line->str ) )
		{
			/* MCP. */
			world_do_mcp_client( wld, line );
		} else {
			/* Regular lines are always activating. */
			wld->flags |= WLD_ACTIVATED;
			linequeue_append( wld->server_toqueue, line );
		}
	}

	/* Process server toqueue to txqueue */
	linequeue_merge( wld->server_txqueue, wld->server_toqueue );

	/* Process client toqueue to txqueue */
	while( ( line = linequeue_pop( wld->client_toqueue ) ) )
	{
		/* Log if logging is enabled, and the line is loggable */
		if( wld->logging_enabled && !( line->flags & LINE_DONTLOG ) )
			world_log_line( wld, line );

		/* If the same line makes it back here again, it should not
		 * be logged again */
		line->flags |= LINE_DONTLOG;

		linequeue_append( wld->client_txqueue, line );
	}

	/* If we aren't connected to the client, move queued lines to the
	 * buffer. */
	if( wld->client_status != ST_CONNECTED )
	{
		while( ( line = linequeue_pop( wld->client_txqueue ) ) )
			if( line->flags & LINE_DONTBUF )
				line_destroy( line );
			else
				linequeue_append( wld->buffered_lines, line );
	}

	/* Flush the send buffers */
	world_flush_client_logqueue( wld );
	world_flush_server_txbuf( wld );
	world_flush_client_txbuf( wld );

	/* Trim the dynamic buffers if they're too long */
	world_trim_dynamic_queues( wld );

	handle_flags( wld );

	if( wld->flags & WLD_SHUTDOWN )
		return;

	}
}



/* Handle wld->flags. Clear each flag which has been handled. */
static void handle_flags( World *wld )
{
	if( wld->flags & WLD_ACTIVATED )
	{
		wld->flags &= ~WLD_ACTIVATED;
		world_inactive_to_history( wld );
	}

	if( current_time() < wld->client_last_notconnmsg + NOTCONN_MSGINTERVAL )
		wld->flags &= ~WLD_NOTCONNECTED;
	if( wld->flags & WLD_NOTCONNECTED )
	{
		wld->flags &= ~WLD_NOTCONNECTED;

		if( wld->server_status == ST_RESOLVING ||
				wld->server_status == ST_CONNECTING )
			world_msg_client( wld, "Connection attempt (to %s) "
				"still in progress!", wld->server_host );
		else
			world_msg_client( wld, "Not connected to server!" );

		wld->client_last_notconnmsg = current_time();
	}

	if( wld->flags & WLD_CLIENTQUIT )
	{
		wld->flags &= ~WLD_CLIENTQUIT;
		world_disconnect_client( wld );
	}

	if( wld->flags & WLD_SERVERQUIT )
	{
		wld->flags &= ~WLD_SERVERQUIT;
		world_disconnect_server( wld );
	}

	if( wld->flags & WLD_RECONNECT )
	{
		wld->flags &= ~WLD_RECONNECT;
		world_schedule_reconnect( wld );
	}

	if( wld->flags & WLD_SERVERRESOLVE )
	{
		wld->flags &= ~WLD_SERVERRESOLVE;
		world_start_server_resolve( wld );
	}

	if( wld->flags & WLD_SERVERCONNECT )
	{
		wld->flags &= ~WLD_SERVERCONNECT;
		world_start_server_connect( wld );
	}

	if( wld->flags & WLD_LOGLINKUPDATE )
	{
		wld->flags &= ~WLD_LOGLINKUPDATE;
		world_log_link_update( wld );
	}

	if( wld->flags & WLD_REBINDPORT )
	{
		wld->flags &= ~WLD_REBINDPORT;
		world_rebind_port( wld );
	}
}



/* Print the help output (-h, --help) */
static void print_help_text( void )
{
	printf( "Mooproxy - a buffering proxy for moo-connections\n"
	"Copyright (C) 2001-2007 Marcel L. Moreaux <marcelm@luon.net>\n"
	"\n"
	"usage: mooproxy [options]\n"
	"\n"
	"  -h, --help        shows this help screen and exits\n"
	"  -V, --version     shows version information and exits\n"
	"  -L, --license     shows licensing information and exits\n"
	"  -w, --world       world to load\n"
	"  -d, --no-daemon   forces mooproxy to stay in the foreground\n"
	"  -m, --md5crypt    prompts for a string to create an md5 hash of\n"
	"\n"
	"Released under the GPL v2, report bugs to <marcelm@luon.net>\n"
	"Mooproxy comes with ABSOLUTELY NO WARRANTY; "
	"for details run mooproxy --license\n" );
}



/* Print the version output (-V, --version) */
static void print_version_text( void )
{
	printf( "Mooproxy version " VERSIONSTR
			". Copyright (C) 2001-2007 Marcel L Moreaux\n" );
}



/* Print the license output (-L, --license) */
static void print_license_text( void )
{
	printf( "Mooproxy - a buffering proxy for moo-connections\n"
	"Copyright (C) 2001-2007 Marcel L. Moreaux <marcelm@luon.net>\n"
	"\n"
	"This program is free software; you can redistribute it and/or modify\n"
	"it under the terms of the GNU General Public License as published by\n"
	"the Free Software Foundation; version 2 dated June, 1991.\n"
	"\n"
	"This program is distributed in the hope that it will be useful,\n"
	"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
	"GNU General Public License for more details.\n" );
}
