/*
 *
 *  mooproxy - a buffering proxy for moo-connections
 *  Copyright (C) 2001-2006 Marcel L. Moreaux <marcelm@luon.net>
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
#include "misc.h"
#include "global.h"



int panic_clientfd = -1;



static void panicreason_to_string( int reason, char *error, unsigned long extra );



extern void panic_sighandler( int sig )
{
	panic( PANIC_SIGNAL, (unsigned long) sig );
}



extern void panic( int reason, unsigned long extra )
{
	static char *timestamp, error[128], panicstr[256], panicfile[4096];
	static int panicfd;

	/* Create the panicfile filename */
	strcpy( panicfile, getenv( "HOME" ) );
	strcat( panicfile, "/" PANIC_FILE );

	/* Create the timestamp and error message */
	timestamp = time_string( time( NULL ), "%F %T" );
	panicreason_to_string( reason, error, extra );

	/* Construct the panic message */
	sprintf( panicstr, "[%s] Mooproxy crashed: %s!\n", timestamp, error );

	/* Open the panicfile */
	panicfd = open( panicfile, O_WRONLY | O_CREAT | O_APPEND );

	/* Write to stderr */
	write( 2, panicstr, strlen( panicstr ) );
	/* Write to panicfile */
	write( panicfd, panicstr, strlen( panicstr ) );
	/* Write to client */
	write( panic_clientfd, "\r\n", sizeof( "\r\n" ) - 1 );
	write( panic_clientfd, panicstr, strlen( panicstr ) );

	/* Bye! */
	_exit( 0 );
}



static void panicreason_to_string( int reason, char *error, unsigned long extra )
{
	switch( reason )
	{
		case PANIC_SIGNAL:
		switch( extra )
		{
			case SIGSEGV:
			strcpy( error, "Segmentation fault" );
			break;

			case SIGILL:
			strcpy( error, "Illegal instruction" );
			break;

			case SIGFPE:
			strcpy( error, "Floating point exception" );
			break;

			case SIGBUS:
			strcpy( error, "Bus error" );
			break;

			default:
			strcpy( error, "Caught unknown signal" );
			break;
		}
		break;

		case PANIC_MALLOC:
		sprintf( error, "Failed to malloc() %lu bytes", extra );
		break;

		case PANIC_REALLOC:
		sprintf( error, "Failed to realloc() %lu bytes", extra );
		break;

		case PANIC_STRDUP:
		sprintf( error, "Failed to strdup() %lu bytes", extra );
		break;

		case PANIC_VASPRINTF:
		strcpy( error, "vasprintf() failed (lack of memory?)" );
		break;

		default:
		strcpy( error, "Unknown error" );
		break;
	}
}
