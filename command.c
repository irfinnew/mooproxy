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

#include "world.h"
#include "command.h"
#include "misc.h"
#include "network.h"
#include "config.h"



static int do_command( World *, char *, char * );
static int do_config( World *, char *, char * );

static void command_help( World *, char * );
static void command_quit( World *, char * );
static void command_shutdown( World *, char * );
static void command_connect( World *, char * );
static void command_disconnect( World *, char * );
static void command_options( World *, char * );
static void command_showopts( World *, char * );
static void command_pass( World *, char * );
static void command_block( World *, char * );
static void command_mcpreset( World *, char * );



static const struct
{
	char *command;
	void (*func)( World *, char * );
} command_db[] = {
	{ "help",		command_help },
	{ "quit",		command_quit },
	{ "shutdown",		command_shutdown },
	{ "connect",		command_connect },
	{ "disconnect",		command_disconnect },
	{ "options",		command_options },
	{ "showopts",		command_showopts },
	{ "pass",		command_pass },
	{ "block",		command_block },
	{ "mcpreset",		command_mcpreset },

	{ NULL,			NULL }
};



/* Returns 1 if the string begins with the command character(s) */
extern int world_is_command( World *wld, char *line )
{
	int i = 0;

	/* Does the line start with the command character(s)? */
	while( wld->commandstring[i] == line[i] && line[i] )
		i++;
	return !wld->commandstring[i];
}



/* Checks if the given string is a valid command for the given world.
 * If it's a valid command, it's executed (and output is sent to the client).
 * The return value is CMD_OK, CMD_NOCMD or CMD_INVCMD. */
extern int world_do_command( World *wld, char *line )
{
	int len, i = 0;
	char *cmd, c;

	/* Does the line start with the command character(s)? */
	while( wld->commandstring[i] == line[i] && line[i] )
		i++;
	if( wld->commandstring[i] )
		return CMD_NOCMD;

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
	line[i] = c;
	
	/* Try if it's a command */
	if( do_command( wld, cmd, line + i ) )
	{
		free( cmd );
		free( line );
		return CMD_OK;
	}

	/* Try if it's a config keyword */
	if( do_config( wld, cmd, line + i ) )
	{
		free( cmd );
		free( line );
		return CMD_OK;
	}

	/* Invalid blurb, inform client */
	free( line );
	asprintf( &line, "Invalid command: `%s'.", cmd );
	world_message_to_client( wld, line );

	free( cmd );
	free( line );
	return CMD_INVCMD;
}



static int do_command( World *wld, char *cmd, char *arg )
{
	int i;

	for( i = 0; command_db[i].command; i++ )
		if( !strcmp( cmd, command_db[i].command ) )
		{
			(*command_db[i].func)( wld, arg );
			return 1;
		}

	return 0;
}



static int do_config( World *wld, char *cmd, char *arg )
{
	char *line, *val;
	int i;

	i = world_get_key( wld, cmd, &val );

	if( i == 1 )
		return 0;

	if( i == 2 )
	{
		asprintf( &line, "%s cannot be read.", cmd );
		world_message_to_client( wld, line );
		free( line );
		return 1;
	}

	asprintf( &line, "%s is `%s'.", cmd, val );
	world_message_to_client( wld, line );
	free( line );
	free( val );
	return 1;
}



static void command_help( World *wld, char *args )
{
	int i, len = 0;
	char *line;

	/* Refuse if there are trailing characters */
	if( args[0] != '\0' )
	{
		world_message_to_client( wld, "Trailing characters." );
		return;
	}

	world_message_to_client( wld, "Commands:" );

	for( i = 0; command_db[i].command; i++ )
		len += strlen( command_db[i].command ) + 2;

	line = malloc( len + 8 );
	strcpy( line, "   " );

	for( i = 0; command_db[i].command; i++ )
	{
		strcat( line, command_db[i].command );
		if( command_db[i + 1].command )
			strcat( line, ", " );
	}
	strcat( line, "." );

	world_message_to_client( wld, line );
	free( line );
}



static void command_quit( World *wld, char *args )
{
	/* Refuse if there are trailing characters */
	if( args[0] != '\0' )
	{
		world_message_to_client( wld, "Trailing characters." );
		return;
	}

	world_message_to_client( wld, "Closing connection." );
	world_disconnect_client( wld );
	wld->flags |= WLD_FDCHANGE;
}



static void command_shutdown( World *wld, char *args )
{
	/* Refuse if there are trailing characters */
	if( args[0] != '\0' )
	{
		world_message_to_client( wld, "Trailing characters." );
		return;
	}

	world_message_to_client( wld, "Shutting down." );
	wld->flags |= WLD_QUIT;
}



static void command_connect( World *wld, char *args )
{
	int i;
	char *line, *err;

	/* Refuse if there are trailing characters */
	if( args[0] != '\0' )
	{
		world_message_to_client( wld, "Trailing characters." );
		return;
	}

	/* Are we already connected? */
	if( world_connected_to_server( wld ) )
	{
		world_message_to_client( wld, "Already connected." );
		return;
	}

	/* Try and resolve */
	world_message_to_client( wld, "Resolving host..." );
	world_flush_client_sbuf( wld );
	i = world_resolve_server( wld, &err );
	if( i != EXIT_OK )
	{
		world_message_to_client( wld, err );
		free( err );
		return;
	}
	/* Display that resolving succeeded */
	asprintf( &line, "`%s' resolved to %s", wld->host,
			wld->ipv4_address );
	world_message_to_client( wld, line );
	free( line );

	/* Try and connect */
	world_message_to_client( wld, "Connecting to host..." );
	world_flush_client_sbuf( wld );
	i = world_connect_server( wld, &err );
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
}



static void command_disconnect( World *wld, char *args )
{
	/* Refuse if there are trailing characters */
	if( args[0] != '\0' )
	{
		world_message_to_client( wld, "Trailing characters." );
		return;
	}

	/* See if we're connected at all. */
	if( world_connected_to_server( wld ) )
		world_message_to_client( wld, "Disconnected." );
	else
		world_message_to_client( wld, "Not connected.." );

	world_disconnect_server( wld );
}



static void command_options( World *wld, char *args )
{
	char *line, **list;
	int i, num;

	/* Refuse if there are trailing characters */
	if( args[0] != '\0' )
	{
		world_message_to_client( wld, "Trailing characters." );
		return;
	}

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



static void command_showopts( World *wld, char *args )
{
	char *str, *line, **list;
	int i, num, r;

	/* Refuse if there are trailing characters */
	if( args[0] != '\0' )
	{
		world_message_to_client( wld, "Trailing characters." );
		return;
	}

	world_message_to_client( wld, "Options:" );

	num = world_get_key_list( wld, &list );

	for( i = 0; i < num; i++ )
	{
		r = world_get_key( wld, list[i], &str );
		if( r )
		{
			free( list[i] );
			continue;
		}
		asprintf( &line, "  %s: %s", list[i], str );
		world_message_to_client( wld, line );
		free( line );
		free( list[i] );
		free( str );
	}

	free( list );
}



static void command_pass( World *wld, char *args )
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



static void command_block( World *wld, char *args )
{
	char *str;
	int on;

	str = trim_whitespace( strdup( args ) );
	on = true_or_false( str );
	free( str );

	switch( on )
	{
	case 0:
		wld->flags &= ~WLD_BLOCKSRV;
		world_message_to_client( wld, "Block is off." );
		break;
	case 1:
		wld->flags |= WLD_BLOCKSRV;
		world_message_to_client( wld, "Block is on." );
		break;
	default:
		asprintf( &str, "Block is %s.", wld->flags & WLD_BLOCKSRV ?
				"on" : "off" );
		world_message_to_client( wld, str );
		free( str );
		break;
	}
}



static void command_mcpreset( World *wld, char *args )
{
	char *str;

	/* Refuse if there are trailing characters */
	if( args[0] != '\0' )
	{
		world_message_to_client( wld, "Trailing characters." );
		return;
	}

	/* If the MCP negotiation has not taken place, do it now. */
	if( !wld->mcp_negotiated )
	{
		world_message_to_client( wld, "No MCP session, negotiating "
				"now." );
		str = strdup( "#$#mcp authentication-key: mehkey version: "
				"1.0 to: 2.1\n" );
		linequeue_append( wld->server_slines, line_create( str, -1 ) );
		str = strdup( "#$#mcp-negotiate-can mehkey package: "
				"dns-nl-icecrew-mcpreset min-version: 1.0 "
				"max-version: 1.0\n" );
		linequeue_append( wld->server_slines, line_create( str, -1 ) );
		str = strdup( "#$#mcp-negotiate-end mehkey\n" );
		linequeue_append( wld->server_slines, line_create( str, -1 ) );

		free( wld->mcp_key );
		wld->mcp_key = strdup( "mehkey" );
	}

	/* Send the MCP reset */
	world_message_to_client( wld, "Sending MCP reset." );
	asprintf( &str, "#$#dns-nl-icecrew-mcpreset-reset %s\n",wld->mcp_key );
	linequeue_append( wld->server_slines, line_create( str, -1 ) );

	free( wld->mcp_key );
	wld->mcp_key = NULL;
	wld->mcp_negotiated = 0;
}
