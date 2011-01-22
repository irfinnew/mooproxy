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



#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "panic.h"
#include "world.h"
#include "misc.h"
#include "global.h"



static void panicreason_to_string( char *str, int reason, long extra,
		unsigned long uextra );
static void worldlist_to_string( char *str, int wldcount, World **worlds );



extern void sighandler_panic( int sig )
{
	panic( PANIC_SIGNAL, sig, 0 );
}



extern void panic( int reason, long extra, unsigned long uextra)
{
	char *timestamp, panicstr[1024], panicfile[4096];
	int panicfd, wldcount, i;
	World **worlds;

	/* First of all, set the signal handlers to default actions.
	 * We don't want to cause an endless loop by SEGVing in here. */
	signal( SIGSEGV, SIG_DFL );
	signal( SIGILL, SIG_DFL );
	signal( SIGFPE, SIG_DFL );
	signal( SIGBUS, SIG_DFL );

	/* Get list of worlds */
	world_get_list( &wldcount, &worlds );

	/* Create the panicfile */
	strcpy( panicfile, getenv( "HOME" ) );
	strcat( panicfile, "/" PANIC_FILE );
	panicfd = open( panicfile, O_WRONLY | O_CREAT | O_APPEND,
			S_IRUSR | S_IWUSR );

	/* Create the timestamp and panic message */
	timestamp = time_string( time( NULL ), "%F %T" );
	sprintf( panicstr, "%s mooproxy (PID %li) crashed!\n",
			timestamp, (long) getpid() );

	/* Write it to all destinations */ 
	write( STDERR_FILENO, panicstr, strlen( panicstr ) );
	write( panicfd, panicstr, strlen( panicstr ) );
	for( i = 0; i < wldcount; i++ )
	{
		if( worlds[i]->client_fd < 0 )
			continue;
		write( worlds[i]->client_fd, "\r\n", sizeof( "\r\n" ) - 1 );
		write( worlds[i]->client_fd, panicstr, strlen( panicstr ) );
	}


	/* Formulate the reason of the crash */
	panicreason_to_string( panicstr, reason, extra, uextra );

	/* And write it to all destinations */
	write( STDERR_FILENO, panicstr, strlen( panicstr ) );
	write( panicfd, panicstr, strlen( panicstr ) );
	for( i = 0; i < wldcount; i++ )
	{
		if( worlds[i]->client_fd < 0 )
			continue;
		write( worlds[i]->client_fd, panicstr, strlen( panicstr ) );
	}


	/* Formulate the list of worlds */
	worldlist_to_string( panicstr, wldcount, worlds );

	/* And write it to all destinations */
	write( STDERR_FILENO, panicstr, strlen( panicstr ) );
	write( panicfd, panicstr, strlen( panicstr ) );
	for( i = 0; i < wldcount; i++ )
	{
		if( worlds[i]->client_fd < 0 )
			continue;
		write( worlds[i]->client_fd, panicstr, strlen( panicstr ) );
	}


	/* Bail out */
	_exit( EXIT_FAILURE );
}



static void panicreason_to_string( char *str, int reason, long extra,
		unsigned long uextra )
{
	/* Make the string begin with two spaces */
	strcpy( str, "  " );
	str += 2;

	switch( reason )
	{
		case PANIC_SIGNAL:
		switch( extra )
		{
			case SIGSEGV:
			strcpy( str, "Segmentation fault" );
			break;

			case SIGILL:
			strcpy( str, "Illegal instruction" );
			break;

			case SIGFPE:
			strcpy( str, "Floating point exception" );
			break;

			case SIGBUS:
			strcpy( str, "Bus error" );
			break;

			default:
			strcpy( str, "Caught unknown signal" );
			break;
		}
		break;

		case PANIC_MALLOC:
		sprintf( str, "Failed to malloc() %lu bytes", uextra );
		break;

		case PANIC_REALLOC:
		sprintf( str, "Failed to realloc() %lu bytes", uextra );
		break;

		case PANIC_STRDUP:
		sprintf( str, "Failed to strdup() %lu bytes", uextra );
		break;

		case PANIC_STRNDUP:
		sprintf( str, "Failed to strndup() %lu bytes", uextra );
		break;

		case PANIC_VASPRINTF:
		strcpy( str, "vasprintf() failed (lack of memory?)" );
		break;

		case PANIC_SELECT:
		sprintf( str, "select() failed: %s", strerror( extra ) );
		break;

		case PANIC_ACCEPT:
		sprintf( str, "accept() failed: %s", strerror( extra ) );
		break;

		default:
		strcpy( str, "Unknown error" );
		break;
	}

	strcat( str, "\n" );
}



static void worldlist_to_string( char *str, int wldcount, World **worlds )
{
	int i;

	strcpy( str, "  open worlds:" );

	for( i = 0; i < wldcount; i++ )
	{
		strcat( str, " [" );
		strcat( str, worlds[i]->name );
		strcat( str, "]" );
	}

	strcat( str, "\n" );
}
