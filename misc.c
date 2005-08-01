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
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include "misc.h"



#define MAX_STRERROR_LEN 128
#define MAX_TIMESTR_LEN 128
/* The estimated memory cost of a line object: the size of the object itself,
 * and some random guess at memory management cost */
#define LINE_BYTE_COST ( sizeof( Line ) + 20 )



/* Exactly like the GNU asprintf() (though different implementation.
 * It is like sprintf, but it allocates its own memory.
 * The memory should be free()d afterwards. */
int asprintf( char **strp, char *format, ... )
{
	va_list argp;
	int i;

	va_start( argp, format );

	*strp = malloc( 1024 );
	i = vsnprintf( *strp, 1023, format, argp );
	if( i != -1 && i < 1023 )
	{
		va_end( argp );
		return i;
	}

	*strp = realloc( *strp, 102400 );
	i = vsnprintf( *strp, 102399, format, argp );
	if( i != -1 && i < 102399 )
	{
		*strp = realloc( *strp, i + 1 );
		va_end( argp );
		return i;
	}

	free( *strp );
	*strp = strdup( "asprintf(): vsnprintf() string exceeded 100 KB!\n" );
	va_end( argp );
	return strlen( *strp );
}



/* This function gets the users homedir from the environment and returns it.
 * The string is strdup()ped, so it should be free()d.
 * The string will either be empty, or /-terminated */
char *get_homedir( void )
{
	char *tmp;

	tmp = getenv( "HOME" );
	if( tmp == NULL )
		return strdup( "" );

	tmp = strdup( tmp );
	if( tmp[strlen( tmp ) - 1] != '/' )
	{
		tmp = realloc( tmp, strlen( tmp ) + 2 );
		strcat( tmp, "/" );
	}

	return tmp;
}



/* Returns the given string, up to but excluding the first newline.
 * The given pointer is set to point to the first character after the
 * newline.
 * When there is nothing left, it returns NULL
 * The returned lines are separately allocated. */
char *read_one_line( char **str )
{
	char *s, b, *line;

	if( **str == '\0' )
		return NULL;

	for( s = *str; *s != '\n' && *s != '\0'; s++ )
		;

	b = *s;
	*s = '\0';

	line = strdup( *str );

	*s = b;

	if( *s == '\n' )
		s++;

	*str = s;
	return line;
}



/* Returns the given string, up to but excluding the first whitespace char.
 * The given pointer is set to point to the first character after the
 * whitespace character.
 * When there is nothing left, it returns NULL
 * The string pointed to is mangled, returned words are still inside. */
char *get_one_word( char **str )
{
	char *s = *str, *word;

	while( isspace( *s ) )
		s++;

	if( *s == '\0' )
		return NULL;

	while( !isspace( *s ) && *s != '\0' )
		s++;

	if( isspace( *s ) )
		*s++ = '\0';
	else
		*s = '\0';

	word = *str;

	*str = s;
	return word;
}



/* Strips all whitespace (determined by isspace()) from a line and returns
 * the modified string */
char *strip_all_whitespace( char *line )
{
	size_t s, t = strlen( line );
	int i = -1;


	for( s = 0; s <= t; s++ )
	{
		if( isspace( line[s] ) )
		{
			if( i == -1 )
				i = s;
		}
		else if( i != -1 )
		{
			strcpy( &line[i], &line[s] );
			t -= ( s - i );
			s = i;
			i = -1;
		}
	}

	return line;
}



/* Strips all whitespace (determined by isspace()) in the left and right
 * of the string and returns the modified string */
char *trim_whitespace( char *line )
{
	size_t r, s, t = strlen( line );

	for( s = 0; s <= t; s++ )
		if( !isspace( line[s] ) )
			break;

	for( r = s; r <= t; r++	)
		line[r - s] = line[r];

	t -= s;

	if( t )
	{
		for( t--; t >= 0; t-- )
			if( !isspace( line[t] ) )
				break;
		t++;
	}

	line[t] = '\0';

	return line;
}



/* If the string starts and ends with ", remove both. Return the new str. */
extern char *remove_enclosing_quotes( char *str )
{
	size_t i, len = strlen( str );

	if( len < 2 )
		return str;
	if( str[0] != '"' && str[0] != '\'' )
		return str;
	if( str[len - 1] != '"' && str[len - 1] != '\'' )
		return str;
	if( str[0] != str[len - 1] )
		return str;

	for( i = 0; i < len - 2; i++ )
		str[i] = str[i + 1];

	str[len - 2] = '\0';

	return str;
}



/* Converts an entire string to lowercase and returns the modified string */
char *lowercase( char *str )
{
	for( ; *str; str++ )
		*str = tolower( *str );

	return str;
}



/* Determines if a string says true or false (case insensitive).
 * If the string is "true", "yes", "on" or "1", it returns 1.
 * If the string is "false", "no", "off" or "0", it returns 0.
 * If the string is unknown, return -1 */
int true_or_false( char *str )
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

	return( -1 );
}



/* Exactly like strerror(), except for the fucking irritating trailing \n */
extern char *strerror_n( int err )
{
	static char strerror_buf[MAX_STRERROR_LEN];
	char *str;
	size_t len;

	str = strerror( err );

	strncpy( strerror_buf, str, MAX_STRERROR_LEN );
	strerror_buf[MAX_STRERROR_LEN - 1] = '\0';

	len = strlen( strerror_buf );
	if( len > 0 && strerror_buf[len - 1] == '\n' )
		strerror_buf[len - 1] = '\0';

	return strerror_buf;
}



/* Convert the given time to a formatted string in local time.
 * For the format, see strftime(3) .
 * It returns a pointer to a statically allocated buffer. */
extern char *time_string( time_t t, char *fmt )
{
	static char timestr_buf[MAX_TIMESTR_LEN];
	struct tm *tms;

	tms = localtime( &t );
	strftime( timestr_buf, MAX_TIMESTR_LEN, fmt, tms );

	return timestr_buf;
}



/* Create a line, flags are clear, prev/next are NULL.
 * If length is -1, it's calculated. */
extern Line *line_create( char *str, long len )
{
	Line *line;

	line = malloc( sizeof( Line ) );
	line->str = str;
	line->len = len == -1 ? strlen( str ) : len;
	line->flags = 0;
	line->prev = NULL;
	line->next = NULL;

	return line;
}



/* Destroy a line, freeing its resources */
extern void line_destroy( Line *line )
{
	if( line )
		free( line->str );
	free( line );
}



/* Return a duplicate of the line (prev/next are NULL). */
extern Line *line_dup( Line *line )
{
	Line *newline;

	newline = malloc( sizeof( Line ) );
	newline->str = malloc( line->len + 1 );
	strcpy( newline->str, line->str );
	newline->len = line->len;
	newline->flags = line->flags;
	newline->prev = NULL;
	newline->next = NULL;

	return newline;
}



/* Allocate and initialize a line queue */
extern Linequeue *linequeue_create( void )
{
	Linequeue *queue;

	queue = malloc( sizeof( Linequeue ) );

	queue->head= NULL;
	queue->tail = NULL;
	queue->count = 0;
	queue->length = 0;

	return queue;
}



/* De-initialize and free al resources */
extern void linequeue_destroy( Linequeue *queue )
{
	linequeue_clear( queue );

	free( queue );
}



/* Clear all lines in the queue */
extern void linequeue_clear( Linequeue *queue )
{
	Line *line, *next;

	for( line = queue->head; line; line = next )
	{
		next = line->next;
		free( line->str );
		free( line );
	}

	queue->head = NULL;
	queue->tail = NULL;
	queue->count = 0;
	queue->length = 0;
}



/* Prepend a line to the start of the queue */
extern void linequeue_prepend( Linequeue *queue, Line *line )
{
	line->prev= NULL;

	if( !queue->head )
	{
		line->next = NULL;
		queue->head = line;
		queue->tail = line;
	}
	else
	{
		queue->head->prev = line;
		line->next = queue->head;
		queue->head = line;
	}

	queue->count++;
	queue->length += line->len + LINE_BYTE_COST;
}



/* Append a line to the end of the queue */
extern void linequeue_append( Linequeue *queue, Line *line )
{
	line->next = NULL;

	if( !queue->head )
	{
		line->prev = NULL;
		queue->head = line;
		queue->tail = line;
	}
	else
	{
		queue->tail->next = line;
		line->prev = queue->tail;
		queue->tail = line;
	}

	queue->count++;
	queue->length += line->len + LINE_BYTE_COST;
}



/* Return the first line at the beginning of the queue. NULL if no line.
 * The next/prev fields in the returned line are garbage. */
extern Line* linequeue_pop( Linequeue *queue )
{
	Line *line;

	line = queue->head;

	if( !line )
		return NULL;

	queue->head = line->next;

	if( !queue->head )
		queue->tail = NULL;

	queue->count--;
	queue->length -= line->len + LINE_BYTE_COST;

	return line;
}



/* Return the last line at the end of the queue. NULL if no line.
 * The next/prev fields in the returned line are garbage. */
extern Line* linequeue_chop( Linequeue *queue )
{
	Line *line;

	line = queue->tail;

	if( !line )
		return NULL;

	queue->tail = line->prev;

	if( !queue->tail )
		queue->head = NULL;

	queue->count--;
	queue->length -= line->len + LINE_BYTE_COST;

	return line;
}



/* Merge the two queues. The contents of the second queue is appended
 * to the first one, leaving the second one empty. */
extern void linequeue_merge( Linequeue *one, Linequeue *two )
{
	/* If the second list is empty, nop. */
	if( two->head == NULL || two->tail == NULL )
		return;

	/* If the first list is empty, simple copy the references. */
	if( one->head == NULL || one->tail == NULL )
	{
		one->head = two->head;
		one->tail = two->tail;
	}
	else
	{
		one->tail->next = two->head;
		two->head->prev = one->tail;
		one->tail = two->tail;
	}

	one->count += two->count;
	one->length += two->length;

	two->head = NULL;
	two->tail = NULL;
	two->count = 0;
	two->length = 0;
}
