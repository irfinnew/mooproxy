/*
 *
 *  mooproxy - a buffering proxy for moo-connections
 *  Copyright (C) 2001-2005 Marcel L. Moreaux <marcelm@luon.net>
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
#include <string.h>
#include <limits.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>

#include "global.h"
#include "world.h"
#include "misc.h"
#include "line.h"



extern World *world_create( char *wldname )
{
	int i;
	World *wld;

	wld = xmalloc( sizeof( World ) );

	/* Essentials */
	wld->name = wldname;
	wld->configfile = NULL;
	wld->flags = 0;

	/* Destination */
	wld->dest_host = NULL;
	wld->dest_port = -1;

	/* Listening connection */
	wld->listenport = -1;
	wld->listen_fds = NULL;

	/* Authentication related stuff */
	wld->auth_md5hash = NULL;
	wld->auth_literal = NULL;
	wld->auth_connections = 0;
	for( i = NET_MAXAUTHCONN - 1; i >= 0; i-- )
	{
		wld->auth_buf[i] = xmalloc( NET_MAXAUTHLEN );
		wld->auth_address[i] = NULL;
	}

	/* Data related to the server connection */
	wld->server_status = ST_DISCONNECTED;
	wld->server_fd = -1;
	wld->server_port = NULL;
	wld->server_host = NULL;
	wld->server_address = NULL;

	wld->server_resolver_pid = -1;
	wld->server_resolver_fd = -1;
	wld->server_addresslist = NULL;
	wld->server_connecting_fd = -1;

	wld->server_rxqueue = linequeue_create();
	wld->server_toqueue = linequeue_create();
	wld->server_txqueue = linequeue_create();
	wld->server_rxbuffer = xmalloc( NET_BBUFFER_LEN );
	wld->server_rxfull = 0;
	wld->server_txbuffer = xmalloc( NET_BBUFFER_LEN + 4 ); /* See (1) */
	wld->server_txfull = 0;

	/* Data related to the client connection */
	wld->client_status = ST_DISCONNECTED;
	wld->client_fd = -1;
	wld->client_address = NULL;
	wld->client_prev_address = NULL;
	wld->client_last_connected = 0;

	wld->client_rxqueue = linequeue_create();
	wld->client_toqueue = linequeue_create();
	wld->client_txqueue = linequeue_create();
	wld->client_rxbuffer = xmalloc( NET_BBUFFER_LEN );
	wld->client_rxfull = 0;
	wld->client_txbuffer = xmalloc( NET_BBUFFER_LEN + 4 ); /* See (1) */
	wld->client_txfull = 0;

	/* Miscellaneous */
	wld->buffered_lines = linequeue_create();
	wld->history_lines = linequeue_create();
	wld->buffer_dropped_lines = 0;

	/* Timer stuff */
	wld->timer_prev_sec = -1;
	wld->timer_prev_min = -1;
	wld->timer_prev_hour = -1;
	wld->timer_prev_day = -1;
	wld->timer_prev_mon = -1;
	wld->timer_prev_year = -1;

	/* Logging */
	wld->log_queue = linequeue_create();
	wld->log_current = linequeue_create();
	wld->log_currentday = 0;
	wld->log_fd = -1;
	wld->log_buffer = xmalloc( NET_BBUFFER_LEN + 4 ); /* See (1) */
	wld->log_bfull = 0;
	wld->log_lasterror = xstrdup( "" );
	wld->log_lasterrtime = 0;

	/* MCP stuff */
	wld->mcp_negotiated = 0;
	wld->mcp_key = NULL;

	/* Options */
	wld->logging_enabled = DEFAULT_LOGENABLE;
	wld->autologin = DEFAULT_AUTOLOGIN;
	wld->commandstring = xstrdup( DEFAULT_CMDSTRING );
	wld->infostring = xstrdup( DEFAULT_INFOSTRING );
	wld->context_on_connect = DEFAULT_CONTEXTLINES;
	wld->max_buffered_size = DEFAULT_MAXBUFFERED;
	wld->max_history_size = DEFAULT_MAXHISTORY;
	wld->strict_commands = DEFAULT_STRICTCMDS;

	return wld;

	/* (1) comment for allocations of several buffers:
	 * 
	 * The network TX buffers and the log write buffer must be a little
	 * longer than the RX buffers, so that mooproxy can append a newline
	 * (\n or \r\n) to the line written into the buffer, even if the
	 * line is as long as the (RX) buffer itself.
	 * 4 bytes is a bit much perhaps, but oh well. */
}



extern void world_destroy( World *wld )
{
	int i;

	/* Essentials */
	free( wld->name );
	free( wld->configfile );

	/* Destination */
	free( wld->dest_host );

	/* Listening connection */
	free( wld->listen_fds );

	/* Authentication related stuff */
	free( wld->auth_md5hash );
	free( wld->auth_literal );
	for( i = NET_MAXAUTHCONN - 1; i >= 0; i-- )
	{
		free( wld->auth_buf[i] );
		free( wld->auth_address[i] );
		if( wld->auth_fd[i] > -1 )
			close( wld->auth_fd[i] );
	}

	/* Data related to server connection */
	if( wld->server_fd > -1 )
		close( wld->server_fd );
	free( wld->server_host );
	free( wld->server_port );
	free( wld->server_address );

	if( wld->server_resolver_fd > -1 )
		close( wld->server_resolver_fd );
	free( wld->server_addresslist );
	if( wld->server_connecting_fd > -1 )
		close( wld->server_connecting_fd );

	linequeue_destroy( wld->server_rxqueue );
	linequeue_destroy( wld->server_toqueue );
	linequeue_destroy( wld->server_txqueue );
	free( wld->server_rxbuffer );
	free( wld->server_txbuffer );

	/* Data related to client connection */
	if( wld->client_fd > -1 )
		close( wld->client_fd );
	free( wld->client_address );
	free( wld->client_prev_address );

	linequeue_destroy( wld->client_rxqueue );
	linequeue_destroy( wld->client_toqueue );
	linequeue_destroy( wld->client_txqueue );
	free( wld->client_rxbuffer );
	free( wld->client_txbuffer );

	/* Miscellaneous */
	linequeue_destroy( wld->buffered_lines );
	linequeue_destroy( wld->history_lines );

	/* Logging */
	linequeue_destroy( wld->log_queue );
	linequeue_destroy( wld->log_current );
	if( wld->log_fd > -1 )
		close( wld->log_fd );
	free( wld->log_buffer );
	free( wld->log_lasterror );

	/* MCP stuff */
	free( wld->mcp_key );

	/* Options */
	free( wld->commandstring );
	free( wld->infostring );

	/* The world itself */
	free( wld );
}



extern void world_configfile_from_name( World *wld )
{
	char *homedir = get_homedir();

	wld->configfile = xmalloc( strlen( homedir ) + 
			strlen( wld->name ) + strlen( WORLDSDIR ) + 1 );

	/* Configuration file = homedir + WORLDSDIR + world name */
	strcpy( wld->configfile, homedir );
	strcat( wld->configfile, WORLDSDIR );
	strcat( wld->configfile, wld->name );

	free( homedir );
}



extern Line *world_msg_client( World *wld, const char *fmt, ... )
{
	va_list argp;
	Line *line;
	char *str, *msg;

	/* Parse the arguments into a string */
	va_start( argp, fmt );
	xvasprintf( &str, fmt, argp );
	va_end( argp );

	/* Construct the message */
	msg = xmalloc( strlen( str ) + strlen( wld->infostring )
		+ strlen( MESSAGE_TERMINATOR ) + 1 );
	strcpy( msg, wld->infostring );
	strcat( msg, str );
	strcat( msg, MESSAGE_TERMINATOR );
	free( str );

	/* And append it to the client queue */
	line = line_create( msg, -1 );
	line->flags = LINE_MESSAGE;
	linequeue_append( wld->client_toqueue, line );

	return line;
}



extern void world_trim_dynamic_queues( World *wld )
{
	unsigned long limit;

	/* Trim history lines */
	limit = wld->max_history_size * 1024;
	while( wld->history_lines->length > limit )
		line_destroy( linequeue_pop( wld->history_lines ) );

	/* Trim bufferd lines */
	limit = wld->max_buffered_size * 1024;
	while( wld->buffered_lines->length > limit )
	{
		line_destroy( linequeue_pop( wld->buffered_lines ) );
		/* Buffered line dropped, take note! */
		wld->buffer_dropped_lines++;
	}
}



extern void world_recall_and_pass( World *wld )
{
	long hlines = wld->history_lines->count;
	long blines = wld->buffered_lines->count;
	long dropped = wld->buffer_dropped_lines;
	double bfull;

	/* Pass min(wld->history_lines->count, wld->context_on_connect) lines */
	if( hlines > wld->context_on_connect )
		hlines = wld->context_on_connect;

	/* Calculate how full the buffer is. */
	bfull = wld->buffered_lines->length / 10.24 / wld->max_buffered_size;

	/* If there's nothing to show, say so and be done. */
	if( hlines == 0 && blines == 0 )
	{
		world_msg_client( wld, "No context lines and no "
				"buffered lines." );
		return;
	}

	/* Inform about the context lines, if any. */
	if( hlines > 0 )
		world_msg_client( wld, "Passing %li line%s of context history.",
				hlines, hlines == 1 ? "" : "s" );

	/* Append context lines. */
	world_recall_history_lines( wld, hlines );

	/* Inform about end context and/or start buffer, as applicable. */
	if( blines > 0 )
		world_msg_client( wld, "%s context lines. Passing %li buffered"
			" line%s (buffer %.1f%% full).",
			hlines > 0 ? "End" : "No", blines,
			blines == 1 ? "" : "s", bfull );
	else
		world_msg_client( wld, "End context. No buffered lines." );

	/* If there were lines dropped, report that. */
	if( dropped > 0 )
		world_msg_client( wld, "WARNING: %li buffered line%s dropped "
				"due to low buffer space.", dropped,
				dropped == 1 ? "" : "s" );
	wld->buffer_dropped_lines = 0;

	/* Append the buffered lines. */
	linequeue_merge( wld->client_toqueue, wld->buffered_lines );

	/* Mark end of buffered text (if any). */
	if( blines > 0 )
		world_msg_client( wld, "End of buffered lines." );
}



extern void world_recall_history_lines( World *wld, int lines )
{
	Line *line, *recalled;
	int i;

	/* No history lines? Bail out. */
	if( wld->history_lines->count == 0 || lines == 0 )
		return;

	/* Seek to correct line in the history queue. */
	line = wld->history_lines->tail;
	for( i = lines; i > 1 && line->prev; i-- )
		line = line->prev;

	/* Duplicate lines, copying them to the txqueue. */
	for( ; line; line = line->next )
	{
		recalled = line_dup( line );
		/* The recalled copy shouldnt go into the history again. */
		recalled->flags |= LINE_NOHIST;
		linequeue_append( wld->client_toqueue, recalled );
	}
}



extern void world_login_server( World *wld, int override )
{
	/* Only log in if autologin is enabled or override is in effect */
	if( !wld->autologin && !override )
		return;

	/* Of course, we need to be connected */
	if( wld->server_status != ST_CONNECTED )
	{
		world_msg_client( wld, "Not connected to server, "
				"cannot login." );
		return;
	}

	/* We can't login without wld->auth_literal */
	if( wld->auth_literal == NULL )
	{
		world_msg_client( wld, "Cannot login to server, literal "
				"authentication string not known." );
		world_msg_client( wld, "You need to re-authenticate to moo"
				"proxy (i.e. reconnect) for this to work." );
		return;
	}

	if( override )
		world_msg_client( wld, "Logging in on world %s.", wld->name );
	else
		world_msg_client( wld, "Automatically logging in on world %s.",
				wld->name );

	linequeue_append( wld->server_toqueue,
			line_create( xstrdup( wld->auth_literal ), -1 ) );
}
