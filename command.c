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



static int do_command( World *, char *, char * );
static int refuse_arguments( World *, char *, char * );

static void command_help( World *, char *, char * );
static void command_quit( World *, char *, char * );
static void command_shutdown( World *, char *, char * );
static void command_connect( World *, char *, char * );
static void command_disconnect( World *, char *, char * );
static void command_listopts( World *, char *, char * );
static void command_getopt( World *, char *, char * );
static void command_setopt( World *, char *, char * );
static void command_pass( World *, char *, char * );
static void command_recall( World *, char *, char * );
static void command_version( World *, char *, char * );
static void command_date( World *, char *, char * );



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
"  pass [<on|off>]            Turn pass on/off, or show the number of lines.",
"  recall [<count>]           Recall <count> lines or show number of lines.",
"  version                    Show the mooproxy version.",
"  date                       Show the current time and date.",
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
	{ "pass",		command_pass },
	{ "recall",		command_recall },
	{ "version",		command_version },
	{ "date",		command_date },

	{ NULL,			NULL }
};



/* Returns 1 if the string begins with the command character(s) */
extern int world_is_command( World *wld, char *line )
{
	int i = 0;

	while( wld->commandstring[i] == line[i] && line[i] )
		i++;

	return !wld->commandstring[i];
}



/* Checks if the given string is a valid command for the given world.
 * If it's a valid command, it's executed (and output is sent to the client).
 * The return value is 1 for a succesful command, 0 for an invalid one. */
extern int world_do_command( World *wld, char *line )
{
	int len, i = 0;
	char *cmd, c;

	/* Does the line start with the command character(s)? */
	while( wld->commandstring[i] == line[i] && line[i] )
		i++;
	if( wld->commandstring[i] )
	{
		free( line );
		return 0;
	}

	/* Strip off command character(s) */
	len = i;
	for( i = 0; line[len + i - 1]; i++ )
		line[i] = line[len + i];

	/* Strip off trailing \n and \r */
	len = strlen( line );
	if( line[len - 1] == '\n' || line[len - 1] == '\r' )
		line[--len] = '\0';
	if( line[len - 1] == '\n' || line[len - 1] == '\r' )
		line[--len] = '\0';

	/* Determine where the command stops */
	for( i = 0; i < len; i++ )
		if( isspace( line[i] ) )
			break;

	/* Put the command in cmd, and remove it from line */
	c = line[i];
	line[i] = '\0';
	cmd = strdup( line );
	
	/* Try if it's a command */
	if( do_command( wld, cmd, line + i + 1 ) )
	{
		free( cmd );
		free( line );
		return 1;
	}

	/* Invalid blurb, inform client */
	free( line );
	asprintf( &line, "Invalid command: `%s'.", cmd );
	world_message_to_client( wld, line );

	free( cmd );
	free( line );
	return 0;
}



/* Tries to execute the given command. Returns 1 on succes, 0 on failure */
static int do_command( World *wld, char *cmd, char *args )
{
	int i;

	for( i = 0; command_db[i].command; i++ )
		if( !strcmp( cmd, command_db[i].command ) )
		{
			(*command_db[i].func)( wld, cmd, args );
			return 1;
		}

	return 0;
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
	world_disconnect_client( wld );
}



static void command_shutdown( World *wld, char *cmd, char *args )
{
	if( refuse_arguments( wld, cmd, args ) )
		return;

	world_message_to_client( wld, "Shutting down." );
	wld->flags |= WLD_QUIT;
}



static void command_connect( World *wld, char *cmd, char *args )
{
	int i;
	long backup_p = wld->port, port = wld->port;
	char *line, *err, *backup_h = wld->host, *tmp;

	/* Are we already connected? */
	if( world_connected_to_server( wld ) )
	{
		world_message_to_client( wld, "Already connected." );
		return;
	}

	/* If there is an argument, use it as hostname */
	if( ( tmp = get_one_word( &args ) ) )
		wld->host = tmp;

	/* If there's another argument, use that as the port */
	if( ( tmp = get_one_word( &args ) ) )
		port = atol( tmp );

	/* Try and resolve */
	asprintf( &line, "Resolving host `%s'...", wld->host );
	world_message_to_client( wld, line );
	free( line );
	world_flush_client_sbuf( wld );
	i = world_resolve_server( wld, &err );

	/* Restore the original hostname */
	wld->host = backup_h;

	if( i != EXIT_OK )
	{
		world_message_to_client( wld, err );
		free( err );
		return;
	}

	/* Try and connect. Set and restore the overriding port. */
	asprintf( &line, "Connecting to %s, port %li...",
			wld->ipv4_address, port );
	world_message_to_client( wld, line );
	free( line );
	world_flush_client_sbuf( wld );
	wld->port = port;
	i = world_connect_server( wld, &err );
	wld->port = backup_p;
	if( i != EXIT_OK )
	{
		world_message_to_client( wld, err );
		free( err );
		return;
	}

	/* We want pass on */
	wld->flags |= WLD_PASSTEXT;
	
	asprintf( &line, "Connected to world %s", wld->name );
	world_message_to_client( wld, line );
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
		world_message_to_client( wld, "Disconnected." );
	else
		world_message_to_client( wld, "Not connected." );

	world_disconnect_server( wld );
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
		case 0:
		asprintf( &line, "The option `%s' is `%s'.", args, val );
		free( val );
		break;

		case 1:
		asprintf( &line, "No such option, `%s'.", args );
		break;

		case 2:
		asprintf( &line, "The option `%s' cannot be read.", args );
		break;	
	}

	world_message_to_client( wld, line );
	free( line );
}



static void command_setopt( World *wld, char *cmd, char *args )
{
	char *line, *key, *val;

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

	switch( world_set_key( wld, key, val ) )
	{
		case 0:
		asprintf( &line, "The option `%s' is now `%s'.", key, val );
		break;

		case 1:
		asprintf( &line, "No such option, `%s'.", args );
		break;

		case 2:
		asprintf( &line, "The option `%s' cannot be written.", args );
		break;	
	}

	world_message_to_client( wld, line );
	free( line );
}



static void command_pass( World *wld, char *cmd, char *args )
{
	char *str;
	int on;

	str = trim_whitespace( strdup( args ) );
	on = true_or_false( str );
	free( str );

	switch( on )
	{
	case 0:
		wld->flags &= ~WLD_PASSTEXT;
		world_message_to_client( wld, "Pass is off." );
		break;
	case 1:
		wld->flags |= WLD_PASSTEXT;
		/* FIXME: could be better */
		if( wld->buffered_text->length == 0 )
		{
			world_message_to_client( wld,
					"Pass is on; no lines to pass." );
		}
		else
		{
			world_message_to_client( wld,
					"Pass is on; passing all lines." );
			world_pass_buffered_text( wld, -1 );
			world_message_to_client( wld, "End pass." );
		}
		break;
	default:
		asprintf( &str, "Pass is %s. %li waiting lines.", 
				wld->flags & WLD_PASSTEXT ? "on" : "off",
				wld->buffered_text->count );
		world_message_to_client( wld, str );
		free( str );
		break;
	}
}



static void command_recall( World *wld, char *cmd, char *args )
{
	int c = atoi( args ), i;
	char *str;
	Line *line, *nl;

	if( c == 0 )
	{
		asprintf( &str, "%li lines in history, using %li bytes.",
			wld->history_text->count, wld->history_text->length );
		world_message_to_client( wld, str );
		free( str );
		return;
	}

	asprintf( &str, "Recall start (%i lines).", c );
	world_message_to_client( wld, str );
	free( str );

	/* FIXME: Inefficient as hell */
	line = wld->history_text->lines;
	for( i = wld->history_text->count; i > c; i-- )
		line = line->next;

	for( ; line; line = line->next )
	{
		nl = line_create( strdup( line->str ), line->len );
		nl->store = 0;
		linequeue_append( wld->client_slines, nl );
	}

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
	char *str, *tme;
	time_t t;
	size_t len;

	if( refuse_arguments( wld, cmd, args ) )
		return;

	t = time( NULL );
	tme = strdup( ctime( &t ) );
	len = strlen( tme );

	/* Sigh. Whoever thought it was a good idea to let ctime() return
	 * a string with \n in it should be shot */
	if( len > 0 && tme[len - 1] == '\n' )
		tme[len - 1] = '\0';

	asprintf( &str, "The current date is %s.", tme );
	world_message_to_client( wld, str );

	free( str );
	free( tme );
}
