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

#include "global.h"
#include "world.h"
#include "misc.h"



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

	/* Destination */
	wld->dest_host = NULL;
	wld->dest_ipv4_address = NULL;
	wld->dest_port = -1;

	/* Listening connection */
	wld->listenport = -1;
	wld->listen_fd = -1;

	/* Authentication related stuff */
	wld->authstring = NULL;
	wld->auth_connections = 0;
	for( i = NET_MAXAUTHCONN - 1; i >= 0; i-- )
		wld->auth_buf[i] = malloc( NET_MAXAUTHLEN );

	/* Data related to the server connection */
	wld->server_fd = -1;
	
	wld->server_rxqueue = linequeue_create();
	wld->server_txqueue = linequeue_create();
	wld->server_rxbuffer = malloc( NET_BBUFFER_LEN );
	wld->server_rxfull = 0;
	wld->server_txbuffer = malloc( NET_BBUFFER_LEN );
	wld->server_txfull = 0;

	/* Data related to the client connection */
	wld->client_fd = -1;

	wld->client_rxqueue = linequeue_create();
	wld->client_txqueue = linequeue_create();
	wld->client_logqueue = linequeue_create();
	wld->client_buffered = linequeue_create();
	wld->client_history = linequeue_create();
	wld->client_rxbuffer = malloc( NET_BBUFFER_LEN );
	wld->client_rxfull = 0;
	wld->client_txbuffer = malloc( NET_BBUFFER_LEN );
	wld->client_txfull = 0;

	/* Miscellaneous */
	wld->buffer_dropped_lines = 0;

	/* Timer stuff */
	wld->timer_prev_sec = -1;
	wld->timer_prev_min = -1;
	wld->timer_prev_hour = -1;
	wld->timer_prev_day = -1;
	wld->timer_prev_mon = -1;
	wld->timer_prev_year = -1;

	/* Logging */
	wld->log_fd = -1;
	wld->logging_enabled = DEFAULT_LOGENABLE;

	/* MCP stuff */
	wld->mcp_negotiated = 0;
	wld->mcp_key = NULL;

	/* Options */
	wld->commandstring = strdup( DEFAULT_CMDSTRING );
	wld->infostring = strdup( DEFAULT_INFOSTRING );
	wld->context_on_connect = DEFAULT_CONTEXTLINES;
	wld->max_buffered_size = DEFAULT_MAXBUFFERED;
	wld->max_history_size = DEFAULT_MAXHISTORY;
	wld->strict_commands = DEFAULT_STRICTCMDS;

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

	/* Destination */
	free( wld->dest_host );
	free( wld->dest_ipv4_address );

	/* Authentication related stuff */
	free( wld->authstring );
	for( i = NET_MAXAUTHCONN - 1; i >= 0; i-- )
		free( wld->auth_buf[i] );

	/* Data related to server connection */
	linequeue_destroy( wld->server_rxqueue );
	linequeue_destroy( wld->server_txqueue );
	free( wld->server_rxbuffer );
	free( wld->server_txbuffer );

	/* Data related to client connection */
	linequeue_destroy( wld->client_rxqueue );
	linequeue_destroy( wld->client_txqueue );
	linequeue_destroy( wld->client_logqueue );
	linequeue_destroy( wld->client_buffered );
	linequeue_destroy( wld->client_history );
	free( wld->client_rxbuffer );
	free( wld->client_txbuffer );

	/* Options */
	free( wld->commandstring );
	free( wld->infostring );

	/* Miscellaneous */

	/* Logging */

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
	char *tmp;

	len = strlen( str ) + strlen( wld->infostring )
		+ sizeof( MESSAGE_TERMINATOR );
	tmp = malloc( len );
	strcpy( tmp, wld->infostring );
	strcat( tmp, str );
	strcat( tmp, MESSAGE_TERMINATOR );

	line = line_create( tmp, len - 1 );
	line->flags = LINE_MESSAGE;
	linequeue_append( wld->client_txqueue, line );
}



/* Queue a line for transmission to the client, and prefix the line with
 * the infostring, to indicate it's a message from mooproxy.
 * The line will not be free()d. */
extern void world_checkpoint_to_client( World *wld, char *str )
{
	Line *line;
	int len;
	char *tmp;

	len = strlen( str ) + strlen( wld->infostring )
		+ sizeof( MESSAGE_TERMINATOR );
	tmp = malloc( len );
	strcpy( tmp, wld->infostring );
	strcat( tmp, str );
	strcat( tmp, MESSAGE_TERMINATOR );

	line = line_create( tmp, len - 1 );
	line->flags = LINE_CHECKPOINT;
	linequeue_append( wld->client_txqueue, line );
}



/* Store the line in the history buffer. */
extern void world_store_history_line( World *wld, Line *line )
{
	if( line->flags & LINE_NOHIST )
	{
		line_destroy( line );
		return;
	}

	line->flags = LINE_HISTORY;
	linequeue_append( wld->client_history, line );
}



/* Measure the length of dynamic queues such as buffered and history,
 * and remove the oldest lines until they are small enought. */
extern void world_trim_dynamic_queues( World *wld )
{
	long limit;

	/* Trim history lines */
	limit = wld->max_history_size * 1024;
	while( wld->client_history->length > limit )
		line_destroy( linequeue_pop( wld->client_history ) );

	/* Trim bufferd lines */
	limit = wld->max_buffered_size * 1024;
	while( wld->client_buffered->length > limit )
	{
		line_destroy( linequeue_pop( wld->client_buffered ) );
		/* Buffered line dropped, take note! */
		wld->buffer_dropped_lines++;
	}
}



/* Replicate the set number of history lines to provide context for the newly
 * connected client, and then pass all buffered lines. */
extern void world_recall_and_pass( World *wld )
{
	char *str;
	long hlines = wld->client_history->count;
	long blines = wld->client_buffered->count;
	long dropped = wld->buffer_dropped_lines;
	double bfull;

	if( hlines > wld->context_on_connect )
		hlines = wld->context_on_connect;

	/* Calculate how full the buffer is. */
	bfull = wld->client_buffered->length / 10.24 / wld->max_buffered_size;

	/* If there's nothing to show, say so and be done. */
	if( hlines == 0 && blines == 0 )
	{
		world_message_to_client( wld, "No context lines and no "
			"buffered lines." );
		return;
	}

	/* Inform about the context lines, if any. */
	if( hlines > 0 )
	{
		asprintf( &str, "Passing %li line%s of context history.",
			hlines, hlines == 1 ? "" : "s" );
		world_message_to_client( wld, str );
		free( str );
	}

	/* Append context lines. */
	world_recall_history_lines( wld, hlines );

	/* Inform about end context and/or start buffer, as applicable. */
	if( blines > 0 )
	{
		asprintf( &str, "%s context lines. Passing %li buffered line%s"
			" (buffer %.1f%% full).", hlines > 0 ? "End" : "No",
			blines, blines == 1 ? "" : "s", bfull );
	} else {
		/* blines <= 0 AND hlines > 0 */
		asprintf( &str, "End context. No buffered lines." );
	}
	world_message_to_client( wld, str );
	free( str );

	/* If there were lines dropped, report that. */
	if( dropped > 0 )
	{
		asprintf( &str, "NOTE: %li buffered line%s dropped due to low "
			"buffer space.", dropped, dropped == 1 ? "" : "s" );
		world_message_to_client( wld, str );
		free( str );
		wld->buffer_dropped_lines = 0;
	}

	/* Append the buffered lines. */
	linequeue_merge( wld->client_txqueue, wld->client_buffered );

	/* Mark end of buffered text (if any). */
	if( blines > 0 )
		world_message_to_client( wld, "End pass." );
}



/* Recall a number of history lines. The second parameter is the
 * number of lines to recall. */
extern void world_recall_history_lines( World *wld, int lines )
{
	Line *line;
	int i;

	/* No history lines? Bail out. */
	if( wld->client_history->count == 0 || lines == 0 )
		return;

	/* Seek to correct line in the history queue. */
	line = wld->client_history->tail;
	for( i = lines; i > 1 && line->prev; i-- )
		line = line->prev;

	/* Duplicate lines, copying them to the txqueue. */
	for( ; line; line = line->next )
		linequeue_append( wld->client_txqueue, line_dup( line ) );
}
