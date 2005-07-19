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



#include <time.h>

#include "world.h"
#include "timer.h"
#include "log.h"



static void tick_second( World *, time_t );
static void tick_minute( World *, time_t );
static void tick_hour( World *, time_t );
static void tick_day( World *, time_t );
static void tick_month( World *, time_t );
static void tick_year( World *, time_t );



/* Should be called approx. once each second, with the current time
 * as argument. Will handle periodic / scheduled events. */
extern void world_timer_tick( World *wld, time_t t )
{
	struct tm *ts;

	ts = localtime( &t );

	/* Seconds */
	if( wld->timer_prev_sec != -1 && wld->timer_prev_sec != ts->tm_sec )
		tick_second( wld, t );

	/* Minutes */
	if( wld->timer_prev_min != -1 && wld->timer_prev_min != ts->tm_min )
		tick_minute( wld, t );

	/* Hours */
	if( wld->timer_prev_hour != -1 && wld->timer_prev_hour != ts->tm_hour )
		tick_hour( wld, t );

	/* Days */
	if( wld->timer_prev_day != -1 && wld->timer_prev_day != ts->tm_mday )
		tick_day( wld, t );

	/* Months */
	if( wld->timer_prev_mon != -1 && wld->timer_prev_mon != ts->tm_mon )
		tick_month( wld, t );

	/* Years */
	if( wld->timer_prev_year != -1 && wld->timer_prev_year != ts->tm_year )
		tick_year( wld, t );

	wld->timer_prev_sec = ts->tm_sec;
	wld->timer_prev_min = ts->tm_min;
	wld->timer_prev_hour = ts->tm_hour;
	wld->timer_prev_day = ts->tm_mday;
	wld->timer_prev_mon = ts->tm_mon;
	wld->timer_prev_year = ts->tm_year;
}



static void tick_second( World *wld, time_t t )
{
}



static void tick_minute( World *wld, time_t t )
{
}



static void tick_hour( World *wld, time_t t )
{
}



static void tick_day( World *wld, time_t t )
{
	world_log_init( wld );
	world_checkpoint_to_client( wld,
			time_string( t, "Day changed to %A %d %b %Y." ) );
}



static void tick_month( World *wld, time_t t )
{
}



static void tick_year( World *wld, time_t t )
{
	world_message_to_client( wld, time_string( t, "Happy %Y!" ) );
}
