/*
 *
 *  mooproxy - a buffering proxy for moo-connections
 *  Copyright (C) 2001-2006 Marcel L. Moreaux <marcelm@luon.net>
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
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "global.h"
#include "world.h"
#include "log.h"
#include "misc.h"
#include "line.h"



static char *strip_ansi( char *, long, long * );
static void log_init( World *, time_t );
static void log_deinit( World * );
static void log_write( World * );
static void nag_client_error( World *, char *, char *, char * );



extern void world_log_line( World *wld, Line *line )
{
	Line *newline;
	char *str;
	long len;

	/* Duplicate the line, but with ANSI stripped. */
	str = strip_ansi( line->str, line->len, &len );
	newline = line_create( str, len );
	newline->flags = line->flags;
	newline->time = line->time;
	newline->day = line->day;

	/* Put the new line in the to-be-logged queue. */
	linequeue_append( wld->log_queue, newline );
}



extern void world_flush_client_logqueue( World *wld )
{
	/* See if there's anything to log at all */
	if( wld->log_queue->count + wld->log_current->count +
			wld->log_bfull == 0 )
	{
		/* If:
		 *   - There's nothing to log, and
		 *   - We have a log file open, and
		 *   - Either:
		 *     - Logging is disabled, or
		 *     - It's a different day now
		 * we should close the log file. */
		if( wld->log_fd > -1 && ( !wld->logging_enabled ||
				wld->log_currentday != current_day() ) )
			log_deinit( wld );

		/* Nothing to log, we're done. */
		return;
	}

	/* Ok, we have something to log. */

	if( wld->log_queue->count > 0 )
	{
		/* If the current day queue is empty, and there are lines
		 * from a different day waiting, the current day is done. */
		if( wld->log_current->count + wld->log_bfull == 0 &&
			wld->log_queue->head->day != wld->log_currentday )
		{
			/* Close the current logfile.
			 * A new one will be opened automatically. */
			log_deinit( wld );
			wld->log_currentday = wld->log_queue->head->day;
			return;
		}

		/* Move lines that were logged in the current day from
		 * log_queue to log_current */
		while( wld->log_queue->count > 0 && wld->log_queue->head->day
				== wld->log_currentday )
			linequeue_append( wld->log_current,
					linequeue_pop( wld->log_queue ) );
	}

	/* Lines waiting and no log file open? Open one! */
	if( wld->log_fd == -1 && wld->log_current->count > 0 )
		log_init( wld, wld->log_current->head->time );

	/* Actually try and write lines from the current day. */
	log_write( wld );
}



static char *strip_ansi( char *orig, long len, long *nlen )
{
	char *str, *newstr = xmalloc( len + 1 );
	int escape = 0;

	/* State machine:
	 * escape = 0   Processing normal characters.
	 * escape = 1   Seen ^[, expect [
	 * escape = 2   Seen ^[[, process until alpha char */

	for( str = newstr; *orig != '\0'; orig++ )
	{
		switch( escape )
		{
			case 0:
			if( *orig == 0x1B )
				escape = 1;
			/* A normal character. Copy it. */
			if( (unsigned char) *orig >= ' ' )
				*str++ = *orig;
			break;

			case 1:
			if( *orig == '[' )
				escape = 2;
			else
				escape = 0;
			break;

			case 2:
			if( isalpha( *orig ) )
				escape = 0;
			break;
		}
	}
	*str = '\0';

	*nlen = str - newstr;
	newstr = realloc( newstr, *nlen + 1 );
	return newstr;
}



static void log_init( World *wld, time_t timestamp )
{
	char *file, *home;
	int fd;

	/* Close the current log first. */
	log_deinit( wld );

	/* Filename: ~/LOGSDIR/worldname - date.log */
	home = get_homedir();
	xasprintf( &file, "%s" LOGSDIR "%s - %s.log", home, wld->name,
			time_string( timestamp, "%F" ) );
	free( home );

	fd = open( file, O_WRONLY | O_CREAT | O_APPEND | O_NONBLOCK,
			S_IRUSR | S_IWUSR );

	/* Error opening logfile? Complain. */
	if( fd == -1 )
	{
		nag_client_error( wld, "Could not open logfile", file,
				strerror( errno ) );
		free( file );
		return;
	}

	wld->log_fd = fd;
	free( file );
}



static void log_deinit( World *wld )
{
	/* Refuse if there's something left in the buffer. */
	if( wld->log_bfull > 0 )
		return;

	if( wld->log_fd != -1 )
		close( wld->log_fd );

	wld->log_fd = -1;
}



static void log_write( World *wld )
{
	int ret, errnum;

	/* No logfile, no writes */
	if( wld->log_fd == -1 )
		return;

	ret = flush_buffer( wld->log_fd, wld->log_buffer, &wld->log_bfull,
			wld->log_current, NULL, 0, &errnum );

	if( ret == 1 )
		nag_client_error( wld, "Could not write to logfile", NULL,
				"nothing was written" );
	if( ret == 2 )
		nag_client_error( wld, "Could not write to logfile", NULL,
				strerror( errnum ) );
}



static void nag_client_error( World *wld, char *msg, char *file, char *err )
{
	char *str;

	/* Construct the message */
	if( file == NULL )
		xasprintf( &str, "%s: %s!", msg, err );
	else
		xasprintf( &str, "%s %s: %s!", msg, file, err );

	/* Either the error should be different, or LOG_MSGINTERVAL seconds
	 * should have passed. Otherwise, be silent. */
	if( current_time() - wld->log_lasterrtime > LOG_MSGINTERVAL ||
			strcmp( wld->log_lasterror, str ) )
	{
		world_msg_client( wld, "%s", str );
		wld->log_lasterrtime = current_time(); 
		free( wld->log_lasterror );
		wld->log_lasterror = str;
	}
	else
		free( str );
}
