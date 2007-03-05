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



#include <stdlib.h>
#include <string.h>

#include "line.h"
#include "misc.h"



/* The estimated memory cost of a line object: the size of the object itself,
 * and some random guess at memory management overhead */
#define LINE_BYTE_COST ( sizeof( Line ) + sizeof( void * ) * 2 + 8 )



extern Line *line_create( char *str, long len )
{
	Line *line;

	line = xmalloc( sizeof( Line ) );
	line->str = str;
	line->len = ( len == -1 ) ? strlen( str ) : len;
	line->flags = LINE_REGULAR;
	line->prev = NULL;
	line->next = NULL;
	line->time = current_time();
	line->day = current_day();

	return line;
}



extern void line_destroy( Line *line )
{
	if( line )
		free( line->str );
	free( line );
}



extern Line *line_dup( Line *line )
{
	Line *newline;

	newline = xmalloc( sizeof( Line ) );
	newline->str = xmalloc( line->len + 1 );
	strcpy( newline->str, line->str );
	newline->len = line->len;
	newline->flags = line->flags;
	newline->time = line->time;
	newline->day = line->day;

	newline->prev = NULL;
	newline->next = NULL;

	return newline;
}



extern Linequeue *linequeue_create( void )
{
	Linequeue *queue;

	queue = xmalloc( sizeof( Linequeue ) );

	queue->head= NULL;
	queue->tail = NULL;
	queue->count = 0;
	queue->length = 0;

	return queue;
}



extern void linequeue_destroy( Linequeue *queue )
{
	linequeue_clear( queue );

	free( queue );
}



extern void linequeue_clear( Linequeue *queue )
{
	Line *line, *next;

	for( line = queue->head; line != NULL; line = next )
	{
		next = line->next;
		line_destroy( line );
	}

	queue->head = NULL;
	queue->tail = NULL;
	queue->count = 0;
	queue->length = 0;
}



extern void linequeue_append( Linequeue *queue, Line *line )
{
	line->next = NULL;

	if( !queue->head )
	{
		/* Only line in queue, there is no previous, and head should
		 * point to this line, too. */
		line->prev = NULL;
		queue->head = line;
	}
	else
	{
		/* There are other lines, last line's next should point
		 * to the new line, and new line's prev should point to
		 * previous tail. */
		queue->tail->next = line;
		line->prev = queue->tail;
	}

	queue->tail = line;

	queue->count++;
	queue->length += line->len + LINE_BYTE_COST;
}



extern Line* linequeue_pop( Linequeue *queue )
{
	Line *line;

	line = queue->head;

	if( !line )
		return NULL;

	/* The next line becomes the head. */
	queue->head = line->next;

	/* Did we just pop the last line? Tail should be NULL too! */
	if( !queue->head )
		queue->tail = NULL;
	else
		/* The new head has no previous. */
		queue->head->prev = NULL;

	queue->count--;
	queue->length -= line->len + LINE_BYTE_COST;

	return line;
}



extern Line* linequeue_popend( Linequeue *queue )
{
	Line *line;

	line = queue->tail;

	if( !line )
		return NULL;

	/* The previous line becomes the tail. */
	queue->tail = line->prev;

	/* Did we just pop the first line? Head should be NULL too! */
	if( !queue->tail )
		queue->head = NULL;
	else
		/* The new tail has no next. */
		queue->tail->next = NULL;

	queue->count--;
	queue->length -= line->len + LINE_BYTE_COST;

	return line;
}



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

	/* Two is now empty. */
	two->head = NULL;
	two->tail = NULL;
	two->count = 0;
	two->length = 0;
}
