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



#ifndef MOOPROXY__HEADER__DAEMON
#define MOOPROXY__HEADER__DAEMON



#include <time.h>



/* Set up the signal handlers. Duh :) */
extern void set_up_signal_handlers( void );

/* Set the time at which mooproxy started, to determine the uptime. */
extern void uptime_started_now( void );

/* Get the time at which mooproxy started. */
extern time_t uptime_started_at( void );



#endif  /* ifndef MOOPROXY__HEADER_DAEMON */
