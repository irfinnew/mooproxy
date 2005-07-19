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
#include <signal.h>
#include <string.h>
#include <time.h>

#include "daemon.h"



static time_t program_start_time = 0;



/* Set up the signal handlers. Duh :) */
extern void set_up_signal_handlers( void )
{
	/* According to valgrind, libc6 fucks something up here */
	signal( SIGPIPE, SIG_IGN );
	signal( SIGHUP, SIG_IGN );
}



/* Set the time at which mooproxy started, to determine the uptime */
extern void uptime_started_now( void )
{
	program_start_time = time( NULL );
}



/* Get the time at which mooproxy started. */
extern time_t uptime_started_at( void )
{
	return program_start_time;
}
