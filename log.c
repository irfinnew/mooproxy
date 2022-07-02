/*
 *
 *  mooproxy - a buffering proxy for MOO connections
 *  Copyright 2001-2011 Marcel Moreaux
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



static void update_one_link( World *wld, char *link, time_t timestamp );
static void log_init( World *, time_t );
static void log_deinit( World * );
static void log_write( World * );
static void nag_client_error( World *, char *, char *, char * );



extern void world_log_line( World *wld, Line *line )
{
	Line *newline;
	char *str;
	long len;

	/* Do we prepend a timestamp? */
	if( wld->log_timestamps )
	{
		/* First, make sure the timestamp is up to date. */
		if( wld->log_currenttime != line->time )
		{
			free( wld->log_currenttimestr );
			wld->log_currenttimestr = xstrdup( time_string(
				line->time, LOG_TIMESTAMP_FORMAT ) );
			wld->log_currenttime = line->time;
		}

		/* Duplicate the line, but with ANSI stripped,
		 * prepending the timestamp. */
		str = xmalloc( line->len + 1 + LOG_TIMESTAMP_LENGTH );
		strcpy( str, wld->log_currenttimestr );
		len = strcpy_noansi( str + LOG_TIMESTAMP_LENGTH, line->str );
		len += LOG_TIMESTAMP_LENGTH;
	}
	else
	{
		/* Duplicate the line, but with ANSI stripped. */
		str = xmalloc( line->len + 1 );
		len = strcpy_noansi( str, line->str );
	}

	/* Process the string into a logline. */
	str = xrealloc( str, len + 1 );
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
		if( wld->log_fd > -1 && ( !wld->logging ||
				wld->log_currentday != current_day() ) )
			log_deinit( wld );

		/* There's nothing to log. If there was a log error situation,
		 * that must be over now, report on that. */
		if( wld->log_lasterror )
		{
			Line *line;

			line = world_msg_client( wld, "Logging resumed success"
				"fully. %li lines have been irretrievably "
				"lost.", wld->dropped_loggable_lines );

			/* Make this line a checkpoint message, but if no
			 * lines were dropped, don't log it. */
			line->flags = LINE_CHECKPOINT;
			if( wld->dropped_loggable_lines == 0 )
				line->flags |= LINE_DONTLOG;

			wld->dropped_loggable_lines = 0;
			free( wld->log_lasterror );
			wld->log_lasterror = NULL;
		}

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
		 * log_queue to log_currentday */
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



extern void world_sync_logdata( World *wld )
{
	if( wld->log_fd == -1 )
		return;

	/* Sync all data written to the logfile FD to disk.
	 * This is best effort, we don't check errors atm. */
	fdatasync( wld->log_fd );
}



extern void world_log_link_remove( World *wld )
{
	int logging  = wld->logging;

	wld->logging = 0;
	world_log_link_update( wld );
	wld->logging = logging;
}



extern void world_log_link_update( World *wld )
{
	time_t timestamp = current_time();
	int today;

	/* Update the 'today' link. */
	update_one_link( wld, "today", timestamp );

	/* Obtain a unix timestamp somewhere in the previous day.
	 * Just subtracting 24*60*60 seconds is not guaranteed to work on
	 * summertime switchover days, because days with a summertime ->
	 * wintertime switch have 25*60*60 seconds. On the other hand,
	 * subtracting 25*60*60 seconds might put us _two_ days before the
	 * current day.
	 * Converting to localtime first and then decrementing the day field
	 * does not help, because the struct tm also contains flags for the
	 * daylight saving time. That means that on switchover days we carry
	 * over the wrong DST flag to the previous day which again leads to
	 * problems.
	 * Finally, renaming 'today' to 'yesterday' at midnight only works
	 * at midnight, not when starting mooproxy.
	 * So we just subtract hours from the current unix timestamp until we
	 * end up in the previous day. Ugly, but it works. */
	today = localtime( &timestamp )->tm_mday;
	while( localtime( &timestamp )->tm_mday == today )
		timestamp -= 3600;

	update_one_link( wld, "yesterday", timestamp );
}



static void update_one_link( World *wld, char *linkname, time_t timestamp )
{
	char *link, *target, *fulltarget;
	struct stat statinfo;
	int ret;

	/* Construct the link path+filename. */
	xasprintf( &link, "%s/%s/%s/%s/%s", get_homedir(), CONFIGDIR,
			LOGSDIR, wld->name, linkname );

	ret = lstat( link, &statinfo );
	/* We give up if stat() fails with anything besides 'file not found'. */
	if( ret == -1 && errno != ENOENT )
	{
		free( link );
		return;
	}
		
	/* Also, if the file exists but isn't a symlink, we leave it alone. */
	if( ret > -1 && !S_ISLNK( statinfo.st_mode ) )
	{
		free( link );
		return;
	}

	/* Old link, begone. */
	unlink( link );

	/* If logging is disabled, we won't create any new symlinks.
	 * Therefore, we're done. */
	if( !wld->logging )
	{
		free( link );
		return;
	}

	/* Create the absolute path+filename of the target file. */
	xasprintf( &fulltarget, "%s/%s/%s/%s/%s/%s - %s.log", get_homedir(),
			CONFIGDIR, LOGSDIR, wld->name,
			time_string( timestamp, "%Y-%m" ),
			wld->name, time_string( timestamp, "%F" ) );

	ret = lstat( fulltarget, &statinfo );
	/* If we can't get to the file, there's no sense in linking to it,
	 * so we bail out. */
	if( ret == -1 )
	{
		free( link );
		free( fulltarget );
		return;
	}

	/* Create the relative path+filename of the target file. */
	xasprintf( &target, "%s/%s - %s.log", time_string( timestamp, "%Y-%m" ),
			wld->name, time_string( timestamp, "%F" ) );

	symlink( target, link );

	free( link );
	free( target );
	free( fulltarget );
}



static void log_init( World *wld, time_t timestamp )
{
	char *file = NULL, *errstr = NULL, *prev = NULL;
	int fd;

	/* Close the current log first. */
	if( wld->log_fd > -1 )
		log_deinit( wld );

	/* Refuse if we have a logfile open. */
	if( wld->log_fd > -1 )
		return;

	/* Try to create ~/CONFIGDIR */
	xasprintf( &file, "%s/%s", get_homedir(), CONFIGDIR );
	if( attempt_createdir( file, &errstr ) )
		goto createdir_failed;

	/* Try to create .../LOGSDIR */
	prev = file;
	xasprintf( &file, "%s/%s", prev, LOGSDIR );
	if( attempt_createdir( file, &errstr ) )
		goto createdir_failed;
	free( prev );

	/* Try to create .../$world */
	prev = file;
	xasprintf( &file, "%s/%s", prev, wld->name );
	if( attempt_createdir( file, &errstr ) )
		goto createdir_failed;
	free( prev );

	/* Try to create .../YYYY-MM */
	prev = file;
	xasprintf( &file, "%s/%s", prev, time_string( timestamp, "%Y-%m" ) );
	if( attempt_createdir( file, &errstr ) )
		goto createdir_failed;
	free( prev );

	/* Filename: .../$world - YYYY-MM-DD.log */
	prev = file;
	xasprintf( &file, "%s/%s - %s.log", prev,
			wld->name, time_string( timestamp, "%F" ) );
	free( prev );

	/* Try and open the logfile. */
	fd = open( file, O_WRONLY | O_CREAT | O_APPEND | O_NONBLOCK,
			S_IRUSR | S_IWUSR );

	/* Error opening logfile? Complain. */
	if( fd == -1 )
	{
		nag_client_error( wld, "Could not open", file,
				strerror( errno ) );
		free( file );
		return;
	}

	wld->log_fd = fd;
	free( file );

	/* Update the log symlinks later. */
	wld->flags |= WLD_LOGLINKUPDATE;

	return;

createdir_failed:
	nag_client_error( wld, "Could not create", file, errstr );
	free( file );
	/* Prevent double free()s ;) */
	if( file != prev )
		free( prev );
	free( errstr );
}



static void log_deinit( World *wld )
{
	/* Refuse if there's something left in the buffer. */
	if( wld->log_bfull > 0 )
		return;

	/* We're closing the logfile, it'd be nice if stuff ended up on
	 * disk now. */
	world_sync_logdata( wld );

	if( wld->log_fd != -1 )
		close( wld->log_fd );

	wld->log_fd = -1;

	/* Update the log symlinks later. */
	wld->flags |= WLD_LOGLINKUPDATE;
}



static void log_write( World *wld )
{
	int ret, errnum;

	/* No logfile, no writes */
	if( wld->log_fd == -1 )
		return;

	ret = flush_buffer( wld->log_fd, wld->log_buffer, &wld->log_bfull,
			wld->log_current, NULL, 0, NULL, NULL, &errnum );

	if( ret == 1 )
		nag_client_error( wld, "Could not write to logfile", NULL,
				"file descriptor is congested" );
	if( ret == 2 )
		nag_client_error( wld, "Could not write to logfile", NULL,
				strerror( errnum ) );
}



static void nag_client_error( World *wld, char *msg, char *file, char *err )
{
	Line *line;
	char *str;
	int buffer_line = 0;

	/* Construct the message */
	if( file == NULL )
		xasprintf( &str, "%s: %s!", msg, err );
	else
		xasprintf( &str, "%s `%s': %s!", msg, file, err );

	/* Either the error should be different, or LOG_MSGINTERVAL seconds
	 * should have passed. Otherwise, be silent. */
	if( current_time() - wld->log_lasterrtime <= LOG_MSGINTERVAL &&
		wld->log_lasterror && !strcmp( wld->log_lasterror, str ) )
	{
		free( str );
		return;
	}

	/* If the new error is the same as the old error, we're just repeating
	 * ourselves. In that case, flag the line as DONTBUF, so the user
	 * doesn't get spammed on connect.
	 * If however, the new error is different, let the line be buffered. */
	if( wld->log_lasterror && !strcmp( wld->log_lasterror, str ) )
		buffer_line = LINE_DONTBUF;

	/* If we have dropped loggable lines, yell about that first. */
	if( wld->dropped_loggable_lines > 0 )
	{
		line = world_msg_client( wld, "LOGGABLE LINES DROPPED! %li "
				"lines have been irretrievably lost!",
				wld->dropped_loggable_lines );
		line->flags |= buffer_line;
	}

	/* Next, inform the user that logging failed and how much logbuffer
	 * space is left. */
	line = world_msg_client( wld, "LOGGING FAILED! Approximately %lu lines"
		" not yet logged (%.1f%% of logbuffer).", wld->log_bfull / 80 +
		wld->log_queue->count + wld->log_current->count + 1, 
		( wld->log_queue->size + wld->log_current->size )
		/ 10.24 / wld->logbuffer_size );
	line->flags |= buffer_line;

	/* Finally, report the reason of the failure. */
	line = world_msg_client( wld, "%s", str );
	line->flags |= buffer_line;

	/* Replace the old error by the new error. */
	wld->log_lasterrtime = current_time(); 
	free( wld->log_lasterror );
	wld->log_lasterror = str;
}
