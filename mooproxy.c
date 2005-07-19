/*
 *
 *  mooproxy - a buffering proxy for moo-connections
 *  Copyright (C) 2002 Marcel L. Moreaux <marcelm@luon.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */



#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

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



static void die( World*, int, char * );
static void mainloop( World * );



int main( int argc, char **argv )
{
	World *world = NULL;
	int i;
	char *err, *worldname = NULL;
	time_t now = time( NULL );

	printf( "Starting mooproxy " VERSIONSTR " at %s", ctime( &now ) );
	uptime_started_now();

	set_up_signal_handlers();

	i = parse_command_line_options( argc, argv, &worldname, &err );
	if( i != EXIT_OK )
		die( world, i, err );

	if( worldname == NULL || worldname[0] == '\0' )
		die( world, EXIT_NOWORLD, strdup( 
				"You must supply a world name." ) );

	world = world_create( worldname );

	world_configfile_from_name( world );

	if( create_configdirs( &err ) )
		die( world, EXIT_CONFIGDIRS, err );

	printf( "Loading config...\n" );
	i = world_load_config( world, &err );
	if( i != EXIT_OK )
		die( world, i, err );

	/* Refuse if nonexistent / empty authstring */
	if( !world->authstring || !*world->authstring )
		die( world, EXIT_NOAUTH, strdup( "Authstring must be "
				"non-empty. Refusing to start." ) );

	printf( "Binding port...\n" );
	i = world_bind_port( world, &err );
	if( i != EXIT_OK )
		die( world, i, err );

	printf( "opened world %s\n", world->name );
	printf( "Ready for connections\n" );

	mainloop( world );

	world_destroy( world );

	exit( EXIT_OK );
}



static void die( World *wld, int ret, char *err )
{
	if( wld )
		world_destroy( wld );

	fprintf( stderr, "%s\n", err );
	free( err );
	exit( ret );
}



static void mainloop( World *wld )
{
	time_t last_checked = time( NULL ), ltime;
	Line *line;

	for(;;)
	{

	/* Wait for input from network to be placed in rx queues. */
	wait_for_network( wld );

	/* See if another second elapsed */
	ltime = time( NULL );
	if( ltime != last_checked )
		world_timer_tick( wld, last_checked = ltime );

	while( ( line = linequeue_pop( wld->server_rxqueue ) ) )
	{
		if( world_is_mcp( line->str ) )
		{
			/* MCP */
			world_do_mcp_server( wld, line );
		} else {
			/* Regular */
			linequeue_append( wld->client_txqueue, line );
		}
	}

	while( ( line = linequeue_pop( wld->client_rxqueue ) ) )
	{
		if( world_do_command( wld, line->str ) )
		{
			/* Command */
			free( line );
		}
		else if( world_is_mcp( line->str ) )
		{
			/* MCP */
			world_do_mcp_client( wld, line );
		}
		else
		{
			/* Regular */
			linequeue_append( wld->server_txqueue, line );
		}
	}

	/* Send loggable lines to log queue. */
	for( line = wld->client_txqueue->head; line; line = line->next )
	{
		if( !( line->flags & LINE_DONTLOG ) )
			linequeue_append( wld->client_logqueue,
					line_dup( line ) );

		/* Lines sent to the logqueue should not be logged again. */
		line->flags |= LINE_DONTLOG;
	}

	if( !world_connected_to_client( wld ) )
	{
		/* If we aren't connected to the client, move queued lines
		 * to the buffer. */
		while( ( line = linequeue_pop( wld->client_txqueue ) ) )
			if( line->flags & LINE_DONTBUF )
				line_destroy( line );
			else
				linequeue_append( wld->client_buffered, line );
	}

	/* Flush the send buffers */
	world_flush_client_logqueue( wld );
	world_flush_server_txbuf( wld );
	world_flush_client_txbuf( wld );

	/* Trim the dynamic buffers if they're too long */
	world_trim_dynamic_queues( wld );

	/* We want to disconnect from the server? */
	if( wld->flags & WLD_SERVERQUIT )
	{
		world_disconnect_server( wld );
		wld->flags &= ~WLD_SERVERQUIT;
	}

	/* Client wants to be disconnected? */
	if( wld->flags & WLD_CLIENTQUIT )
	{
		world_disconnect_client( wld );
		wld->flags &= ~WLD_CLIENTQUIT;
	}

	/* Request for shutdown? */
	if( wld->flags & WLD_SHUTDOWN )
		return;

	}
}
