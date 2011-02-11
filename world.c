/*
 *
 *  mooproxy - a buffering proxy for moo-connections
 *  Copyright (C) 2001-2011 Marcel L. Moreaux <marcelm@luon.net>
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
#include <stdio.h>
#include <ctype.h>

#include "global.h"
#include "world.h"
#include "misc.h"
#include "line.h"
#include "panic.h"
#include "network.h"



#define EASTEREGG_TRIGGER " vraagt aan je, \"Welke mooproxy versie draai je?\""



static int worlds_count = 0;
static World **worlds_list = NULL;



static void register_world( World *wld );
static void unregister_world( World *wld );
static Line *message_client( World *wld, char *prefix, char *str );



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
	wld->requestedlistenport = -1;
	wld->listen_fds = NULL;
	wld->bindresult = xmalloc( sizeof( BindResult ) );
	world_bindresult_init( wld->bindresult );

	/* Authentication related stuff */
	wld->auth_hash = NULL;
	wld->auth_literal = NULL;
	wld->auth_tokenbucket = NET_AUTH_BUCKETSIZE;
	wld->auth_connections = 0;
	for( i = 0; i < NET_MAXAUTHCONN; i++ )
	{
		wld->auth_buf[i] = xmalloc( NET_MAXAUTHLEN );
		wld->auth_address[i] = NULL;
		wld->auth_fd[i] = -1;
	}
	wld->auth_privaddrs = linequeue_create();

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

	wld->reconnect_enabled = 0;
	wld->reconnect_delay = 0;
	wld->reconnect_at = 0;

	wld->server_rxqueue = linequeue_create();
	wld->server_toqueue = linequeue_create();
	wld->server_txqueue = linequeue_create();
	wld->server_rxbuffer = xmalloc( NET_BBUFFER_ALLOC ); /* See (1) */
	wld->server_rxfull = 0;
	wld->server_txbuffer = xmalloc( NET_BBUFFER_ALLOC ); /* See (1) */
	wld->server_txfull = 0;

	/* Data related to the client connection */
	wld->client_status = ST_DISCONNECTED;
	wld->client_fd = -1;
	wld->client_address = NULL;
	wld->client_prev_address = NULL;
	wld->client_last_connected = 0;
	wld->client_login_failures = 0;
	wld->client_last_failaddr = NULL;
	wld->client_last_failtime = 0;
	wld->client_last_notconnmsg = 0;

	wld->client_rxqueue = linequeue_create();
	wld->client_toqueue = linequeue_create();
	wld->client_txqueue = linequeue_create();
	wld->client_rxbuffer = xmalloc( NET_BBUFFER_ALLOC ); /* See (1) */
	wld->client_rxfull = 0;
	wld->client_txbuffer = xmalloc( NET_BBUFFER_ALLOC ); /* See (1) */
	wld->client_txfull = 0;

	/* Miscellaneous */
	wld->buffered_lines = linequeue_create();
	wld->inactive_lines = linequeue_create();
	wld->history_lines = linequeue_create();
	wld->dropped_inactive_lines = 0;
	wld->dropped_buffered_lines = 0;
	wld->easteregg_last = 0;

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
	wld->log_buffer = xmalloc( NET_BBUFFER_ALLOC ); /* See (1) */
	wld->log_bfull = 0;
	wld->log_lasterror = NULL;
	wld->log_lasterrtime = 0;

	/* MCP stuff */
	wld->mcp_negotiated = 0;
	wld->mcp_key = NULL;
	wld->mcp_initmsg = NULL;

	/* Ansi Client Emulation (ACE) */
	wld->ace_enabled = 0;
	wld->ace_prestr = NULL;
	wld->ace_poststr = NULL;

	/* Options */
	wld->autologin = DEFAULT_AUTOLOGIN;
	wld->autoreconnect = DEFAULT_AUTORECONNECT;
	wld->commandstring = xstrdup( DEFAULT_CMDSTRING );
	wld->strict_commands = DEFAULT_STRICTCMDS;
	wld->infostring = xstrdup( DEFAULT_INFOSTRING );
	wld->infostring_parsed = parse_ansi_tags( wld->infostring );
	wld->newinfostring = xstrdup( DEFAULT_NEWINFOSTRING );
	wld->newinfostring_parsed = parse_ansi_tags( wld->newinfostring );
	wld->context_lines = DEFAULT_CONTEXTLINES;
	wld->buffer_size = DEFAULT_BUFFERSIZE;
	wld->logbuffer_size = DEFAULT_LOGBUFFERSIZE;
	wld->logging = DEFAULT_LOGGING;
	wld->log_timestamps = DEFAULT_LOGTIMESTAMPS;
	wld->easteregg_version = DEFAULT_EASTEREGGS;

	/* Add to the list of worlds */
	register_world( wld );

	return wld;

	/* (1) comment for allocations of several buffers:
	 * 
	 * We allocate using NET_BBUFFER_ALLOC rather than NET_BUFFER_LEN,
	 * because the buffers need to be longer than their "pretend length",
	 * so we can fit in some additional things even when the buffer is
	 * completely full with lines.
	 *
	 * Some things that we need to fit in there:
	 *
	 *   - For the log buffer, a \n after the last line.
	 *   - For the TX buffers, a \r\n and possible ansi sequences for
	 *     client emulation
	 *   - For the RX buffers, a sentinel for the linear search
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
	if( wld->listen_fds )
		for( i = 0; wld->listen_fds[i] > -1; i++ )
			close( wld->listen_fds[i] );
	free( wld->listen_fds );
	world_bindresult_free( wld->bindresult );
	free( wld->bindresult );

	/* Authentication related stuff */
	free( wld->auth_hash );
	free( wld->auth_literal );
	for( i = 0; i < NET_MAXAUTHCONN; i++ )
	{
		free( wld->auth_buf[i] );
		free( wld->auth_address[i] );
		if( wld->auth_fd[i] > -1 )
			close( wld->auth_fd[i] );
	}
	linequeue_destroy( wld->auth_privaddrs );

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

	/* Ansi Client Emulation (ACE) */
	free( wld->ace_prestr );
	free( wld->ace_poststr );

	/* Options */
	free( wld->commandstring );
	free( wld->infostring );
	free( wld->infostring_parsed );
	free( wld->newinfostring );
	free( wld->newinfostring_parsed );

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
	bindresult->listen_fds = NULL;
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
	if( bindresult->listen_fds )
		for( i = 0; bindresult->listen_fds[i] > -1; i++ )
			close( bindresult->listen_fds[i] );
	free( bindresult->listen_fds );
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
	char *str;

	/* Parse the arguments into a string */
	va_start( argp, fmt );
	xvasprintf( &str, fmt, argp );
	va_end( argp );

	/* Send the message with the infostring prefix. */
	return message_client( wld, wld->infostring_parsed, str );
}



extern Line *world_newmsg_client( World *wld, const char *fmt, ... )
{
	va_list argp;
	char *str;

	/* Parse the arguments into a string */
	va_start( argp, fmt );
	xvasprintf( &str, fmt, argp );
	va_end( argp );

	/* Send the message with the newinfostring prefix. */
	return message_client( wld, wld->newinfostring_parsed, str );
}



/* Send prefix + str + <ansi reset> to the client.
 * str is consumed, prefix is not.
 * If prefix is NULL or "", use wld->infostring_parsed.
 * Returns the line object that has been enqueued to the client. */
static Line *message_client( World *wld, char *prefix, char *str )
{
	Line *line;
	char *msg;

	if( prefix == NULL || *prefix == '\0' )
		prefix = wld->infostring_parsed;

	/* Construct the message. The 5 is for the ansi reset and \0. */
	msg = xmalloc( strlen( str ) + strlen( prefix ) + 5 );
	strcpy( msg, prefix );
	strcat( msg, str );
	strcat( msg, "\x1B[0m" );
	free( str );

	/* And append it to the client queue */
	line = line_create( msg, -1 );
	line->flags = LINE_MESSAGE;
	linequeue_append( wld->client_toqueue, line );

	return line;
}



extern void world_trim_dynamic_queues( World *wld )
{
	unsigned long *bll = &wld->buffered_lines->size;
	unsigned long *ill = &wld->inactive_lines->size;
	unsigned long *hll = &wld->history_lines->size;
	unsigned long limit;

	/* Trim the normal buffers. The data is distributed over
	 * buffered_lines, inactive_lines and history_lines. We will trim
	 * them in reverse order until everything is small enough. */
	limit = wld->buffer_size * 1024;

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
	limit = wld->logbuffer_size * 1024;

	/* First log_queue. These are very important, so we count the number
	 * of dropped lines. Remove newest lines. */
	while( wld->log_queue->size + wld->log_current->size > limit &&
			wld->log_queue->head != NULL )
	{
		line_destroy( linequeue_popend( wld->log_queue ) );
		wld->dropped_loggable_lines++;
	}

	/* Next, log_current. These are very important, so we count the number
	 * of dropped lines. Remove newest lines. */
	while( wld->log_current->size > limit &&
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
	Linequeue *queue;
	Line *line, *recalled;
	int nocontext = 0, noposnew = 0, nocernew = 0;

	if( wld->context_lines > 0 )
	{
		/* Recall history lines. */
		queue = world_recall_history( wld, wld->context_lines );

		/* Inform the client of the context lines */
		if( queue->count > 0 )
		{
			world_newmsg_client( wld, "%lu line%s of context "
					"history.", queue->count,
					( queue->count == 1 ) ? "" : "s" );

			/* Move the lines from local queue to client. */
			linequeue_merge( wld->client_toqueue, queue );
		}
		else
			/* Flag no context lines for later reporting. */
			nocontext = 1;

		/* We don't need this anymore. */
		linequeue_destroy( queue );
	}

	/* Possibly new lines. */
	if( il->count > 0 )
	{
		/* Inform the user about them. */
		world_newmsg_client( wld, "%s%lu possibly new line%s "
			"(%.1f%% of buffer).",
			( nocontext == 0 ) ? "" : "No context lines. ",
			il->count, ( il->count == 1 ) ? "" : "s",
			il->size / 10.24 / wld->buffer_size );

		/* If nocontext was 1, we reported on it now. Clear it. */
		nocontext = 0;

		/* Pass the lines. These are already in the history path, so
		 * they need to be duplicated and be NOHISTed. */
		line = wld->inactive_lines->head;
		while( line != NULL )
		{
			recalled = line_dup( line );
			recalled->flags = LINE_RECALLED;
			linequeue_append( wld->client_toqueue, recalled );
			line = line->next;
		}
	}
	else
		/* Flag no possibly new lines for later reporting. */
		noposnew = 1;

	/* Certainly new lines. */
	if( bl->count > 0 )
	{
		/* Inform the user about them. */
		world_newmsg_client( wld, "%s%s%lu certainly new line%s "
			"(%.1f%% of buffer).",
			( nocontext == 0 ) ? "" : "No context lines. ",
			( noposnew == 0 ) ? "" : "No possibly new lines. ",
			bl->count, ( bl->count == 1 ) ? "" : "s",
			bl->size / 10.24 / wld->buffer_size );

		/* If nocontext/noposnew were 1, we reported. Clear them. */
		nocontext = 0;
		noposnew = 0;

		/* Pass the lines. Since these are just waiting to be passed
		 * to a client, and never have been in history, they don't
		 * need to be fiddled with and can be passed in one go. */
		linequeue_merge( wld->client_toqueue, wld->buffered_lines );
	}
	else
		/* Flag no possibly new lines for later reporting. */
		nocernew = 1;

	/* Flag end of new lines, with some special cases... */
	if( noposnew == 1 && nocernew == 1 )
		world_newmsg_client( wld, "%sNo new lines.",
			( nocontext == 0 ) ? "" : "No context lines. " );
	else
		world_newmsg_client( wld, "%sEnd of new lines.",
			( nocernew == 0 ) ? "" : "No certainly new lines. " );

	/* Inform the user about dropped possibly new lines, if any. */
	if( wld->dropped_inactive_lines > 0 )
	{
		world_newmsg_client( wld, "WARNING: Due to low buffer space, "
			"%lu possibly new line%s %s been dropped!",
			wld->dropped_inactive_lines,
			( wld->dropped_inactive_lines == 1 ) ? "" : "s", 
			( wld->dropped_inactive_lines == 1 ) ? "has" : "have" );
		wld->dropped_inactive_lines = 0;
	}

	/* Inform the user about dropped certainly new lines, if any. */
	if( wld->dropped_buffered_lines > 0 )
	{
		world_newmsg_client( wld, "WARNING: Due to low buffer space, "
			"%lu certainly new line%s %s been dropped!",
			wld->dropped_buffered_lines,
			( wld->dropped_buffered_lines == 1 ) ? "" : "s", 
			( wld->dropped_buffered_lines == 1 ) ? "has" : "have" );
		wld->dropped_buffered_lines = 0;
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

	/* Say if we're logging in automatically or manually. */
	if( override )
		world_msg_client( wld, "Logging in on world %s.", wld->name );
	else
		world_msg_client( wld, "Automatically logging in on world %s.",
				wld->name );

	linequeue_append( wld->server_toqueue,
			line_create( xstrdup( wld->auth_literal ), -1 ) );
}



extern void world_inactive_to_history( World *wld )
{
	linequeue_merge( wld->history_lines, wld->inactive_lines );
}



extern Linequeue *world_recall_history( World *wld, long count )
{
	Linequeue *queue;
	Line *line, *recalled;
	char *str;
	long len;

	/* Create our queue. */
	queue = linequeue_create();

	/* If we have no lines or count <= 0, return the empty queue. */
	line = wld->history_lines->tail;
	if( line == NULL || count < 1 )
		return queue;

	/* Seek back from the end of the history, to the first line we 
	 * should display. */
	for( ; line->prev != NULL && count > 1; count-- )
		line = line->prev;

	/* Copy the lines to our local queue. */
	while( line != NULL )
	{
		/* Copy the string, without ASCII BELLs. */
		str = xmalloc( line->len + 1 );
		len = strcpy_nobell( str, line->str );

		/* Resize it, if necessary. */
		if( len != line->len )
			str = xrealloc( str, len + 1 );

		/* Make it into a proper line object... */
		recalled = line_create( str, len );
		recalled->flags = LINE_RECALLED;
		recalled->time = line->time;
		recalled->day = line->day;

		/* And put it in our queue. */
		linequeue_append( queue, recalled );
		line = line->next;
	}

	/* All done, return the queue. */
	return queue;
}



extern void world_rebind_port( World *wld )
{
	BindResult *result = wld->bindresult;
	int i;

	/* Binding to the port we're already bound to makes no sense. */
	if( wld->requestedlistenport == wld->listenport )
	{
		world_msg_client( wld, "Already bound to port %li.",
				wld->requestedlistenport );
		return;
	}

	/* Announce what we're gonna do. */
	world_msg_client( wld, "Binding to new port." );

	/* Actually try and bind to the new port. */
	world_bind_port( wld, wld->requestedlistenport );

	/* Fatal error, inform the user and abort. */
	if( result->fatal )
	{
		world_msg_client( wld, "%s", result->fatal );
		world_msg_client( wld, "The option listenport has not been "
				"changed." );
		return;
	}

	/* Print the result for each address family. */
	for( i = 0; i < result->af_count; i++ )
		world_msg_client( wld, "  %s %s", result->af_success[i]
				? " " : "!", result->af_msg[i] );

	/* Report the conclusion. */
	world_msg_client( wld, "%s", result->conclusion );

	/* We need success on at least one af. */
	if( result->af_success_count == 0 )
	{
		world_msg_client( wld, "The option listenport has not been "
				"changed." );
		return;
	}

	/* Yay, success! Get rid of old file descriptors. */
	if( wld->listen_fds != NULL )
		for( i = 0; wld->listen_fds[i] != -1; i++ )
			close( wld->listen_fds[i] );

	/* Install the new ones. */
	free( wld->listen_fds );
	wld->listen_fds = result->listen_fds;
	result->listen_fds = NULL;

	/* Update the listenport. */
	wld->listenport = wld->requestedlistenport;

	/* Announce success. */
	world_msg_client( wld, "The option listenport has been changed "
			"to %li.", wld->listenport );
}



extern void world_schedule_reconnect( World *wld )
{
	long delay = ( wld->reconnect_delay + 500 ) / 1000;
	char *whenstr;

	/* This is only valid if we're currently not connected. */
	if( wld->server_status != ST_DISCONNECTED )
		return;

	/* And we're not gonna do this if we're not supposed to. */
	if( !wld->reconnect_enabled )
		return;

	/* Indicate we're waiting, and for when. */
	wld->server_status = ST_RECONNECTWAIT;
	wld->reconnect_at = current_time() + delay;

	/* Construct a nice message for the user. */
	if( delay == 0 )
		whenstr = xstrdup( "immediately" );
	else if( delay < 60 )
		xasprintf( &whenstr, "in %li second%s", delay,
				( delay == 1 ) ? "" : "s" );
	else
		xasprintf( &whenstr, "in %li minute%s and %i second%s",
				delay / 60, ( delay / 60 == 1 ) ? "" : "s",
				delay % 60, ( delay % 60 == 1 ) ? "" : "s" );

	/* And send it off. */
	world_msg_client( wld, "Will reconnect %s (at %s).", whenstr,
			time_string( wld->reconnect_at, "%T" ) );
	free( whenstr );

	/* Increase the delay. */
	wld->reconnect_delay *= AUTORECONNECT_BACKOFF_FACTOR;
	wld->reconnect_delay += 3000 + rand() / (RAND_MAX / 4000);
	if( wld->reconnect_delay > AUTORECONNECT_MAX_DELAY * 1000 )
		wld->reconnect_delay = AUTORECONNECT_MAX_DELAY * 1000;

	/* If we have to reconnect immediately, actually do it immediately. */
	if( delay == 0 )
		world_do_reconnect( wld );
}



extern void world_do_reconnect( World *wld )
{
	/* We'll only do a reconnect if we're waiting for reconnecting. */
	if( wld->server_status != ST_RECONNECTWAIT )
		return;

	/* Start the connecting. */
	wld->flags |= WLD_SERVERRESOLVE;

	/* And announce what we did. */
	world_msg_client( wld, "" );
	world_msg_client( wld, "Reconnecting now (at %s).", 
			time_fullstr( current_time() ) );
}



extern void world_decrease_reconnect_delay( World *wld )
{
	/* Decrease the delay. */
	wld->reconnect_delay = wld->reconnect_delay / 2 - 1;
	if( wld->reconnect_delay < 0 )
		wld->reconnect_delay = 0;
}



extern int world_enable_ace( World *wld )
{
	Linequeue *queue;
	char *tmp, *status;
	int cols = wld->ace_cols, rows = wld->ace_rows;

	/* Create the "statusbar". */
	status = xmalloc( cols + strlen( wld->name ) + 20 );
	sprintf( status, "---- mooproxy - %s ", wld->name );
	memset( status + strlen( wld->name ) + 17, '-', cols );
	status[cols] = '\0';

	/* Construct the string to send to the client:
	 *   - reset terminal
	 *   - move cursor to the statusbar location
	 *   - print statusbar
	 *   - set scroll area to "input window"
	 *   - move cursor to the very last line.
	 */
	xasprintf( &tmp, "\x1B" "c\x1B[%i;1H%s\x1B[%i;%ir\x1B[%i;1H",
			rows - 3, status, rows - 2, rows, rows );
	free( status );

	/* We fail if the string doesn't fit into the buffer. */
	if( wld->client_txfull + strlen( tmp ) > NET_BBUFFER_ALLOC )
	{
		free( tmp );
		return 0;
	}

	/* Append the string to the buffer. */
	strcpy( wld->client_txbuffer + wld->client_txfull, tmp );
	wld->client_txfull += strlen( tmp );
	free( tmp );

	/* Construct the string that will be prepended to each line:
	 *   - save cursor position
	 *   - set scroll area to "output window"
	 *   - set cursor to the last line of the "output window"
	 */
	free( wld->ace_prestr );
	xasprintf( &wld->ace_prestr, "\x1B[s\x1B[1;%ir\x1B[%i;1H",
			rows - 4, rows - 4 );

	/* Construct the string that will be appended to each line:
	 *   - set scroll area to "input window"
	 *   - restore cursor position
	 *   - reset attribute mode
	 */
	free( wld->ace_poststr );
	xasprintf( &wld->ace_poststr, "\x1B[%i;%ir\x1B[u\x1B[0m",
			rows - 2, rows );

	/* Of course, ACE is enabled now. */
	wld->ace_enabled = 1;

	/* We want to include the inactive lines in our recall as well, so
	 * we move them to the history right now. */
	world_inactive_to_history( wld );

	/* Recall enough lines to fill the "output window". */
	queue = world_recall_history( wld, rows - 4 );
	linequeue_merge( wld->client_toqueue, queue );
	linequeue_destroy( queue );

	return 1;
}



extern void world_disable_ace( World *wld )
{
	/* Send "reset terminal" to the client, so the clients terminal is
	 * not left in a messed up state.
	 * ACE deactivation should always succeed, so if the ansi sequence
	 * doesn't fit in the buffer, we'll still do all the other stuff. */
	if( wld->client_txfull + 2 < NET_BBUFFER_ALLOC )
	{
		wld->client_txbuffer[wld->client_txfull++] = '\x1B';
		wld->client_txbuffer[wld->client_txfull++] = 'c';
	}

	free( wld->ace_prestr );
	wld->ace_prestr = NULL;

	free( wld->ace_poststr );
	wld->ace_poststr = NULL;

	wld->ace_enabled = 0;
}



/* A little easter egg. If someone asks us (in Dutch, in a specific format)
 * what Mooproxy version we're running, we respond. */
extern void world_easteregg_server( World *wld, Line *line )
{
	const static char *verbs[] =
		{"an", "bl", "fl", "gie", "gl", "gn", "gr", "gri",
		"grm", "ju", "ko", "mo", "mu", "pe", "sn", "ve", "zu", "zwi"};
	const char *verb;
	char *name, *response, *p;

	/* Only once per minute, please. */
	if( wld->easteregg_last > current_time() - 60 )
		return;

	/* If the line is too short, it can't be the trigger. */
	if( line->len < sizeof( EASTEREGG_TRIGGER ) - 1 + 1 )
		return;

	/* Trigger? */
	if( strcmp( line->str + line->len - sizeof( EASTEREGG_TRIGGER ) + 1,
			EASTEREGG_TRIGGER ) )
		return;

	/* Isolate the name of the player triggering the easter egg. */
	name = xmalloc( line->len + 1 );
	strcpy_noansi( name, line->str );
	for( p = name; *p; p++ )
		if( isspace( *p ) )
			break;
	*p = '\0';

	if( name[0] == '[' || name[0] == '<' || name[0] == '{' )
	{
		/* Looks like a channel, let's ignore it. */
		free( name );
		return;
	}

	/* Pick a random verb. */
	verb = verbs[rand() % ( sizeof( verbs ) / sizeof( char * ) )];

	if( name[0] == '>' && name[1] == '>' )
		/* Response to a page. */
		xasprintf( &response, "> %s Ik draai mooproxy %s",
				verb, VERSIONSTR );
	else
		/* Response to a local player. */
		xasprintf( &response, "%s -%s Ik draai mooproxy %s",
				verb, name, VERSIONSTR );

	/* Send the message and clean up. */
	linequeue_append( wld->server_toqueue, line_create( response, -1 ) );
	wld->easteregg_last = current_time();
	free( name );
}



extern int world_start_shutdown( World *wld, int force, int fromsig )
{
	Line *line;

	/* Unlogged data? Refuse if not forced. */
	if( !force && wld->log_queue->size + wld->log_current->size +
			wld->log_bfull > 0 )
	{
		long unlogged_lines = 1 + wld->log_queue->count +
				wld->log_current->count + wld->log_bfull / 80;
		long unlogged_kib = ( wld->log_bfull + wld->log_queue->size +
				wld->log_current->size + 512 ) / 1024;
		world_msg_client( wld, "There are approximately %li lines "
				"(%liKiB) not yet logged to disk. ",
				unlogged_lines, unlogged_kib );
		world_msg_client( wld, "Refusing to shut down. "
				"Use %s to override.",
				fromsig ? "SIGQUIT" : "shutdown -f" );
		return 0;
	}

	/* We try to leave their terminal in a nice state. */
	if( wld->ace_enabled )
		world_disable_ace( wld );

	/* We're good, proceed. */
	line = world_msg_client( wld, "Shutting down%s (reason: %s).",
			force ? " forcibly" : "",
			fromsig ? "signal" : "client request" );
	line->flags = LINE_CHECKPOINT;
	wld->flags |= WLD_SHUTDOWN;

	return 1;
}
