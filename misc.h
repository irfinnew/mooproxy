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



#ifndef MOOPROXY__HEADER__MISC
#define MOOPROXY__HEADER__MISC



#include <sys/types.h>



typedef struct _Line Line;
struct _Line
{
	char *str;
	long len;
	Line *next;
	char store;
};

typedef struct _Linequeue Linequeue;
struct _Linequeue
{
	Line *lines;
	Line **tail;
	ulong count;
	ulong length;
};



/* Exactly like the GNU asprintf() (though different implementation).
 * It is like sprintf, but it allocates its own memory.
 * The memory should be free()d afterwards. */
extern int asprintf( char **, char *, ... );

/* This function gets the users homedir from the environment and returns it.
 * The string is strdup()ped, so it should be free()d. */
extern char *get_homedir( void );

/* Returns the given string, up to but excluding the first newline.
 * The given pointer is set to point to the first character after the
 * newline.
 * When there is nothing left, it returns NULL */
extern char *read_one_line( char ** );

/* Strips all whitespace (determined by isspace()) from a line and returns
 * the modified string */
extern char *strip_all_whitespace( char * );

/* Strips all whitespace (determined by isspace()) in the left and right
 * of the string and returns the modified string */
extern char *trim_whitespace( char * );

/* If the string starts and ends with " or with ', remove both the leading
 * and trailing quote. Return the new string */
extern char *remove_enclosing_quotes( char * );

/* Converts an entire string to lowercase and returns the modified string */
extern char *lowercase( char * );

/* Determines if a string says true or false (case insensitive).
 * If the string is "true", "yes", "on" or "1", it returns 1.
 * If the string is "false", "no", "off" or "0", it returns 0.
 * If te string is unknown, return -1 */
extern int true_or_false( char * );

/* Exactly like strerror(), except for the fucking irritating trailing \n */
extern char *strerror_n( int );

/* Create a line */
extern Line *line_create( char *, long );

/* Allocate and initialize a line queue */
extern Linequeue *linequeue_create( void );

/* De-initialize and free al resources */
extern void linequeue_destroy( Linequeue * );

/* Clear all lines in the queue */
extern void linequeue_clear( Linequeue * );

/* Append a line to the end of the queue */
extern void linequeue_append( Linequeue *, Line * );

/* Return the first line at the beginning of the queue. NULL if no line.
 * The next field in the returned line is garbage. */
extern Line* linequeue_pop( Linequeue * );



#endif  /* ifndef MOOPROXY__HEADER__MISC */
