/*
 *
 *  mooproxy - a smart proxy for MUD/MOO connections
 *  Copyright 2001-2011 Marcel Moreaux
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



#ifndef MOOPROXY__HEADER__TIMER
#define MOOPROXY__HEADER__TIMER



#include <time.h>



/* Update the current time administration to the supplied time, but do not
 * run periodic or scheduled events. This function should be called at least
 * once before calling world_timer_tick(). */
extern void world_timer_init( World *wld, time_t t );

/* Update current time administration, and execute periodic or scheduled
 * events. This function should be called regularly (preferably once each
 * second). t should be the current time as reported by time() */
extern void world_timer_tick( World *wld, time_t t );



#endif  /* ifndef MOOPROXY__HEADER__TIMER */
