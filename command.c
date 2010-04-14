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



#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#include "world.h"
#include "command.h"
#include "misc.h"
#include "network.h"
#include "config.h"
#include "log.h"
#include "daemon.h"
#include "resolve.h"
#include "recall.h"



static int try_command( World *wld, char *cmd, char *args );
static int try_getoption( World *wld, char *key, char *args );
static int try_setoption( World *wld, char *key, char *args );
static int command_index( char *cmd );
static int refuse_arguments( World *wld, char *cmd, char *args );
static void show_longdesc( World *wld, char *desc );
static void show_help( World *wld );
static int show_command_help( World *wld, char *cmd );
static int show_setting_help( World *wld, char *key );

static void command_help( World *wld, char *cmd, char *args );
static void command_quit( World *wld, char *cmd, char *args );
static void command_shutdown( World *wld, char *cmd, char *args );
static void command_connect( World *wld, char *cmd, char *args );
static void command_disconnect( World *wld, char *cmd, char *args );
static void command_settings( World *wld, char *cmd, char *args );
static void command_recall( World *wld, char *cmd, char *args );
static void command_ace( World *wld, char *cmd, char *args );
static void command_version( World *wld, char *cmd, char *args );
static void command_date( World *wld, char *cmd, char *args );
static void command_uptime( World *wld, char *cmd, char *args );
static void command_world( World *wld, char *cmd, char *args );
static void command_forget( World *wld, char *cmd, char *args );
static void command_authinfo( World *wld, char *cmd, char *args );



static const struct
{
	char *cmd;
	void (*func)( World *, char *, char * );
	char *args;
	char *shortdesc;
	char *longdesc;
}
cmd_db[] =
{
	{ "help", command_help, "[<topic>]",
	"Helps you.",
	"Without argument, displays a summary of commands and settings.\n"
	"When the name of a command or setting is provided, displays \n"
	"detailed help for that command or setting." },

	{ "quit", command_quit, "",
	"Disconnects your client from mooproxy.",
	NULL },

	{ "shutdown", command_shutdown, "[-f]",
	"Shuts down mooproxy.",
	"Shuts down mooproxy.\n\n"
	"Under some circumstances, mooproxy may refuse to shut down\n"
	"(for example if not all loggable lines have been written to\n"
	"disk). -f may be used to force shutdown." },

	{ "connect", command_connect, "[<host> [<port>]]",
	"Connects to the server.",
	"Connects to the server.\n\n"
	"The arguments, <host> and <port>, are optional. If they're not\n"
	"provided, mooproxy will use the settings \"host\" and \"port\"." },

	{ "disconnect", command_disconnect, "",
	"Disconnects from the server.",
	NULL },

	{ "settings", command_settings, "",
	"Lists the avaliable settings.",
	"Lists all available settings, and their current values." },

	{ "recall", command_recall, "<...>",
	"Recalls some lines from history.",
	"FIXME" },

	{ "ace", command_ace, "[<C>x<R> | off]",
	"En/disables ANSI client emulation.",
	"Enables or disables ANSI client emulation (ACE).\n"
	"\n"
	"ACE is intended to be used when the client is a text terminal\n"
	"(such as telnet or netcat running in a terminal window) rather\n"
	"than a proper MOO client.\n"
	"ACE makes mooproxy send ANSI escape sequences that will emulate\n"
	"the behaviour of a primitive MOO client on a text terminal.\n"
	"\n"
	"Because mooproxy cannot know the size of your terminal, you have\n"
	"to supply the size of your terminal to the ace command as COLxROW,\n"
	"where COL is the number of columns, and ROW is the number of rows.\n"
	"\n"
	"If you invoke the command without arguments, and ACE was not yet\n"
	"enabled, ACE will be enabled with a default size of 80x24.\n"
	"If you invoke the command without arguments, and ACE was already\n"
	"enabled, ACE will re-initialize with the last known size.\n"
	"\n"
	"When invoked with \"off\", ACE will be disabled.\n"
	"\n"
	"ACE will be automatically disabled when you disconnect or reconnect.\n"
	"\n"
	"ACE is known to work properly with the Linux console, xterm, rxvt,\n"
	"gnome-terminal, putty, and Windows XP telnet.\n" },

	{ "version", command_version, "",
	"Shows the mooproxy version.",
	NULL },

	{ "date", command_date, "",
	"Shows the current date and time.",
	NULL },

	{ "uptime", command_uptime, "",
	"Shows mooproxy's starting time and uptime.",
	NULL },

	{ "world", command_world, "",
	"Shows the name of the current world.",
	NULL },

	{ "forget", command_forget, "",
	"Forgets all history lines.",
	NULL },

	{ "authinfo", command_authinfo, "",
	"Shows some authentication information.",
	NULL },

	{ NULL, NULL, NULL, NULL, NULL }
};



extern int world_do_command( World *wld, char *line )
{
	char backup, *backuppos, *args, *cmd = line, *cstr = wld->commandstring;

	/* Ignore the equal parts of commandstr and the line */
	while( *cstr && *cstr == *cmd )
	{
		cstr++;
		cmd++;
	}

	/* If there's still something left in cstr, it's not a command */
	if( *cstr )
		return 0;

	/* Now separate the command from the args */
	args = cmd;
	while( *args && !isspace( *args ) )
		args++;

	/* Mark the end of the command */
	backup = *args;
	backuppos = args;
	*args = '\0';

	/* Advance args to the actual start of the arguments. */
	if( backup )
		args++;
	/* Remove enclosing whitespace, too. */
	args = trim_whitespace( args );

	/* Try parsing cmd + args as a command, an option query, and an
	 * option update, in that order.
	 * The try_* functions may modify cmd and args _only_ if they
	 * successfully parse the command. They may never free them. */
	if( try_command( wld, cmd, args ) )
	{
		free( line );
		return 1;
	}

	if( try_getoption( wld, cmd, args ) )
	{
		free( line );
		return 1;
	}

	if( try_setoption( wld, cmd, args ) )
	{
		free( line );
		return 1;
	}

	/* Ok, it's not something we understand. */

	if( !wld->strict_commands )
	{
		/* Strictcmd is off, the line should now be
		 * processed as a regular one. */
		*backuppos = backup;
		return 0;
	}

	/* Invalid command, complain */
	world_msg_client( wld, "No such command or option: %s.", cmd );
	free( line );
	return 1;
}



/* Try if cmd is a valid command.
 * If it is, call the proper function and return 1. Otherwise, return 0. */
static int try_command( World *wld, char *cmd, char *args )
{
	int idx;

	/* See if it's a command, and if so, call the proper function. */
	idx = command_index( cmd );
	if( idx > -1 )
	{
		(*cmd_db[idx].func)( wld, cmd, args );
		return 1;
	}

	return 0;
}



/* Try if key is a valid key and args is empty.
 * If it is, query the option value and return 1. Otherwise, return 0. */
static int try_getoption( World *wld, char *key, char *args )
{
	char *val, *kend;

	/* If we have arguments, the option is not being queried. */
	if( *args )
		return 0;

	/* Make kend point to the last char of key, or \0 if key is empty. */
	kend = key + strlen( key );
	if( kend != key )
		kend--;

	switch( world_get_key( wld, key, &val ) )
	{
		case GET_KEY_NF:
		/* Key not found, tell the caller to keep trying. */
		return 0;
		break;

		case GET_KEY_OK:
		world_msg_client( wld, "The option %s is %s.", key, val );
		free( val );
		break;

		case GET_KEY_PERM:
		world_msg_client( wld, "The option %s may not be read.", key );
		break;
	}

	return 1;
}



/* Try if key is a valid key and args is non-empty.
 * If it is, update the option value and return 1. Otherwise, return 0. */
static int try_setoption( World *wld, char *key, char *args )
{
	char *val, *tmp, *err;

	/* If we have no arguments, the option is not being set. */
	if( !*args )
		return 0;

	/* Remove any enclosing quotes.
	 * We strdup, because we may not modify args yet. */
	val = remove_enclosing_quotes( xstrdup( args ) );

	switch( world_set_key( wld, key, val, &err ) )
	{
		case SET_KEY_NF:
		/* Key not found, tell the caller to keep trying. */
		free( val );
		return 0;
		break;

		case SET_KEY_PERM:
		world_msg_client( wld, "The option %s may not be set.", key );
		break;

		case SET_KEY_BAD:
		world_msg_client( wld, "%s", err );
		free( err );
		break;

		case SET_KEY_OKSILENT:
		break;

		case SET_KEY_OK:
		/* Query the option we just set, so we get a normalized
		 * representation of the value. */
		if( world_get_key( wld, key, &tmp ) == GET_KEY_OK )
		{
			world_msg_client( wld, "The option %s is now %s.",
					key, tmp );
			free( tmp );
			break;
		}
		/* The key was successfully set, but we could not query the
		 * new value. Maybe the option is hidden or it may not be
		 * read. Just say it's been changed. */
		world_msg_client( wld, "The option %s has been changed.", key );
		break;
	}

	free( val );
	return 1;
}



/* Returns the index of the command in the db, or -1 if not found. */
static int command_index( char *cmd )
{
	int i;

	for( i = 0; cmd_db[i].cmd; i++ )
		if( !strcmp( cmd, cmd_db[i].cmd ) )
			return i;

	return -1;
}



/* If the args contains anything else than whitespace, complain to the
 * client and return 1. Otherwise, NOP and return 0. */
static int refuse_arguments( World *wld, char *cmd, char *args )
{
	/* Search for \0 or first non-whitespace char */
	while( *args != '\0' && isspace( *args ) )
		args++;

	/* If that char is \0, it's not non-whitespace */
	if( *args == '\0' )
		return 0;

	world_msg_client( wld, "The command %s does not take arguments.", cmd );
	return 1;
}



/* Shows a \n-separated string of lines to the client, line by line.
 * Each line is indented by two spaces. */
static void show_longdesc( World *wld, char *desc )
{
	char *s, *t;

	s = xmalloc( strlen( desc ) + 1 );

	while( *desc )
	{
		/* s is our temporary storage. Copy the current line of desc
		 * to s, and advance desc to point to the next line. */
		t = s;
		while( *desc != '\0' && *desc != '\n' )
			*t++ = *desc++;
		*t = '\0';
		if( *desc == '\n' )
			desc++;

		/* And send the copied line off to the client. */
		world_msg_client( wld, "  %s", s );
	}

	free( s );
}



/* Display the "generic" help, listing all commands and settings. */
static void show_help( World *wld )
{
	int i, numkeys, longest = 0;
	char **keylist, *desc, *tmp;

	world_msg_client( wld, "" );
	world_msg_client( wld, "Use %shelp <command> or %shelp <setting> "
			"to get more detailed help.",
			wld->commandstring, wld->commandstring );
	world_msg_client( wld, "" );

	/* Determine the longest command (for layout). */
	for( i = 0; cmd_db[i].cmd; i++ )
	{
		/* We're displaying the command name plus its arguments, so
		 * we need to determine the length of both. */
		int l = strlen( cmd_db[i].cmd ) + strlen( cmd_db[i].args ) + 1;
		if( l > longest )
			longest = l;
	}

	/* Get the list of settings, and determine the longest cmd/setting. */
	numkeys = world_get_key_list( wld, &keylist );
	for( i = 0; i < numkeys; i++ )
		if( strlen( keylist[i] ) > longest )
			longest = strlen( keylist[i] );

	/* Display the commands. */
	world_msg_client( wld, "Commands" );
	for( i = 0; cmd_db[i].cmd; i++ )
	{
		/* Join the command and its arguments, we need to feed them
		 * to printf as one string. */
		xasprintf( &tmp, "%s %s", cmd_db[i].cmd, cmd_db[i].args );
		world_msg_client( wld, "  %*s    %s", -longest, tmp,
				cmd_db[i].shortdesc );
		free( tmp );
	}

	/* Display the settings. */
	world_msg_client( wld, "" );
	world_msg_client( wld, "Settings" );
	for( i = 0; i < numkeys; i++ )
	{
		char *key = keylist[i];

		desc = ""; /* In case world_desc_key() fails. */
		world_desc_key( wld, key, &desc, &tmp);

		world_msg_client( wld, "  %*s    %s", -longest, key, desc );
	}

	free( keylist );
}



/* Displays the help for one command.
 * Returns 1 if it was able to show the help for cmd, 0 otherwise. */
static int show_command_help( World *wld, char *cmd )
{
	int idx;

	idx = command_index( cmd );
	if( idx < 0 )
		return 0;

	world_msg_client( wld, "" );
	world_msg_client( wld, "%s%s %s", wld->commandstring,
			cmd_db[idx].cmd, cmd_db[idx].args );
	world_msg_client( wld, "" );

	if( cmd_db[idx].longdesc )
		show_longdesc( wld, cmd_db[idx].longdesc );
	else
		show_longdesc( wld, cmd_db[idx].shortdesc );

	return 1;
}



/* Displays the help for one setting.
 * Returns 1 if it was able to show the help for key, 0 otherwise. */
static int show_setting_help( World *wld, char *key )
{
	char *shortdesc, *longdesc;

	if( world_desc_key( wld, key, &shortdesc, &longdesc ) != GET_KEY_OK )
		return 0;

	world_msg_client( wld, "" );
	world_msg_client( wld, "%s", key );
	world_msg_client( wld, "" );

	if( longdesc )
		show_longdesc( wld, longdesc );
	else
		show_longdesc( wld, shortdesc );

	return 1;
}



/* Give help on a command or setting. If the command or setting is not
 * recognized, show some generic help. */
static void command_help( World *wld, char *cmd, char *args )
{
	/* Generic help. */
	if( !*args )
	{
		show_help( wld );
		return;
	}

	/* Help for one command. */
	if( show_command_help( wld, args ) )
		return;

	/* Help for one setting. */
	if( show_setting_help( wld, args ) )
		return;

	/* And if all else fails: generic help. */
	show_help( wld );
}



/* Disconnect the client. No arguments. */
static void command_quit( World *wld, char *cmd, char *args )
{
	Line *line;

	if( refuse_arguments( wld, cmd, args ) )
		return;

	/* We try to leave their terminal in a nice state when they leave. */
	if( wld->ace_enabled )
		world_disable_ace( wld );

	world_msg_client( wld, "Closing connection." );
	wld->flags |= WLD_CLIENTQUIT;

	/* And one for the log... */
	line = world_msg_client( wld, "Client /quit." );
	line->flags = LINE_LOGONLY;
}



/* Shut mooproxy down (forcibly on -f). Arguments: [-f] */
static void command_shutdown( World *wld, char *cmd, char *args )
{
	long unlogged_lines;
	float unlogged_kib;
	char *tmp;
	Line *line;

	tmp = get_one_word( &args );

	/* Garbage arguments? */
	if( tmp != NULL && strcmp( tmp, "-f" ) )
	{
		world_msg_client( wld, "Unrecognised argument: `%s'", tmp );
		return;
	}

	/* Forced shutdown. */
	if( tmp != NULL && !strcmp( tmp, "-f" ) )
	{
		/* We try to leave their terminal in a nice state. */
		if( wld->ace_enabled )
			world_disable_ace( wld );

		line = world_msg_client( wld, "Shutting down forcibly." );
		line->flags = LINE_CHECKPOINT;
		wld->flags |= WLD_SHUTDOWN;
		return;
	}

	/* If we got here, there were no arguments. */

	/* Unlogged data? Refuse. */
	if( wld->log_queue->size + wld->log_current->size +
			wld->log_bfull > 0 )
	{
		unlogged_lines = wld->log_queue->count +
				wld->log_current->count + wld->log_bfull / 80;
		unlogged_kib = ( wld->log_bfull + wld->log_queue->size +
				wld->log_current->size ) / 1024.0;
		world_msg_client( wld, "There are approximately %li lines "
				"(%.1fKiB) not yet logged to disk. ",
				unlogged_lines, unlogged_kib );
		world_msg_client( wld, "Refusing to shut down. "
				"Use /shutdown -f to override." );
		return;
	}

	/* We try to leave their terminal in a nice state. */
	if( wld->ace_enabled )
		world_disable_ace( wld );

	/* We're good, proceed. */
	line = world_msg_client( wld, "Shutting down." );
	line->flags = LINE_CHECKPOINT;
	wld->flags |= WLD_SHUTDOWN;
}



/* Connect to the server. Arguments: [host [port]]
 * The function sets wld->server_host and wld->server_port and then
 * flags the world for server resolve start. */
static void command_connect( World *wld, char *cmd, char *args )
{
	char *tmp;
	long port;

	/* Are we already connected? */
	if( wld->server_status == ST_CONNECTED )
	{
		world_msg_client( wld, "Already connected to %s.",
				wld->server_host );
		return;
	}

	/* Or in the process of connecting? */
	if( wld->server_status == ST_RESOLVING ||
			wld->server_status == ST_CONNECTING )
	{
		world_msg_client( wld, "Connection attempt to %s already "
				"in progress.", wld->server_host );
		return;
	}

	/* Or perhaps we're waiting to reconnect? */
	if( wld->server_status == ST_RECONNECTWAIT )
	{
		world_msg_client( wld, "Resetting autoreconnect delay and "
				"reconnecting immediately." );
		wld->reconnect_delay = 0;
		wld->reconnect_at = current_time();
		world_do_reconnect( wld );
		return;
	}

	/* First, clean up existing target. */
	free( wld->server_host );
	wld->server_host = NULL;
	free( wld->server_port );
	wld->server_port = NULL;

	/* If there is an argument, use the first word as hostname */
	if( ( tmp = get_one_word( &args ) ) != NULL )
		wld->server_host = xstrdup( tmp );

	/* If there's another argument, use that as the port */
	if( ( tmp = get_one_word( &args ) ) != NULL )
	{
		port = strtol( tmp, NULL, 0 );
		/* See that the argument port is valid. */
		if( port < 1 || port > 65535 )
		{
			world_msg_client( wld, "`%s' is not a valid port "
					"number.", tmp );
			return;
		}

		xasprintf( &wld->server_port, "%li", port );
	}

	/* If there's no server hostname at all, complain. */
	if( wld->server_host == NULL && wld->dest_host == NULL )
	{
		world_msg_client( wld, "No server host to connect to." );
		return;
	}

	/* If there's no argument hostname, use the configured one. */
	if( wld->server_host == NULL )
		wld->server_host = xstrdup( wld->dest_host );

	/* If there's no server port at all, complain. */
	if( wld->server_port == NULL && wld->dest_port == -1 )
	{
		world_msg_client( wld, "No server port to connect to." );
		return;
	}

	/* If there's no argument port, use the configured one. */
	if( wld->server_port == NULL )
		xasprintf( &wld->server_port, "%li", wld->dest_port );

	/* Inform the client */
	world_msg_client( wld, "Connecting to %s, port %s",
			wld->server_host, wld->server_port );

	/* We don't reconnect if a new connection attempt fails. */
	wld->reconnect_enabled = 0;

	/* Start the resolving */
	wld->flags |= WLD_SERVERRESOLVE;
}



/* Disconnects from the server. No arguments. */
static void command_disconnect( World *wld, char *cmd, char *args )
{
	Line *line;

	if( refuse_arguments( wld, cmd, args ) )
		return;

	switch ( wld->server_status )
	{
		case ST_DISCONNECTED:
		world_msg_client( wld, "Not connected." );
		break;

		case ST_RESOLVING:
		world_cancel_server_resolve( wld );
		world_msg_client( wld, "Connection attempt aborted." );
		break;

		case ST_CONNECTING:
		world_cancel_server_connect( wld );
		world_msg_client( wld, "Connection attempt aborted." );
		break;

		case ST_CONNECTED:
		wld->flags |= WLD_SERVERQUIT;
		line = world_msg_client( wld, "Disconnected from server." );
		line->flags = LINE_CHECKPOINT;
		break;

		case ST_RECONNECTWAIT:
		wld->server_status = ST_DISCONNECTED;
		wld->reconnect_delay = 0;
		world_msg_client( wld, "Canceled reconnect." );
	}
}



/* Prints a list of options and their values. No arguments. */
static void command_settings( World *wld, char *cmd, char *args )
{
	char **list, *key, *val;
	int i, num, longest = 0;

	if( refuse_arguments( wld, cmd, args ) )
		return;

	world_msg_client( wld, "Settings" );

	num = world_get_key_list( wld, &list );

	/* Calculate the longest key name, so we can line out the table. */
	for( i = 0; i < num; i++ )
		if( strlen( list[i] ) > longest )
			longest = strlen( list[i] );

	for( i = 0; i < num; i++ )
	{
		key = list[i];

		if( world_get_key( wld, key, &val ) != GET_KEY_OK )
			val = xstrdup( "-" );

		world_msg_client( wld, "  %*s    %s", -longest, key, val );

		free( val );
	}

	free( list );
}



/* Recalls lines. FIXME: better description. */
static void command_recall( World *wld, char *cmd, char *args )
{
	Linequeue *queue;
	long count;
	char *str;

	/* If there are no arguments, print terse usage. */
	if( args[0] == '\0' )
	{
		world_msg_client( wld, "Use: recall [from <when>] [to <when>]"
				" [search <text>]" );
		world_msg_client( wld, "Or:  recall last <number>"
				" [search <text>]" );
		world_msg_client( wld, "Or:  recall <number>" );
		world_msg_client( wld, "See the README file for details." );
		return;
	}

	/* We want to include the inactive lines in our recall as well, so
	 * we move them to the history right now. */
	world_inactive_to_history( wld );

	/* Check if there are any lines to recall. */
	if( wld->history_lines->count == 0 )
	{
		world_msg_client( wld, "There are no lines to recall." );
		return;
	}

	/* Check if the arg string is all digits. */
	for( str = args; isdigit( *str ); str++ )
		continue;

	/* It isn't, pass it on for more sophisticated processing. */
	if( *str != '\0' )
	{
		world_recall_command( wld, args );
		return;
	}

	/* Ok, it's all digits, do 'classic' recall. */
	count = atol( args );
	if( count <= 0 )
	{
		world_msg_client( wld, "Number of lines to recall should be "
				"greater than zero." );
		return;
	}

	/* Get the recalled lines. */
	queue = world_recall_history( wld, count );

	/* Announce the recall. */
	world_msg_client( wld, "Recalling %lu line%s.", queue->count,
			( queue->count == 1 ) ? "" : "s" );

	/* Append the recalled lines, and clean up. */
	linequeue_merge( wld->client_toqueue, queue );
	linequeue_destroy( queue );
}



/* Enable/disable ACE. Arguments: ROWSxCOLUMNS, or 'off'. */
static void command_ace( World *wld, char *cmd, char *args )
{
	int cols, rows;

	/* If /ace off, and ACE wasn't enabled, complain and be done. */
	if( !strcmp( args, "off" ) && !wld->ace_enabled )
	{
		world_msg_client( wld, "ACE is not enabled." );
		return;
	}

	/* If /ace off, and ACE was enabled, disable it and be done. */
	if( !strcmp( args, "off" ) && wld->ace_enabled )
	{
		world_msg_client( wld, "Disabled ACE." );
		world_disable_ace( wld );
		return;
	}

	/* If we have arguments, parse them. */
	if( *args )
	{
		if( sscanf( args, "%ix%i", &cols, &rows ) != 2 )
		{
			/* Parsing failed, complain and be done. */
			world_msg_client( wld, "Usage: ace <columns>x<rows>" );
			return;
		}
		/* Parsing succesful, the dimensions are in cols/rows. */
	}

	/* No arguments. */
	if( !*args )
	{
		if( !wld->ace_enabled )
		{
			/* ACE wasn't enabled, initialize to 80x24. */
			cols = 80;
			rows = 24;
		}
		else
		{
			/* Ace was already enabled, use the previous dims. */
			cols = wld->ace_cols;
			rows = wld->ace_rows;
		}
	}

	/* If the number of columns is too small or too large, complain. */
	if( cols < 20 || cols > 2000 )
	{
		world_msg_client( wld, "The number of columns should be "
				"between 20 and 2000." );
		return;
	}

	/* If the number of rows is too small or too large, complain. */
	if( rows < 10 || rows > 1000 )
	{
		world_msg_client( wld, "The number of rows should be "
				"between 10 and 1000." );
		return;
	}

	wld->ace_cols = cols;
	wld->ace_rows = rows;
	if( world_enable_ace( wld ) )
		world_msg_client( wld, "Enabled ACE for %ix%i terminal.",
			wld->ace_cols, wld->ace_rows );
	else
		world_msg_client( wld, "Failed to enable ACE." );
}



/* Print the version. No arguments. */
static void command_version( World *wld, char *cmd, char *args )
{
	if( refuse_arguments( wld, cmd, args ) )
		return;

	world_msg_client( wld, "Mooproxy version %s (released on %s).",
		VERSIONSTR, RELEASEDATE );
}



/* Print the current (full) date. No arguments. */
static void command_date( World *wld, char *cmd, char *args )
{
	if( refuse_arguments( wld, cmd, args ) )
		return;

	world_msg_client( wld, "The current date is %s.",
			time_fullstr( current_time() ) ); 
}



/* Print mooproxies starting date/time and uptime. No arguments. */
static void command_uptime( World *wld, char *cmd, char *args )
{
	time_t tme = uptime_started_at();
	long utme = (long) current_time() - tme;

	if( refuse_arguments( wld, cmd, args ) )
		return;

	world_msg_client( wld, "Started %s. Uptime is %li days, %.2li:%.2li:"
			"%.2li.", time_fullstr( tme ), utme / 86400,
			utme % 86400 / 3600, utme % 3600 / 60, utme % 60 );
}



/* Print the name of the current world. No arguments. */
static void command_world( World *wld, char *cmd, char *args )
{
	char *status;

	if( refuse_arguments( wld, cmd, args ) )
		return;

	switch( wld->server_status )
	{
		case ST_DISCONNECTED:
		status = "not connected";
		break;

		case ST_RESOLVING:
		status = "resolving hostname";
		break;

		case ST_CONNECTING:
		status = "attempting to connect";
		break;

		case ST_CONNECTED:
		status = "connected";
		break;

		case ST_RECONNECTWAIT:
		status = "waiting before reconnect";
		break;

		default:
		status = "unknown status";
		break;
	}

	world_msg_client( wld, "The world is %s (%s).", wld->name, status );
}



/* Clear all history lines from memory. No arguments. */
static void command_forget( World *wld, char *cmd, char *args )
{
	if( refuse_arguments( wld, cmd, args ) )
		return;

	world_inactive_to_history( wld );

	linequeue_clear( wld->history_lines );

	world_msg_client( wld, "All history lines have been forgotten." );
}



/* Print some authentication information. No arguments. */
static void command_authinfo( World *wld, char *cmd, char *args )
{
	Line *line;

	if( refuse_arguments( wld, cmd, args ) )
		return;

	/* Header. */
	world_msg_client( wld, "Authentication information:" );
	world_msg_client( wld, "" );

	/* Current connection. */
	world_msg_client( wld, "  Current connection from %s since %s.",
			wld->client_address,
			time_fullstr( wld->client_connected_since ) );
	/* Previous connection. */
	if( wld->client_prev_address )
		world_msg_client( wld, "  Previous connection from %s until "
				"%s.", wld->client_prev_address,
				time_fullstr( wld->client_last_connected ) );
	else
		world_msg_client( wld, "  No previous connection." );
	world_msg_client( wld, "" );

	/* Failed attempts. */
	world_msg_client( wld, "  %i failed login attempts since you logged "
			"in.", wld->client_login_failures );
	/* Last failed attempt. */
	if( wld->client_last_failaddr )
		world_msg_client( wld, "  Last failed from %s at %s.",
				wld->client_last_failaddr,
				time_fullstr( wld->client_last_failtime ) );
	world_msg_client( wld, "" );

	/* Privileged addresses. */
	world_msg_client( wld, "  Privileged addresses (%i/%i):",
			wld->auth_privaddrs->count, NET_MAX_PRIVADDRS );
	for( line = wld->auth_privaddrs->tail; line; line = line->prev)
		world_msg_client( wld, "    - %s", line->str );
	world_msg_client( wld, "" );

	/* Authentication token bucket. */
	world_msg_client( wld, "  Authentication token bucket is %i/%i full. "
			"Refill rate: %i/sec.", wld->auth_tokenbucket,
			NET_AUTH_BUCKETSIZE, NET_AUTH_TOKENSPERSEC );
}
