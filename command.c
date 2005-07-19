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



#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

#include "world.h"
#include "command.h"
#include "misc.h"
#include "network.h"
#include "config.h"
#include "log.h"
#include "daemon.h"



static int command_index( char * );
static int refuse_arguments( World *, char *, char * );

static void command_help( World *, char *, char * );
static void command_quit( World *, char *, char * );
static void command_shutdown( World *, char *, char * );
static void command_connect( World *, char *, char * );
static void command_disconnect( World *, char *, char * );
static void command_listopts( World *, char *, char * );
static void command_getopt( World *, char *, char * );
static void command_setopt( World *, char *, char * );
static void command_recall( World *, char *, char * );
static void command_version( World *, char *, char * );
static void command_date( World *, char *, char * );
static void command_uptime( World *, char *, char * );
static void command_world( World *, char *, char * );



static char *help_text[] = {
"Commands:",
"  help                       Show this help message.",
"  quit                       Disconnect from mooproxy.",
"  shutdown                   Shut down the mooproxy.",
"  connect [<host> [<port>]]  Connect to the server. If the arguments are",
"                               given, use those instead of the set options.",
"  disconnect                 Disconnect from the server.",
"  listopts                   List the available option names.",
"  getopt <option>            Query the value of one option.",
"  setopt <option> <value>    Set the value of one option.",
"  recall [<count>]           Recall <count> lines or show number of lines.",
"  version                    Show the mooproxy version.",
"  date                       Show the current time and date.",
"  uptime                     Show mooproxy's starting time and uptime.",
"  world                      Print the name of the current world.",
NULL
};

static const struct
{
	char *command;
	void (*func)( World *, char *, char * );
} command_db[] = {
	{ "help",		command_help },
	{ "quit",		command_quit },
	{ "shutdown",		command_shutdown },
	{ "connect",		command_connect },
	{ "disconnect",		command_disconnect },
	{ "listopts",		command_listopts },
	{ "getopt",		command_getopt },
	{ "setopt",		command_setopt },
	{ "recall",		command_recall },
	{ "version",		command_version },
	{ "date",		command_date },
	{ "uptime",		command_uptime },
	{ "world",		command_world },

	{ NULL,			NULL }
};



/* Checks if the given string is a valid command for the given world.
 * If it's a valid command, it's executed (and output is sent to the client).
 * The return value is 1 for a succesful command, 0 for an invalid one.
 * If the command is succesful, the line is freed, otherwise not. */
extern int world_do_command( World *wld, char *line )
{
	char backup, *args, *cmd = line, *cstr = wld->commandstring;
	int idx, len;

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
	*args = '\0';

	/* Look op the command */
	idx = command_index( cmd );

	if( idx == -1 )
	{
		if( !wld->strict_commands )
		{
			/* Strictcmd is off, the line should now be
			 * processed like a regular one. */
			*args = backup;
			return 0;
		}

		/* Strict is on, complain */
		asprintf( &cstr, "Invalid command: `%s'.", cmd );
		world_message_to_client( wld, cstr );
		free( cstr );
		free( line );
		return 1;
	}

	/* Ok, it's a command */

	/* Advance args to the actual start of the arguments */
	if( backup )
		args++;

	/* Strip off trailing \n and \r */
	/* FIXME: Remove when newlines are done the proper way */
	len = strlen( args );
	if( args[len - 1] == '\n' || args[len - 1] == '\r' )
		args[--len] = '\0';
	if( args[len - 1] == '\n' || args[len - 1] == '\r' )
		args[--len] = '\0';

	/* Do the command! */
	(*command_db[idx].func)( wld, cmd, args );

	free( line );
	return 1;
}



/* Returns the index of the command in the db, or -1 if not found. */
static int command_index( char *cmd )
{
	int i;

	for( i = 0; command_db[i].command; i++ )
		if( !strcmp( cmd, command_db[i].command ) )
			return i;

	return -1;
}



/* If the args contains anything else than whitespace, complain to the
 * client and return 1. Otherwise, NOP and return 0. */
static int refuse_arguments( World *wld, char *cmd, char *args )
{
	char *str;

	for( ; *args; args++ )
		if( !isspace( *args ) )
		{
			asprintf( &str, "The command `%s' does not take "
					"arguments.", cmd );
			world_message_to_client( wld, str );
			free( str );
			return 1;
		}

	return 0;
}



static void command_help( World *wld, char *cmd, char *args )
{
	int i;

	if( refuse_arguments( wld, cmd, args ) )
		return;

	for( i = 0; help_text[i]; i++ )
		world_message_to_client( wld, help_text[i] );
}



static void command_quit( World *wld, char *cmd, char *args )
{
	if( refuse_arguments( wld, cmd, args ) )
		return;

	world_message_to_client( wld, "Closing connection." );
	wld->flags |= WLD_CLIENTQUIT;
}



static void command_shutdown( World *wld, char *cmd, char *args )
{
	if( refuse_arguments( wld, cmd, args ) )
		return;

	world_checkpoint_to_client( wld, "Shutting down." );
	wld->flags |= WLD_SHUTDOWN;
}



static void command_connect( World *wld, char *cmd, char *args )
{
	int i;
	long backup_p = wld->dest_port, port = wld->dest_port;
	char *line, *err, *backup_h = wld->dest_host, *tmp;

	/* Are we already connected? */
	if( world_connected_to_server( wld ) )
	{
		world_message_to_client( wld, "Already connected." );
		return;
	}

	/* If there is an argument, use it as hostname */
	if( ( tmp = get_one_word( &args ) ) )
		wld->dest_host = tmp;

	/* If there's another argument, use that as the port */
	if( ( tmp = get_one_word( &args ) ) )
		port = atol( tmp );

	/* Try and resolve */
	asprintf( &line, "Resolving host `%s'...", wld->dest_host );
	world_message_to_client( wld, line );
	free( line );
	/* FIXME: shouldn't be flushing this way */
	// world_log_toclient_queue( wld );
	// world_flush_client_txbuf( wld );
	i = world_resolve_server( wld, &err );

	/* Restore the original hostname */
	wld->dest_host = backup_h;

	if( i != EXIT_OK )
	{
		world_checkpoint_to_client( wld, err );
		free( err );
		return;
	}

	/* Try and connect. Set and restore the overriding port. */
	asprintf( &line, "Connecting to %s, port %li...",
			wld->dest_ipv4_address, port );
	world_message_to_client( wld, line );
	free( line );
	/* FIXME: shouldn't be flushing this way */
	// world_log_toclient_queue( wld );
	// world_flush_client_txbuf( wld );
	wld->dest_port = port;
	i = world_connect_server( wld, &err );
	wld->dest_port = backup_p;
	if( i != EXIT_OK )
	{
		world_message_to_client( wld, err );
		free( err );
		return;
	}

	asprintf( &line, "Connected to world %s", wld->name );
	world_checkpoint_to_client( wld, line );
	free( line );

	if( wld->log_fd == -1 )
		world_log_init( wld );

	wld->mcp_negotiated = 0;
}



static void command_disconnect( World *wld, char *cmd, char *args )
{
	if( refuse_arguments( wld, cmd, args ) )
		return;

	/* See if we're connected at all. */
	if( world_connected_to_server( wld ) )
		world_checkpoint_to_client( wld, "Disconnected." );
	else
		world_message_to_client( wld,
				"Not connected, so cannot disconnect." );

	wld->flags |= WLD_SERVERQUIT;
}



static void command_listopts( World *wld, char *cmd, char *args )
{
	char *line, **list;
	int i, num;

	if( refuse_arguments( wld, cmd, args ) )
		return;

	world_message_to_client( wld, "Options:" );

	line = strdup( "   " );
	num = world_get_key_list( wld, &list );

	for( i = 0; i < num; i++ )
	{
		if( strlen( line ) + strlen( list[i] ) > 65 )
		{
			world_message_to_client( wld, line );
			strcpy( line, "   " );
		}

		line = realloc( line, strlen( line ) + strlen( list[i] ) + 3 );
		line = strcat( line, list[i] );
		line = strcat( line, ", " );

		free( list[i] );
	}

	i = strlen( line );
	line[i - 2] = '.';
	line[i - 1] = '\0';

	world_message_to_client( wld, line );

	free( list );
	free( line );
}



static void command_getopt( World *wld, char *cmd, char *args )
{
	char *line, *val;

	args = trim_whitespace( args );

	if( !*args )
	{
		world_message_to_client( wld, "Use: getopt <option>" );
		return;
	}

	switch( world_get_key( wld, args, &val ) )
	{
		case GET_KEY_OK:
		asprintf( &line, "The option `%s' is `%s'.", args, val );
		free( val );
		break;

		case GET_KEY_NF:
		asprintf( &line, "No such option, `%s'.", args );
		break;

		case GET_KEY_PERM:
		asprintf( &line, "The option `%s' cannot be read.", args );
		break;	
	}

	world_message_to_client( wld, line );
	free( line );
}



static void command_setopt( World *wld, char *cmd, char *args )
{
	char *line, *key, *val, *err;

	key = args = trim_whitespace( args );

	while( *args && !isspace( *args ) )
		args++;

	if( !*args )
	{
		world_message_to_client( wld, "Use: setopt <option> <value>" );
		return;
	}

	*args++ = '\0';

	while( *args && isspace( *args ) )
		args++;

	val = remove_enclosing_quotes( args );

	switch( world_set_key( wld, key, val, &err ) )
	{
		/* FIXME: have a look at this */
		case SET_KEY_OK:
		err = NULL;
		if( world_get_key( wld, key, &err ) == GET_KEY_OK )
			val = err;
		asprintf( &line, "The option `%s' is now `%s'.", key, val );
		free( err );
		break;

		case SET_KEY_NF:
		asprintf( &line, "No such option, `%s'.", key );
		break;

		case SET_KEY_PERM:
		asprintf( &line, "The option `%s' cannot be written.", key );
		break;

		case SET_KEY_BAD:
		line = err;
		break;
	}

	world_message_to_client( wld, line );
	free( line );
}



static void command_recall( World *wld, char *cmd, char *args )
{
	int c = atoi( args );
	char *str;

	if( c == 0 )
	{
		asprintf( &str, "%li lines in history, using %li bytes.",
			wld->client_history->count, wld->client_history->length );
		world_message_to_client( wld, str );
		free( str );
		return;
	}

	asprintf( &str, "Recall start (%i lines).", c );
	world_message_to_client( wld, str );
	free( str );

	world_recall_history_lines( wld, c );

	world_message_to_client( wld, "Recall end." );
}



static void command_version( World *wld, char *cmd, char *args )
{
	if( refuse_arguments( wld, cmd, args ) )
		return;

	world_message_to_client( wld, "Mooproxy version " VERSIONSTR "." );
}



static void command_date( World *wld, char *cmd, char *args )
{
	char *str;

	if( refuse_arguments( wld, cmd, args ) )
		return;

	asprintf( &str, "The current date is %s.", time_string(
			time( NULL ), "%c" ) );
	world_message_to_client( wld, str );
	free( str );
}



static void command_uptime( World *wld, char *cmd, char *args )
{
	char *str;
	time_t tme = uptime_started_at();
	long utme = (long) time( NULL ) - tme;

	if( refuse_arguments( wld, cmd, args ) )
		return;

	asprintf( &str, "Started %s. Uptime is %li days, %.2li:%.2li:%.2li.",
			time_string( tme, "%c" ), utme / 86400,
			utme % 86400 / 3600, utme % 3600 / 60, utme % 60 );
	world_message_to_client( wld, str );
	free( str );
}



static void command_world( World *wld, char *cmd, char *args )
{
	char *str;

	if( refuse_arguments( wld, cmd, args ) )
		return;

	asprintf( &str, "The world is `%s'.", wld->name );
	world_message_to_client( wld, str );
	free( str );
}
