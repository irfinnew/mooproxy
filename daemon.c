/*
 *
 *  mooproxy - a buffering proxy for moo-connections
 *  Copyright (C) 2001-2005 Marcel L. Moreaux <marcelm@luon.net>
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
#include <signal.h>
#include <string.h>
#include <time.h>

#include "daemon.h"



static time_t program_start_time = 0;



extern void set_up_signal_handlers( void )
{
	/* This signal might be sent if a connection breaks down.
	 * We handle that with select() and read(). */
	signal( SIGPIPE, SIG_IGN );

	/* This signal might be sent if someone sends us OOB TCP messages.
	 * We're not interested. */
	signal( SIGURG, SIG_IGN );

	/* This signal might be sent if the controlling terminal exits.
	 * We want to continue running in that case. */
	signal( SIGHUP, SIG_IGN );

	/* This signal might be sent if the resolver slave exits.
	 * We already know that through IPC. */
	signal( SIGCHLD, SIG_IGN );
}



extern void uptime_started_now( void )
{
	program_start_time = time( NULL );
}



extern time_t uptime_started_at( void )
{
	return program_start_time;
}
