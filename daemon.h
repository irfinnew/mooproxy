/*
 *
 *  mooproxy - a buffering proxy for MOO connections
 *  Copyright (C) 2001-2011 Marcel L. Moreaux <marcelm@qvdr.net>
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



#include <sys/types.h>
#include <time.h>

#include "world.h"



/* Set up the signal handlers. Duh :) */
extern void set_up_signal_handlers( void );

/* Set the time at which mooproxy started, to determine the uptime.
 * This function also seeds the PRNG using the useconds from gettimeofday(). */
extern void uptime_started_now( void );

/* Get the time at which mooproxy started. */
extern time_t uptime_started_at( void );

/* Split off daemon, so we can run in the background.
 * In summary: fork, setsid, fork again, chdir, and redirect stdin, stdout
 * and stderr to /dev/zero. */
extern pid_t daemonize( char **err );

/* The exit function for the launching parent, after the daemonize step
 * is complete. */
extern void launch_parent_exit( int exitval );

/* Attempt to lock this worlds lockfile (creating it if it doesn't exist).
 * If the file could not be created or locked, err will contain an error
 * message, and non-zero is returned. On success, the lockfile FD is placed
 * in world->lockfile_fd, and zero is returned. */
extern int world_acquire_lock_file( World *wld, char **err );

/* Write the PID to the pidfile (best effort, won't throw error) */
extern void world_write_pid_to_file( World *wld, pid_t pid );

/* Closes the lockfile, and remove it. */
extern void world_remove_lockfile( World *wld );



#endif  /* ifndef MOOPROXY__HEADER__DAEMON */
