/*
 *
 *  mooproxy - a buffering proxy for MOO connections
 *  Copyright (C) 2001-2011 Marcel L. Moreaux <marcelm@luon.net>
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

#include "world.h"
#include "timer.h"
#include "log.h"
#include "misc.h"
#include "network.h"



static void tick_second( World *, time_t );
static void tick_minute( World *, time_t );
static void tick_hour( World *, time_t );
static void tick_day( World *, time_t, long );
static void tick_month( World *, time_t );
static void tick_year( World *, time_t );



extern void world_timer_init( World *wld, time_t t )
{
	struct tm *ts;

	ts = localtime( &t );

	wld->timer_prev_sec = ts->tm_sec;
	wld->timer_prev_min = ts->tm_min;
	wld->timer_prev_hour = ts->tm_hour;
	wld->timer_prev_day = ts->tm_year * 366 + ts->tm_yday;
	wld->timer_prev_mon = ts->tm_mon;
	wld->timer_prev_year = ts->tm_year;
}



extern void world_timer_tick( World *wld, time_t t )
{
	struct tm *ts;

	ts = localtime( &t );

	/* Seconds */
	if( wld->timer_prev_sec != ts->tm_sec )
		tick_second( wld, t );

	/* Minutes */
	if( wld->timer_prev_min != ts->tm_min )
		tick_minute( wld, t );

	/* Hours */
	if( wld->timer_prev_hour != ts->tm_hour )
		tick_hour( wld, t );

	/* Days */
	if( wld->timer_prev_day != ts->tm_year * 366 + ts->tm_yday )
		tick_day( wld, t, ts->tm_year * 366 + ts->tm_yday );

	/* Months */
	if( wld->timer_prev_mon != ts->tm_mon )
		tick_month( wld, t );

	/* Years */
	if( wld->timer_prev_year != ts->tm_year )
		tick_year( wld, t );

	wld->timer_prev_sec = ts->tm_sec;
	wld->timer_prev_min = ts->tm_min;
	wld->timer_prev_hour = ts->tm_hour;
	wld->timer_prev_day = ts->tm_year * 366 + ts->tm_yday;
	wld->timer_prev_mon = ts->tm_mon;
	wld->timer_prev_year = ts->tm_year;
}



/* Called each time a second elapses. */
static void tick_second( World *wld, time_t t )
{
	set_current_time( t );

	/* Add some tokens to the auth token bucket. */
	world_auth_add_bucket( wld );

	/* If we're waiting for a reconnect, and now is the time, do it. */
	if( wld->server_status == ST_RECONNECTWAIT && wld->reconnect_at <= t )
		world_do_reconnect( wld );
}



/* Called each time a minute elapses. */
static void tick_minute( World *wld, time_t t )
{
	/* Try and sync written logdata to disk. The sooner it hits the
	 * actual disk, the better. */
	world_sync_logdata( wld );

	/* If we're connected, decrease the reconnect delay every minute. */
	if( wld->reconnect_delay != 0 && wld->server_status == ST_CONNECTED )
		world_decrease_reconnect_delay( wld );
}



/* Called each time an hour elapses. */
static void tick_hour( World *wld, time_t t )
{
}



/* Called each time a day elapses. */
static void tick_day( World *wld, time_t t, long day )
{
	Line *line;

	set_current_day( day );

	line = world_msg_client( wld, "%s",
			time_string( t, "Day changed to %A %d %b %Y." ) );
	line->flags = LINE_CHECKPOINT;

	wld->flags |= WLD_LOGLINKUPDATE;
}



/* Called each time a month elapses. */
static void tick_month( World *wld, time_t t )
{
}



/* Called each time a year elapses. */
static void tick_year( World *wld, time_t t )
{
	Line *line;

	line = world_msg_client( wld, "%s", time_string( t, "Happy %Y!" ) );
	line->flags = LINE_CHECKPOINT;
}
