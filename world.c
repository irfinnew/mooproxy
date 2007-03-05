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
#include <string.h>
#include <limits.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>

#include "global.h"
#include "world.h"
#include "misc.h"
#include "line.h"
#include "panic.h"



static int worlds_count = 0;
static World **worlds_list = NULL;



static void register_world( World *wld );
static void unregister_world( World *wld );



extern World *world_create( char *wldname )
{
	int i;
	World *wld;

	wld = xmalloc( sizeof( World ) );

	/* Essentials */
	wld->name = wldname;
	wld->configfile = NULL;
	wld->flags = 0;
	wld->lockfile = NULL;
	wld->lockfile_fd = -1;

	/* Destination */
	wld->dest_host = NULL;
	wld->dest_port = -1;

	/* Listening connection */
	wld->listenport = -1;
	wld->listen_fds = NULL;
	wld->bindresult = xmalloc( sizeof( BindResult ) );
	world_bindresult_init( wld->bindresult );

	/* Authentication related stuff */
	wld->auth_md5hash = NULL;
	wld->auth_literal = NULL;
	wld->auth_connections = 0;
	for( i = NET_MAXAUTHCONN - 1; i >= 0; i-- )
	{
		wld->auth_buf[i] = xmalloc( NET_MAXAUTHLEN );
		wld->auth_address[i] = NULL;
		wld->auth_fd[i] = -1;
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
	wld->server_rxbuffer = xmalloc( NET_BBUFFER_LEN + 4 ); /* See (1) */
	wld->server_rxfull = 0;
	wld->server_txbuffer = xmalloc( NET_BBUFFER_LEN + 4 ); /* See (1) */
	wld->server_txfull = 0;

	/* Data related to the client connection */
	wld->client_status = ST_DISCONNECTED;
	wld->client_fd = -1;
	wld->client_address = NULL;
	wld->client_prev_address = NULL;
	wld->client_last_connected = 0;
	wld->client_last_notconnmsg = 0;

	wld->client_rxqueue = linequeue_create();
	wld->client_toqueue = linequeue_create();
	wld->client_txqueue = linequeue_create();
	wld->client_rxbuffer = xmalloc( NET_BBUFFER_LEN + 4 ); /* See (1) */
	wld->client_rxfull = 0;
	wld->client_txbuffer = xmalloc( NET_BBUFFER_LEN + 4 ); /* See (1) */
	wld->client_txfull = 0;

	/* Miscellaneous */
	wld->buffered_lines = linequeue_create();
	wld->inactive_lines = linequeue_create();
	wld->history_lines = linequeue_create();
	wld->dropped_inactive_lines = 0;
	wld->dropped_buffered_lines = 0;

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
	wld->dropped_loggable_lines = 0;
	wld->log_currentday = 0;
	wld->log_currenttime = 0;
	wld->log_currenttimestr = NULL;
	wld->log_fd = -1;
	wld->log_buffer = xmalloc( NET_BBUFFER_LEN + 4 ); /* See (1) */
	wld->log_bfull = 0;
	wld->log_lasterror = NULL;
	wld->log_lasterrtime = 0;

	/* MCP stuff */
	wld->mcp_negotiated = 0;
	wld->mcp_key = NULL;
	wld->mcp_initmsg = NULL;

	/* Options */
	wld->logging_enabled = DEFAULT_LOGENABLE;
	wld->autologin = DEFAULT_AUTOLOGIN;
	wld->commandstring = xstrdup( DEFAULT_CMDSTRING );
	wld->infostring = xstrdup( DEFAULT_INFOSTRING );
	wld->infostring_parsed = parse_ansi_tags( wld->infostring );
	wld->context_on_connect = DEFAULT_CONTEXTLINES;
	wld->max_buffer_size = DEFAULT_MAXBUFFERSIZE;
	wld->max_logbuffer_size = DEFAULT_MAXLOGBUFFERSIZE;
	wld->strict_commands = DEFAULT_STRICTCMDS;
	wld->log_timestamps = DEFAULT_LOGTIMESTAMPS;

	/* Add to the list of worlds */
	register_world( wld );

	return wld;

	/* (1) comment for allocations of several buffers:
	 * 
	 * The TX and log buffers must be a little larger than specified, so
	 * that flush_buffer() can append a newline (\n or \r\n) to the line
	 * written in the buffer, even if the buffer is completely filled with
	 * lines.
	 *
	 * The RX buffers must be larger so that buffer_to_lines() can append a
	 * sentinal for its linear search.
	 */
}



extern void world_destroy( World *wld )
{
	int i;

	/* Remove the world from the worldlist. */
	unregister_world( wld );

	/* Essentials */
	free( wld->name );
	free( wld->configfile );
	free( wld->lockfile );

	/* Destination */
	free( wld->dest_host );

	/* Listening connection */
	free( wld->listen_fds );
	world_bindresult_free( wld->bindresult );
	free( wld->bindresult );

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
	linequeue_destroy( wld->inactive_lines );
	linequeue_destroy( wld->history_lines );

	/* Logging */
	linequeue_destroy( wld->log_queue );
	linequeue_destroy( wld->log_current );
	free( wld->log_currenttimestr );
	if( wld->log_fd > -1 )
		close( wld->log_fd );
	free( wld->log_buffer );
	free( wld->log_lasterror );

	/* MCP stuff */
	free( wld->mcp_key );
	free( wld->mcp_initmsg );

	/* Options */
	free( wld->commandstring );
	free( wld->infostring );
	free( wld->infostring_parsed );

	/* The world itself */
	free( wld );
}



extern void world_get_list( int *count, World ***wldlist )
{
	*count = worlds_count;
	*wldlist = worlds_list;
}



/* Add a world to the global list of worlds. */
static void register_world( World *wld )
{
	worlds_list = xrealloc( worlds_list,
			( worlds_count + 1 ) * sizeof( World * ) );

	worlds_list[worlds_count] = wld;
	worlds_count++;
}



/* Remove a world from the global list of worlds. */
static void unregister_world( World *wld )
{
	int i = 0;

	/* Search for the world in the list */
	while( i < worlds_count && worlds_list[i] != wld )
		i++;

	/* Is the world not in the list? */
	if( i == worlds_count )
		/* FIXME: this shouldn't happen. Report? */
		return;

	/* At this point, worlds_list[i] is our world */
	for( i++; i < worlds_count; i++ )
		worlds_list[i - 1] = worlds_list[i];

	worlds_list = xrealloc( worlds_list,
			( worlds_count - 1 ) * sizeof( World * ) );
	worlds_count--;
}



extern void world_bindresult_init( BindResult *bindresult )
{
	bindresult->fatal = NULL;
	bindresult->af_count = 0;
	bindresult->af_success_count = 0;
	bindresult->af_success = NULL;
	bindresult->af_msg = NULL;
	bindresult->conclusion = NULL;
}



extern void world_bindresult_free( BindResult *bindresult )
{
	int i;

	free( bindresult->fatal );
	free( bindresult->af_success );
	for( i = 0; i < bindresult->af_count; i++ )
		free( bindresult->af_msg[i] );
	free( bindresult->af_msg );
	free( bindresult->conclusion );
}



extern void world_configfile_from_name( World *wld )
{
	/* Configuration file = ~/CONFIGDIR/WORLDSDIR/worldname */
	xasprintf( &wld->configfile, "%s/%s/%s/%s", get_homedir(), CONFIGDIR,
			WORLDSDIR, wld->name );
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
	msg = xmalloc( strlen( str ) + strlen( wld->infostring_parsed )
		+ sizeof( MESSAGE_TERMINATOR ) );
	strcpy( msg, wld->infostring_parsed );
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
	unsigned long *bll = &( wld->buffered_lines->length );
	unsigned long *ill = &( wld->inactive_lines->length );
	unsigned long *hll = &( wld->history_lines->length );
	unsigned long limit;

	/* Trim the normal buffers. The data is distributed over
	 * buffered_lines, inactive_lines and history_lines. We will trim
	 * them in reverse order until everything is small enough. */
	limit = wld->max_buffer_size * 1024;

	/* First, the history lines. Remove oldest lines. */
	while( *bll + *ill + *hll > limit && wld->history_lines->head != NULL )
		line_destroy( linequeue_pop( wld->history_lines ) );

	/* Next, the inactive lines. These are important, so we count
	 * the number of dropped lines. Remove oldest lines. */
	while( *bll + *ill > limit && wld->inactive_lines->head != NULL )
	{
		line_destroy( linequeue_pop( wld->inactive_lines ) );
		wld->dropped_inactive_lines++;
	}

	/* Finally, the buffered lines. These are important, so we count
	 * the number of dropped lines. Remove oldest lines. */
	while( *bll > limit && wld->buffered_lines->head != NULL )
	{
		line_destroy( linequeue_pop( wld->buffered_lines ) );
		wld->dropped_buffered_lines++;
	}

	/* Trim the logbuffers. The data is distributed over log_current and
	 * log_queue. We will trim them in reverse order until everything is
	 * small enough. */
	limit = wld->max_logbuffer_size * 1024;

	/* First log_queue. These are very important, so we count the number
	 * of dropped lines. Remove newest lines. */
	while( wld->log_queue->length + wld->log_current->length > limit &&
			wld->log_queue->head != NULL )
	{
		line_destroy( linequeue_popend( wld->log_queue ) );
		wld->dropped_loggable_lines++;
	}

	/* Next, log_current. These are very important, so we count the number
	 * of dropped lines. Remove newest lines. */
	while( wld->log_current->length > limit &&
			wld->log_current->head != NULL )
	{
		line_destroy( linequeue_popend( wld->log_current ) );
		wld->dropped_loggable_lines++;
	}
}



extern void world_recall_and_pass( World *wld )
{
	Linequeue *il = wld->inactive_lines;
	Linequeue *bl = wld->buffered_lines;
	Line *line, *recalled, *contextstart = NULL;
	long contextcount = 0;
	unsigned long ilcount = il->count, blcount = bl->count;

	if( wld->context_on_connect > 0 )
	{
		/* First, seek back from the end of the history, to the first
		 * line we should display. */
		line = wld->history_lines->tail;
		while( line != NULL && contextcount < wld->context_on_connect )
		{
			contextcount++;
			contextstart = line;
			line = line->prev;
		}

		/* Inform the client of the context lines */
		if( contextcount > 0 )
			world_msg_client( wld, "%lu line%s of context history.",
					contextcount,
					( contextcount == 1 ) ? "" : "s" );
		else
			world_msg_client( wld, "No context lines." );

		/* And pass context, starting at the line we seeked to. */
		line = contextstart;
		while( line != NULL )
		{
			recalled = line_dup( line );
			recalled->flags |= LINE_NOHIST;
			linequeue_append( wld->client_toqueue, recalled );
			line = line->next;
		}
	}

	/* Inform the user about dropped possibly new lines, if any. */
	if( wld->dropped_inactive_lines > 0 )
	{
		world_msg_client( wld, "WARNING: Due to low buffer space, "
			"%lu possibly new line%s %s been dropped!",
			wld->dropped_inactive_lines,
			( wld->dropped_inactive_lines == 1 ) ? "" : "s", 
			( wld->dropped_inactive_lines == 1 ) ? "has" : "have" );
		wld->dropped_inactive_lines = 0;
	}

	/* Possibly new lines. */
	if( il->count > 0 )
	{
		/* Inform the user about them. */
		world_msg_client( wld, "%lu possibly new line%s "
			"(%.1f%% of buffer).",
			il->count, ( il->count == 1 ) ? "" : "s",
			il->length / 10.24 / wld->max_buffer_size );

		/* Pass the lines. These are already in the history path, so
		 * they need to be duplicated and be NOHISTed. */
		line = wld->inactive_lines->head;
		while( line != NULL )
		{
			recalled = line_dup( line );
			recalled->flags |= LINE_NOHIST;
			linequeue_append( wld->client_toqueue, recalled );
			line = line->next;
		}
	}

	/* Inform the user about dropped certainly new lines, if any. */
	if( wld->dropped_buffered_lines > 0 )
	{
		world_msg_client( wld, "WARNING: Due to low buffer space, "
			"%lu certainly new line%s %s been dropped!",
			wld->dropped_buffered_lines,
			( wld->dropped_buffered_lines == 1 ) ? "" : "s", 
			( wld->dropped_buffered_lines == 1 ) ? "has" : "have" );
		wld->dropped_buffered_lines = 0;
	}

	/* Certainly new lines. */
	if( bl->count > 0 )
	{
		/* Inform the user about them. */
		world_msg_client( wld, "%s%lu certainly new line%s "
			"(%.1f%% of buffer).",
			( ilcount == 0 ) ? "No possibly new lines. " : "",
			bl->count, ( bl->count == 1 ) ? "" : "s",
			bl->length / 10.24 / wld->max_buffer_size );

		/* Pass the lines. Since these are just waiting to be passed
		 * to a client, and never have been in history, they don't
		 * need to be fiddled with and can be passed in one go. */
		linequeue_merge( wld->client_toqueue, wld->buffered_lines );
	}

	/* If there are no new lines at all, say so.
	 * Otherwise, mark the end of all new lines. */
	if( wld->inactive_lines->count + wld->buffered_lines->count == 0 )
		world_msg_client( wld, "No new lines." );
	else
		world_msg_client( wld, "%sEnd of new lines.",
			( blcount == 0 ) ? "No certainly new lines. " : "" );
}



extern void world_recall_history_lines( World *wld, int hlc, int ilc )
{
	Line *line, *recalled;
	int i;

	/* First, do the history lines. */
	if( wld->history_lines->count > 0 && hlc > 0 )
	{
		/* Seek backwards to the correct line. */
		line = wld->history_lines->tail;
		for( i = hlc; i > 1 && line->prev != NULL; i-- )
			line = line->prev;

		/* Duplicate lines, copying them to the toqueue. */
		for( ; line; line = line->next )
		{
			recalled = line_dup( line );
			recalled->flags |= LINE_NOHIST;
			linequeue_append( wld->client_toqueue, recalled );
		}
	}

	/* Second, the inactive lines. */
	if( wld->inactive_lines->count > 0 && ilc > 0 )
	{
		/* Seek backwards to the correct line. */
		line = wld->inactive_lines->tail;
		for( i = ilc; i > 1 && line->prev != NULL; i-- )
			line = line->prev;

		/* Duplicate lines, copying them to the toqueue. */
		for( ; line; line = line->next )
		{
			recalled = line_dup( line );
			recalled->flags |= LINE_NOHIST;
			linequeue_append( wld->client_toqueue, recalled );
		}
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
