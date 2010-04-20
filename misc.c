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



#define _GNU_SOURCE /* For vasprintf(). */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "global.h"
#include "misc.h"
#include "line.h"
#include "panic.h"



static time_t current_second = 0;
static long current_daynum = 0;
static char *empty_homedir = "";

/* Lookup table for the translation of codes like %W to ANSI sequences. */
static const char *ansi_sequences[] = {
	"", "\x1B[1;34m", "\x1B[1;36m", "", "", "", "\x1B[1;32m", "", "", "",
	"\x1B[1;30m", "", "\x1B[1;35m", "", "", "", "", "\x1B[1;31m", "", "",
	"", "", "\x1B[1;37m", "", "\x1B[1;33m",
	
	"", "", "", "", "", "", "",

	"", "\x1B[0;34m", "\x1B[0;36m", "", "", "", "\x1B[0;32m", "", "", "",
	"\x1B[0;30m", "", "\x1B[0;35m", "", "", "", "", "\x1B[0;31m", "", "",
	"", "", "\x1B[0;37m", "", "\x1B[0;33m", ""
};



extern void *xmalloc( size_t size )
{
	void *ptr;
	int i;

	ptr = malloc( size );

	if( ptr != NULL )
		return ptr;

	/* Allocation failed. Retry a few times. */
	for( i = 0; i < XMALLOC_OOM_RETRIES; i++ )
	{
		sleep( 1 );
		ptr = malloc( size );
		if( ptr != NULL )
			return ptr;
	}

	/* Still failed, give up. */
	panic( PANIC_MALLOC, 0, (unsigned long) size );
	return NULL;
}



extern void *xrealloc( void *ptr, size_t size )
{
	int i;

	ptr = realloc( ptr, size );

	if( ptr != NULL || size == 0 )
		return ptr;

	/* Reallocation failed. Retry a few times. */
	for( i = 0; i < XMALLOC_OOM_RETRIES; i++ )
	{
		sleep( 1 );
		ptr = realloc( ptr, size );
		if( ptr != NULL )
			return ptr;
	}

	/* Still failed, give up. */
	panic( PANIC_REALLOC, 0, (unsigned long) size );
	return NULL;
}



extern char *xstrdup( const char *s )
{
	char *str;
	int i;

	str = strdup( s );

	if( str != NULL )
		return str;

	/* Strdup failed. Retry a few times. */
	for( i = 0; i < XMALLOC_OOM_RETRIES; i++ )
	{
		sleep( 1 );
		str = strdup( s );
		if( str != NULL )
			return str;
	}

	/* Still failed, give up. */
	panic( PANIC_STRDUP, 0, (unsigned long) strlen( s ) );
	return NULL;
}



extern char *xstrndup( const char *s, size_t n )
{
	char *str;
	int i;

	str = strndup( s, n );

	if( str != NULL )
		return str;

	/* Strdup failed. Retry a few times. */
	for( i = 0; i < XMALLOC_OOM_RETRIES; i++ )
	{
		sleep( 1 );
		str = strndup( s, n );
		if( str != NULL )
			return str;
	}

	/* Still failed, give up. */
	panic( PANIC_STRDUP, 0, (unsigned long) n );
	return NULL;
}



extern int xasprintf( char **strp, const char *fmt, ... )
{
	va_list argp;
	int i;

	va_start( argp, fmt );
	i = xvasprintf( strp, fmt, argp );
	va_end( argp );

	return i;
}



extern int xvasprintf( char **strp, const char *fmt, va_list argp )
{
	int i, r;

	r = vasprintf( strp, fmt, argp );

	if( r >= 0 )
		return r;

	/* Vasprintf failed. Retry a few times. */
	for( i = 0; i < XMALLOC_OOM_RETRIES; i++ )
	{
		sleep( 1 );
		r = vasprintf( strp, fmt, argp );
		if( r >= 0 )
			return r;
	}

	/* Still failed, give up. */
	panic( PANIC_VASPRINTF, 0, 0 );
	return 0;
}



extern void set_current_time( time_t t )
{
	current_second = t;
}



extern time_t current_time( void )
{
	return current_second;
}



extern void set_current_day( long day )
{
	current_daynum = day;
}



extern long current_day( void )
{
	return current_daynum;
}



extern char *get_homedir( void )
{
	char *tmp;

	tmp = getenv( "HOME" );
	if( tmp )
		return tmp;

	return empty_homedir;
}



extern char *read_one_line( char **str )
{
	char *s, b, *line;

	/* Nothing left? */
	if( **str == '\0' )
		return NULL;

	/* Search for the first \n or \0 */
	s = *str;
	while( *s != '\n' && *s != '\0' )
		s++;

	/* Back up the \n or \0, and replace it with \0 */
	b = *s;
	*s = '\0';

	/* Duplicate the first line */
	line = xstrdup( *str );

	/* Restore the backup */
	*s = b;

	/* If it was a newline, advance str one further, so it points to
	 * the next line. But it should not point beyond a \0. */
	if( *s == '\n' )
		s++;

	*str = s;
	return line;
}



extern char *get_one_word( char **str )
{
	char *s = *str, *word;

	/* Find the first non-whitespace character */
	while( isspace( *s ) )
		s++;

	/* If that's \0, we're out of words. */
	if( *s == '\0' )
		return NULL;

	word = s;

	/* Look for the first whitespace character or \0 */
	while( !isspace( *s ) && *s != '\0' )
		s++;

	/* If we have a whitespace character, replace it with \0 */
	if( *s != '\0' )
		*s++ = '\0';

	*str = s;
	return word;
}



extern char *peek_one_word( char *str )
{
	char *s = str, *t, *word;

	/* Find the first non-whitespace character */
	while( isspace( *s ) )
		s++;

	/* If that's \0, we're out of words. */
	if( *s == '\0' )
		return NULL;

	t = s;

	/* Look for the first whitespace character or \0 */
	while( !isspace( *s ) && *s != '\0' )
		s++;

	word = xmalloc( s - t + 1 );
	memcpy( word, t, s - t );
	word[s - t] = '\0';

	return word;
}



extern char *trim_whitespace( char *line )
{
	size_t r, s, t = strlen( line );

	/* Find the first non-whitespace character */
	for( s = 0; s <= t; s++ )
		if( !isspace( line[s] ) )
			break;

	/* Move everything from the first non-whitespace character to the
	 * start of the string. */
	for( r = s; r <= t; r++	)
		line[r - s] = line[r];

	/* Subtract number of skipped ws chars from the length */
	t -= s;

	if( t )
	{
		/* Search for the last non-whitespace character. */
		for( t--; t >= 0; t-- )
			if( !isspace( line[t] ) )
				break;
		/* Found it, make t point to the (whitespace) character
		 * after the last non-whitespace character. */
		t++;
	}

	/* Chop off trailing whitespace. */
	line[t] = '\0';

	return line;
}



extern char *remove_enclosing_quotes( char *str )
{
	size_t i, len = strlen( str );

	/* If the length is < 2, it can't contain two quotes. */
	if( len < 2 )
		return str;
	/* If the first character is not ' or ", we're done. */
	if( str[0] != '"' && str[0] != '\'' )
		return str;
	/* If the last character is not ' or ", we're done. */
	if( str[len - 1] != '"' && str[len - 1] != '\'' )
		return str;
	/* If the first and last characters are not equal, we're done. */
	if( str[0] != str[len - 1] )
		return str;

	/* Shift string one character forward, eliminating leading quote. */
	for( i = 0; i < len - 2; i++ )
		str[i] = str[i + 1];

	/* Chop off trailing quote. */
	str[len - 2] = '\0';

	return str;
}



extern int true_or_false( const char *str )
{       
	if( !strcasecmp( str, "true" ) )
		return 1;
	if( !strcasecmp( str, "false" ) )
		return 0;
	if( !strcasecmp( str, "yes" ) )
		return 1;
	if( !strcasecmp( str, "no" ) )
		return 0;
	if( !strcasecmp( str, "on" ) )
		return 1;
	if( !strcasecmp( str, "off" ) )
		return 0;
	if( !strcasecmp( str, "1" ) )
		return 1;
	if( !strcasecmp( str, "0" ) )
		return 0;

	return -1;
}



extern char *time_string( time_t t, const char *fmt )
{
	static char timestr_buf[TIMESTR_MAXSIMULT][TIMESTR_MAXLEN];
	static int current_buf = TIMESTR_MAXSIMULT;
	struct tm *tms;

	current_buf++;
	if( current_buf >= TIMESTR_MAXSIMULT )
		current_buf = 0;

	timestr_buf[current_buf][0] = '\0';

	tms = localtime( &t );
	strftime( timestr_buf[current_buf], TIMESTR_MAXLEN, fmt, tms );

	return timestr_buf[current_buf];
}



extern char *time_fullstr( time_t t )
{
	return time_string( t, "%A %d %b %Y, %T" );
}



extern int buffer_to_lines( char *buffer, int offset, int read, Linequeue *q )
{
	char *eob = buffer + offset + read, *new;
	char *start = buffer, *end = buffer + offset;
	size_t len;

	/* eob:    end of buffer. Points _beyond_ the last char of the buffer
	 * new:    Used temporarily to hold the copy of the current line
	 * start:  start of the current line
	 * end:    end of the current line */

	/* Place the sentinel \n at the end of the buffer. */
	*eob = '\n';

	while( end < eob )
	{
		/* Search for \n using linear search with sentinel. */
		while( *end != '\n' )
			end++;

		/* We reached the end of buffer without hitting a newline,
		 * and there is more room in the buffer. Abort, and let
		 * more data accumulate in the buffer. */
		if( end == eob && end < buffer + NET_BBUFFER_LEN )
			break;

		/* Reached end of buffer, and there's no space left in the
		 * buffer. But, we consumed some lines from the buffer,
		 * so we should shift the unconsumed data to the start
		 * of buffer, and let more data accumulate in the buffer. */
		if( end == eob && start != buffer )
			break;

		/* If we got here, either we hit a \n, or the buffer is
		 * filled entirely with one big line. Either way: process it */

		/* Chop leading \r */
		if( *start == '\r' )
			start++;

		len = end - start;
		/* If the last character before \n is a \r (and it's not
		 * before the start of string), chop it. */
		if( end > start && *( end - 1 ) == '\r' )
			len--;

		/* Copy the line out of the buffer, NUL-terminate it,
		 * create a line, and queue it. */
		new = xmalloc( len + 1 );
		memcpy( new, start, len );
		new[len] = '\0';
		linequeue_append( q, line_create( new, len ) );

		/* If the end-of-line is before end-of-buffer, advance end
		 * so it points beyond the \n. */
		if( end < eob )
			end++;

		start = end;
	}

	/* If the start of the first line left in the buffer is not at the
	 * start of the buffer, move it. */
	if( start != buffer )
		memmove( buffer, start, offset + read - ( start - buffer ) );

	return offset + read - ( start - buffer );
}



extern int flush_buffer( int fd, char *buffer, long *bffl, Linequeue *queue,
		Linequeue *tohist, int network_nl, char *prestr, char *poststr,
		int *errnum )
{
	long bfull = *bffl, len;
	Line *line;
	int wr;

	while( bfull > 0 || queue->count > 0 )
	{
		/* Add any queued lines into the buffer. */
		while( queue->count > 0 )
		{
			/* Get the length of the next line. */
			len = queue->head->len;
			/* If it's too large, truncate (or it'll never fit) */
			if( len > NET_BBUFFER_LEN )
				len = NET_BBUFFER_LEN;

			/* If the line doesn't fit, bail out. */
			if( bfull + len > NET_BBUFFER_LEN )
				break;

			/* First, write the prepend-string, if present. */
			if( prestr )
			{
				strcpy( buffer + bfull, prestr );
				bfull += strlen( prestr );
			}

			/* Now, get the line itself, and write to the buffer. */
			line = linequeue_pop( queue );
			memcpy( buffer + bfull, line->str, len );
			bfull += len;

			/* Next up, the newline. */
			if( network_nl )
				buffer[bfull++] = '\r';
			buffer[bfull++] = '\n';

			/* And finally the append-string, if present. */
			if( poststr )
			{
				strcpy( buffer + bfull, poststr );
				bfull += strlen( poststr );
			}

			/* Move the line to a history queue, or destroy it. */
			if( tohist == NULL || line->flags & LINE_NOHIST )
				line_destroy( line );
			else
				linequeue_append( tohist, line );
		}

		/* Now we try to write as much of the buffer as possible */
		wr = write( fd, buffer, bfull );

		/* If there was an error, or if the socket can't take any
		 * more data, abort. The higher layers will handle
		 * errors or congestion. */
		if( wr == -1 || wr == 0 )
		{
			*bffl = bfull;
			if( errnum != NULL )
				*errnum = errno;
			if( wr == -1 )
				return 2;
			else
				return 1;
		}

		/* If only part of the buffer was written, move the unwritten
		 * part of the data to the start of the buffer. */
		if( wr < bfull )
			memmove( buffer, buffer + wr, bfull - wr );

		bfull -= wr;
	}

	*bffl = bfull;
	return 0;
}



extern char *parse_ansi_tags( char *str )
{
	char *parsed, *parsedstart;

	if( str == NULL )
		return NULL;

	/* We allocate plenty, to make sure the parsed string fits. */
	parsed = xmalloc( strlen( str ) * 8 + 8 );
	parsedstart = parsed;

	while( *str != '\0' )
	{
		if( *str != '%' )
		{
			/* Don't copy control chars. */
			if( *str < 32 )
				str++;
			else
				*parsed++ = *str++;
			continue;
		}

		/* We got a %, advance to the next character. */
		str++;

		if( *str == '\0' )
			break;

		if( *str == '%' )
		{
			*parsed++ = *str++;
			continue;
		}

		if( *str >= 'A' && *str <= 'z' )
		{
			strcpy( parsed, ansi_sequences[*str - 'A'] );
			parsed += strlen( ansi_sequences[*str - 'A'] );
			str++;
			continue;
		}

		str++;
	}

	*parsed = '\0';
	parsed = xrealloc( parsedstart, strlen( parsedstart ) + 1 );

	return parsed;
}



extern int attempt_createdir( char *dirname, char **err )
{
	struct stat fileinfo;

	if( mkdir( dirname, S_IRUSR | S_IWUSR | S_IXUSR ) == 0 )
		return 0;

	/* If there was an error other than "already exists", complain */
	if( errno != EEXIST )
	{
		*err = xstrdup( strerror( errno ) );
		return 1;
	}

	/* The directory already existed. But is it really a dir? */
	if( stat( dirname, &fileinfo ) == -1 )
	{
		if( errno == ENOTDIR )
			*err = xstrdup( "File exists" );
		else
			*err = xstrdup( strerror( errno ) );

		return 1;
	}
	
	/* Is it? */
	if( !S_ISDIR( fileinfo.st_mode ) )
	{
		*err = xstrdup( "File exists" );
		return 1;
	}

	return 0;
}



extern long strcpy_noansi( char *dest, char *src )
{
	char *origdest = dest;

	for(;;)
	{
		/* Normal character, copy and continue. */
		if( (unsigned char) *src >= ' ' || *src == '\t' )
		{
			*dest++ = *src++;
			continue;
		}

		/* Escape char, skip ANSI sequence. */
		if( *src == '\x1B' )
		{
			src++;
			if( *src == '[' )
				while( *src != '\0' && !isalpha( *src ) )
					src++;

			if( *src != '\0' )
				src++;
			continue;
		}

		/* Nul, terminate. */
		if( *src == '\0' )
			break;

		/* A character we don't deal with, skip over it. */
		src++;
	}
	*dest = '\0';

	return dest - origdest;
}



extern long strcpy_nobell( char *dest, char *src )
{
	char *origdest = dest;

	/* Copy the string, skipping over ASCII BELL. */
	while( *src )
		if( *src != 0x07 )
			*dest++ = *src++;
		else
			src++;

	/* Nul-terminate. */
	*dest = '\0';

	return dest - origdest;
}



extern int strcmp_under( char *s, char *t )
{
	for(;;)
	{
		while( *s == '_' )
			s++;

		while( *t == '_' )
			t++;

		while( *s && *s == *t )
			s++, t++;

		if( *s == '\0' && *t == '\0' )
			return 0;

		if( *s != '_' && *t != '_' )
			return *s - *t;
	}
}
