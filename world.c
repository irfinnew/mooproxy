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
#include <string.h>
#include <limits.h>
#include <time.h>

#include "world.h"
#include "global.h"
#include "misc.h"
#include "network.h"
#include "command.h"
#include "mcp.h"
#include "log.h"



/* This function creates a world by allocating memory and initialising
 * some variables. */
World* world_create( char *wldname )
{
	int i;
	World *wld;

	wld = malloc( sizeof( World ) );

	/* Essentials */
	wld->name = wldname;
	wld->configfile = NULL;
	wld->flags = 0;

	/* Listening connection */
	wld->listenport = -1;
	wld->listen_fd = -1;

	/* Authentication related stuff */
	wld->authstring = NULL;
	wld->auth_connections = 0;
	for( i = NET_MAXAUTHCONN - 1; i >= 0; i-- )
		wld->auth_buf[i] = malloc( 1024 );

	/* Data related to the server connection */
	wld->host = NULL;
	wld->ipv4_address = NULL;
	wld->port = -1;
	wld->server_fd = -1;
	
	wld->server_rlines = linequeue_create();
	wld->server_slines = linequeue_create();
	wld->server_rbuffer = malloc( NET_BBUFFER_LEN );
	wld->server_rbfull = 0;
	wld->server_sbuffer = malloc( NET_BBUFFER_LEN );
	wld->server_sbfull = 0;

	/* Data related to the client connection */
	wld->client_fd = -1;

	wld->client_rlines = linequeue_create();
	wld->client_slines = linequeue_create();
	wld->buffered_text = linequeue_create();
	wld->history_text = linequeue_create();
	wld->client_rbuffer = malloc( NET_BBUFFER_LEN );
	wld->client_rbfull = 0;
	wld->client_sbuffer = malloc( NET_BBUFFER_LEN );
	wld->client_sbfull = 0;

	/* Miscellaneous */
	wld->log_fd = -1;

	/* MCP stuff */
	wld->mcp_negotiated = 0;
	wld->mcp_key = NULL;

	/* Options */
	wld->commandstring = strdup( DEFAULT_CMDSTRING );
	wld->infostring = strdup( DEFAULT_INFOSTRING );

	return wld;
}



/* This function de-initialises a world and frees the allocated memory.
 * The world must be disconnected before destroying it */
void world_destroy( World *wld )
{
	int i;

	/* Essentials */
	free( wld->name );
	free( wld->configfile );

	/* Authentication related stuff */
	free( wld->authstring );
	for( i = NET_MAXAUTHCONN - 1; i >= 0; i-- )
		free( wld->auth_buf[i] );

	/* Data related to server connection */
	free( wld->host );
	free( wld->ipv4_address );
	linequeue_destroy( wld->server_rlines );
	linequeue_destroy( wld->server_slines );
	free( wld->server_rbuffer );
	free( wld->server_sbuffer );

	/* Data related to client connection */
	linequeue_destroy( wld->client_rlines );
	linequeue_destroy( wld->client_slines );
	linequeue_destroy( wld->buffered_text );
	linequeue_destroy( wld->history_text );
	free( wld->client_rbuffer );
	free( wld->client_sbuffer );

	/* Options */
	free( wld->commandstring );
	free( wld->infostring );

	/* Miscellaneous */

	/* MCP stuff */
	free( wld->mcp_key );

	/* The world itself */
	free( wld );
}



/* This function takes a worldname as argument and returns the filename
 * The string is strdup()ped, so it can be free()d. */
void world_configfile_from_name( World *wld )
{
	char *homedir = get_homedir();
	
	wld->configfile = malloc( strlen( homedir ) + 
			strlen( wld->name ) + sizeof( WORLDSDIR ) + 1 );

	strcpy( wld->configfile, homedir );
	strcat( wld->configfile, WORLDSDIR );
	strcat( wld->configfile, wld->name );

	free( homedir );
}



/* Queue a line for transmission to the client, and prefix the line with
 * the infostring, to indicate it's a message from mooproxy.
 * The line will not be free()d. */
extern void world_message_to_client( World *wld, char *str )
{
	Line *line;
	int len;

	/* FIXME: Use line_create() */
	line = malloc( sizeof( Line ) );

	len = strlen( str ) + strlen( wld->infostring )
		+ sizeof( MESSAGE_TERMINATOR );
	line->str = malloc( len );
	line->len = len - 1;
	line->store = 0;

	strcpy( line->str, wld->infostring );
	strcat( line->str, str );
	strcat( line->str, MESSAGE_TERMINATOR );

	linequeue_append( wld->client_slines, line );
}



/* Exactly like world_message_to_client, but pass it through the buffer
 * regular server->client lines go through. */
extern void world_message_to_client_buf( World *wld, char *str )
{
	Line *line;
	int len;

	/* FIXME: Use line_create() */
	line = malloc( sizeof( Line ) );

	len = strlen( str ) + strlen( wld->infostring )
		+ sizeof( MESSAGE_TERMINATOR );
	line->str = malloc( len );
	line->len = len - 1;
	line->store = 1;

	strcpy( line->str, wld->infostring );
	strcat( line->str, str );
	strcat( line->str, MESSAGE_TERMINATOR );

	world_log_server_line( wld, line );
	linequeue_append( wld->buffered_text, line );
}



/* Handle all the queued lines from the client.
 * Handling includes commands, MCP, logging, requeueing, etc. */
extern void world_handle_client_queue( World *wld )
{
	Line *line;

	while( ( line = linequeue_pop( wld->client_rlines ) ) )
		if( world_is_command( wld, line->str ) )
		{
			world_do_command( wld, line->str );
			free( line );
		}
		else if( world_is_mcp( line->str ) )
		{
			world_do_mcp_client( wld, line );
		}				
		else
		{
			linequeue_append( wld->server_slines, line );
		}
}



/* Handle all the queued lines from the server.
 * Handling includes commands, MCP, logging, requeueing, etc. */
extern void world_handle_server_queue( World *wld )
{
	Line *line;

	while( ( line = linequeue_pop( wld->server_rlines ) ) )
		if( world_is_mcp( line->str ) )
			world_do_mcp_server( wld, line );
		else
		{
			line->store = 1;
			world_log_server_line( wld, line );
			linequeue_append( wld->buffered_text, line );
		}
}



/* Pass the lines from buffered_text to the client send buffer.
 * The long is the number of lines to pass, -1 for all. */
extern void world_pass_buffered_text( World *wld, long num )
{
	Line *line;

	if( num == -1 )
		num = LONG_MAX;

	while( num-- > 0 && ( line = linequeue_pop( wld->buffered_text ) ) )
		linequeue_append( wld->client_slines, line );
}



/* Store the line in the history buffer, pushing out lines on the
 * other end if space runs tight. */
extern void world_store_history_line( World *wld, Line *line )
{
	Line *l;

	/* If the line shouldn't be stored, discard it */
	if( !line->store )
	{
		free( line->str );
		free( line );
		return;
	}

	linequeue_append( wld->history_text, line );

	/* FIXME: dynamic limit */
	while( wld->history_text->count > 1024 )
	{
		l = linequeue_pop( wld->history_text );
		free( l->str );
		free( l );
	}	
}



/* Should be called approx. once each second, with the current time
 * as argument. Will handle periodic / scheduled events. */
extern void world_timer_tick( World *wld, time_t t )
{
	static int previous = -1;
	struct tm *tms;

	tms = localtime( &t );
	if( tms->tm_mday == previous )
		return;

	previous = tms->tm_mday;
	world_log_init( wld );
	world_message_to_client_buf( wld, time_string( t, "Day changed to %A %d %b %Y." ) );
}
