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

#include "misc.h"



#define MAX_STRERROR_LEN 128



static char strerror_buf[MAX_STRERROR_LEN];



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
 * The string is strdup()ped, so it should be free()d. */
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
 * When there is nothing left, it returns NULL */
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

	return realloc( line, t ? t : 1 );
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

	return realloc( line, ++t );
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
	size_t i, l = strlen( str );

	for( i = 0; i < l; i++ )
		str[i] = tolower( str[i] );

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



/* Create a line */
extern Line *line_create( char *str, long len )
{
	Line *line;

	line = malloc( sizeof( Line ) );
	line->str = str;
	line->len = len == -1 ? strlen( str ) : len;
	line->store = 0;

	return line;
}



/* Allocate and initialize a line queue */
extern Linequeue *linequeue_create( void )
{
	Linequeue *queue;

	queue = malloc( sizeof( Linequeue ) );

	queue->lines = NULL;
	queue->tail = &( queue->lines );
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

	for( line = queue->lines; line; line = next )
	{
		next = line->next;
		free( line->str );
		free( line );
	}

	queue->lines = NULL;
	queue->tail = &( queue->lines );
	queue->count = 0;
	queue->length = 0;
}



/* Append a line to the end of the queue */
extern void linequeue_append( Linequeue *queue, Line *line )
{
	line->next = NULL;

	*( queue->tail ) = line;
	queue->tail = &( line->next );

	queue->count++;
	queue->length += line->len;
}



/* Return the first line at the beginning of the queue. NULL if no line.
 * The next field in the returned line is garbage. */
extern Line* linequeue_pop( Linequeue *queue )
{
	Line *line;

	line = queue->lines;

	if( !line )
		return NULL;

	queue->lines = line->next;

	if( !queue->lines )
		queue->tail = &( queue->lines );

	queue->count--;
	queue->length -= line->len;

	return line;
}
