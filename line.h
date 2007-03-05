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



#ifndef MOOPROXY__HEADER__LINE
#define MOOPROXY__HEADER__LINE



#include <time.h>



/* Line flags */
#define LINE_DONTLOG 0x00000001 /* Don't log the line. */
#define LINE_DONTBUF 0x00000002 /* Don't buffer the line. */
#define LINE_NOHIST  0x00000004 /* Don't put the line in history. */

/* Regular server->client or client->server lines. */
#define LINE_REGULAR ( 0 )
/* MCP lines. */
#define LINE_MCP ( LINE_DONTLOG | LINE_DONTBUF | LINE_NOHIST )
/* Mooproxy checkpoint message (e.g. day rollover). */
#define LINE_CHECKPOINT ( 0 )
/* Mooproxy normal message (e.g. /listopts output). */
#define LINE_MESSAGE ( LINE_DONTLOG | LINE_NOHIST )



/* Line type */
typedef struct _Line Line;
struct _Line
{
	char *str;
	long len;
	Line *next;
	Line *prev;
	int flags;
	/* Time of the line's creation. */
	time_t time;
	/* Day of the line's creation. Used in logging. */
	long day;
};

/* Linequeue type */
typedef struct _Linequeue Linequeue;
struct _Linequeue
{
	Line *head;
	Line *tail;
	/* Number if lines in this queue. */
	unsigned long count;
	/* Total number of bytes this queue occupies. */
	unsigned long length;
};



/* Create a line, set it's flags to LINE_REGULAR, and prev/next to NULL.
 * Time and day are set to current time/day.
 * len should contain the length of str (excluding \0), or -1, in which case
 * line_create() will calculate the length itself.
 * Returns the new line. */
extern Line *line_create( char *str, long len );

/* Destroy line, freeing its resources. */
extern void line_destroy( Line *line );

/* Duplicate line (and its string). All fields are equivalent, except for
 * prev and next, which are set to NULL. Returns the new line. */
extern Line *line_dup( Line *line );

/* Allocate and initialize a line queue. The queue is empty.
 * Return value: the new queue. */
extern Linequeue *linequeue_create( void );

/* Destroy queue, freeing its resources.
 * Any strings left in the queue are destroyed too. */
extern void linequeue_destroy( Linequeue *queue );

/* Destroy all lines in queue. */
extern void linequeue_clear( Linequeue *queue );

/* Append line to the end of queue */
extern void linequeue_append( Linequeue *line, Line *queue );

/* Return the first line at the beginning of the queue.
 * If queue is empty, it returns NULL.
 * The next/prev fields of the returned line should be considered garbage. */
extern Line* linequeue_pop( Linequeue *queue );

/* Return the last line at the end of the queue.
 * If queue is empty, it returns NULL.
 * The next/prev fields of the returned line should be considered garbage. */
extern Line* linequeue_popend( Linequeue *queue );

/* Merge two queues. The contents of queue two is appended to queue one,
 * leaving the second one empty. */
extern void linequeue_merge( Linequeue *one, Linequeue *two );



#endif  /* ifndef MOOPROXY__HEADER__LINE */
