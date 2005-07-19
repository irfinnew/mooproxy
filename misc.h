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



#define LINE_DONTLOG 0x01
#define LINE_DONTBUF 0x02
#define LINE_NOHIST 0x04

#define LINE_REGULAR ( 0 )
#define LINE_MCP ( LINE_DONTLOG | LINE_DONTBUF | LINE_NOHIST )
#define LINE_CHECKPOINT ( 0 )
#define LINE_MESSAGE ( LINE_DONTLOG | LINE_NOHIST )
#define LINE_BUFFERED ( LINE_DONTLOG )
#define LINE_HISTORY ( LINE_DONTLOG | LINE_DONTBUF | LINE_NOHIST )



typedef struct _Line Line;
struct _Line
{
	char *str;
	long len;
	Line *next;
	Line *prev;
	int flags;
};

typedef struct _Linequeue Linequeue;
struct _Linequeue
{
	Line *head;
	Line *tail;
	ulong count;
	ulong length;
};



/* Exactly like the GNU asprintf() (though different implementation).
 * It is like sprintf, but it allocates its own memory.
 * The memory should be free()d afterwards. */
extern int asprintf( char **, char *, ... );

/* This function gets the users homedir from the environment and returns it.
 * The string is strdup()ped, so it should be free()d.
 * The string will either be empty, or /-terminated */
extern char *get_homedir( void );

/* Returns the given string, up to but excluding the first newline.
 * The given pointer is set to point to the first character after the
 * newline.
 * When there is nothing left, it returns NULL
 * The returned lines are separately allocated. */
extern char *read_one_line( char ** );

/* Returns the given string, up to but excluding the first whitespace char.
 * The given pointer is set to point to the first character after the
 * whitespace character.
 * When there is nothing left, it returns NULL
 * The string pointed to is mangled, returned words are still inside. */
extern char *get_one_word( char ** );

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
 * If the string is unknown, return -1 */
extern int true_or_false( char * );

/* Exactly like strerror(), except for the fucking irritating trailing \n */
extern char *strerror_n( int );

/* Convert the given time to a formatted string in local time.
 * For the format, see strftime(3) .
 * It returns a pointer to a statically allocated buffer. */
extern char *time_string( time_t, char * );

/* Create a line, flags are clear, prev/next are NULL.
 * If length is -1, it's calculated. */
extern Line *line_create( char *, long );

/* Destroy a line, freeing its resources */
extern void line_destroy( Line * );

/* Return a duplicate of the line (prev/next are NULL). */
extern Line *line_dup( Line * );

/* Allocate and initialize a line queue */
extern Linequeue *linequeue_create( void );

/* De-initialize and free al resources */
extern void linequeue_destroy( Linequeue * );

/* Clear all lines in the queue */
extern void linequeue_clear( Linequeue * );

/* Prepend a line to the start of the queue */
extern void linequeue_prepend( Linequeue *, Line * );

/* Append a line to the end of the queue */
extern void linequeue_append( Linequeue *, Line * );

/* Return the first line at the beginning of the queue. NULL if no line.
 * The next/prev fields in the returned line are garbage. */
extern Line* linequeue_pop( Linequeue * );

/* Return the last line at the end of the queue. NULL if no line.
 * The next/prev fields in the returned line are garbage. */
extern Line* linequeue_chop( Linequeue * );

/* Merge the two queues. The contents of the second queue is appended
 * to the first one, leaving the second one empty. */
extern void linequeue_merge( Linequeue *, Linequeue * );



#endif  /* ifndef MOOPROXY__HEADER__MISC */
