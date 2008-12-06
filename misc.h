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



#ifndef MOOPROXY__HEADER__MISC
#define MOOPROXY__HEADER__MISC



#include <stdarg.h>
#include <sys/types.h>
#include <time.h>

#include "line.h"



/* Maximum length of the string to be returned by time_string() */
#define TIMESTR_MAXLEN 256
/* Maximum number of time_string() results that may be used simultaneously.
 * Additional calls to time_string() will overwrite older returned strings. */
#define TIMESTR_MAXSIMULT 4



/* Wrappers for malloc(), realloc(), strdup(), asprintf(), and vasprintf() 
 * These wrappers check for and abort on memory allocation failure.
 * Otherwise, they behave identical to the original functions. */
extern void *xmalloc( size_t size );
extern void *xrealloc( void *ptr, size_t size );
extern char *xstrdup( const char *s );
extern int xasprintf( char **strp, const char *fmt, ... );
extern int xvasprintf( char **strp, const char *fmt, va_list argp );

/* Set the current time which can later be queried by current_time().
 * The time is used e.g. to timestamp newly created lines. */
extern void set_current_time( time_t t );

/* Get the current time, as set by set_current_time(). */
extern time_t current_time( void );

/* Set the current day which can later be queried by current_day().
 * The time is used to timestamp lines for logging. */
extern void set_current_day( long day );

/* Get the current day, as set by set_current_day(). */
extern long current_day( void );

/* Returns a string containing the users homedir.
 * The string should not be freed or manipulated. */
extern char *get_homedir( void );

/* Extract the first line from str.
 * Returns a pointer to a copy of the first line of str, excluding the
 * newline character. The copy should be free()d.
 * If there are no lines left in str, the function returns NULL.
 * str is modified to point to the next line. */
extern char *read_one_line( char **str );

/* Extract the first word from str.
 * Returns a pointer to the first word of str, excluding any whitespace
 * characters. The pointer points to the word inside the (mangled) str,
 * it should not be free()d separately.
 * If there are no words left in str, the function returns NULL.
 * str is modified to point to the first character after the first whitespace
 * character after the first word. */
extern char *get_one_word( char **str );

/* Extract the first word from str.
 * Returns the first word in freshly allocated memory, which should be free()d.
 * If there are no words left in str, the function returns NULL.
 * str is not modified. */
extern char *peek_one_word( char *str );

/* Strips all whitespace from the start and end of line.
 * Returns line. Modifies line in place. */
extern char *trim_whitespace( char *line );

/* If str starts and ends with ", or starts and ends with ', modify str
 * to remove leading and trailing quote. Returns str. */
extern char *remove_enclosing_quotes( char *str );

/* Determines if str says true or false (case insensitive).
 * If the string is "true", "yes", "on", or "1", return 1.
 * If the string is "false", "no", "off", or "0", return 0.
 * If the string is not recognized, return -1. */
extern int true_or_false( const char *str );

/* Convert the time in t to a formatted string in local time.
 * For the format, see strftime (3).
 * Returns a pointer to a statically allocated buffer.
 * The buffer should not be free()d, and will be overwritten by subsequent
 * later calls to time_string().
 * See the TIMESTR_ defines earlier in this file. */
extern char *time_string( time_t t, const char *fmt );

/* Process buffer into lines. Start scanning at offset, scan only read bytes.
 * The salvaged lines are appended to q. Return the new offset. */
extern int buffer_to_lines( char *buffer, int offset, int read, Linequeue *q );

/* Process the given buffer/queue, and write it to the given fd.
 * Arguments:
 *   fd:         FD to write to.
 *   buffer:     Buffer to use for writing to the FD.
 *   bffl:       Indicates how full the buffer is. Will be updated.
 *   queue:      Queue of lines to be written.
 *   tohist:     Queue to append written lines to.
 *               If queue is NULL, written lines are discarded.
 *   network_nl: If true:  appends network newlines to written lines.
 *               If false: appends UNIX newlines to written lines.
 *   prestr:     Prepended to each line, if not NULL.
 *   poststr:    Appended to every line, if not NULL.
 *   errnum:     Modified to contain error code on error.
 * Return value:
 *   0 on success (everything in buffer and queue was written without error)
 *   1 on congestion (the FD could take no more, not everything was written)
 *   2 on error (errnum is set) */
extern int flush_buffer( int fd, char *buffer, long *bffl, Linequeue *queue,
		Linequeue *tohist, int network_nl, char *prestr, char *poststr,
		int *errnum );

/* Return a copy of str (which must be freed manually) in which all occurrences
 * of colortags (like %R) are replaced by their corresponding ANSI sequence
 * (like ^[[1,31m). */
extern char *parse_ansi_tags( char *str );

/* Attempts to create dirname (but not parent dirs).
 * On success, return 0.
 * On failure, return non-zero, and put error in err (err should be freed).
 * Err will contain just a brief error message (often strerror output),
 * more formatting should be done by the caller. */
extern int attempt_createdir( char *dirname, char **err );

/* Copy a character string from src to dest, like strcpy(), but skip any
 * ANSI sequences (such as color, bold, etc) and other unprintable characters
 * (control characters, ASCII BELL, etc).
 * dest must point to a large enough buffer.
 * Returns the length of the new string (excluding terminating \0). */
extern long strcpy_noansi( char *dest, char *src );

/* Copy a character string from src to dest, like strcpy(), but skip any
 * ASCII BELL (0x07) characters. dest must point to a large enough buffer.
 * Returns the length of the new string (excluding terminating \0). */
extern long strcpy_nobell( char *dest, char *src );



#endif  /* ifndef MOOPROXY__HEADER__MISC */
