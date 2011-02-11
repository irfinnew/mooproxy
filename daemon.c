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



#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/file.h>
#include <ctype.h>
#include <sys/time.h>

#include "daemon.h"
#include "misc.h"
#include "panic.h"



/* Can hold a 128 bit number in decimal. Should be enough for pids. */
#define MAX_PIDDIGITS 42



static time_t program_start_time = 0;



static char *pid_from_pidfile( int fd );
extern void sighandler_sigterm( int sig );



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

	/* We're not interested in user signals. */
	signal( SIGUSR1, SIG_IGN );
	signal( SIGUSR2, SIG_IGN );

	/* 'Bad' signals, indicating a mooproxy bug. Panic. */
	signal( SIGSEGV, &sighandler_panic );
	signal( SIGILL, &sighandler_panic );
	signal( SIGFPE, &sighandler_panic );
	signal( SIGBUS, &sighandler_panic );

	/* Handle SIGTERM (shutdown) and SIGQUIT (shutdown -f). */
	signal( SIGTERM, &sighandler_sigterm );
	signal( SIGQUIT, &sighandler_sigterm );
}



extern void uptime_started_now( void )
{
	struct timeval tv;

	program_start_time = time( NULL );

	gettimeofday( &tv, NULL );
	srand( tv.tv_usec );
}



extern time_t uptime_started_at( void )
{
	return program_start_time;
}



extern pid_t daemonize( char **err )
{
	pid_t pid;
	int devnull;

	/* We need /dev/null, so we can redirect stdio to it.
	 * Open it now, so we can complain before the child is created. */
	devnull = open( "/dev/null", O_RDWR );
	if( devnull == -1 )
	{
		xasprintf( err, "Opening /dev/null failed: %s",
				strerror( errno ) );
		return -1;
	}

	/* Fork the child... */
	pid = fork();
	if( pid == -1 )
	{
		xasprintf( err, "Fork failed: %s", strerror( errno ) );
		return -1;
	}

	/* If we're the parent, we're done now. */
	if( pid > 0 )
		return pid;

	/* Become a session leader. */
	setsid();

	/* Don't tie up the path we're launched from. */
	chdir( "/" );

	/* Redirect stdio to /dev/null. */
	dup2( devnull, 0 );
	dup2( devnull, 1 );
	dup2( devnull, 2 );
	close( devnull );

	return 0;
}



extern void launch_parent_exit( int exitval )
{
	exit( exitval );
}



extern int world_acquire_lock_file( World *wld, char **err )
{
	char *lockfile, *pidstr;
	int fd, ret;

	/* Lockfile = ~/CONFIGDIR/LOCKSDIR/worldname */
	xasprintf( &lockfile, "%s/%s/%s/%s", get_homedir(), CONFIGDIR,
			LOCKSDIR, wld->name );

	/* Open lockfile. Should be non-blocking; read and write actions
	 * are best-effort. */
	fd = open( lockfile, O_RDWR | O_CREAT | O_NONBLOCK, S_IRUSR | S_IWUSR );
	if( fd < 0 )
	{
		xasprintf( err, "Error opening lockfile %s: %s", lockfile,
				strerror( errno ) );
		free( lockfile );
		return 1;
	}

	/* Acquire a lock on the file. */
	ret = flock( fd, LOCK_EX | LOCK_NB );

	/* Some other process already has a lock. We assume it's another
	 * instance of mooproxy. */
	if( ret < 0 && errno == EWOULDBLOCK )
	{
		pidstr = pid_from_pidfile( fd );
		xasprintf( err, "Mooproxy instance for world %s already "
				"running (PID %s).", wld->name, pidstr );
		free( lockfile );
		free( pidstr );
		close( fd );
		return 1;
	}

	/* Some other error. */
	if( ret < 0 )
	{
		xasprintf( err, "Error locking lockfile %s: %s", lockfile,
				strerror( errno ) );
		free( lockfile );
		close( fd );
		return 1;
	}

	/* Everything appears OK. Store info, return. */
	wld->lockfile = lockfile;
	wld->lockfile_fd = fd;
	return 0;
}



static char *pid_from_pidfile( int fd )
{
	char *pidstr;
	int len, i;

	pidstr = xmalloc( MAX_PIDDIGITS + 1 );
	len = read( fd, pidstr, MAX_PIDDIGITS + 1 );

	/* If len < 0, there was an error. If len == 0, the file was empty. */
	if( len < 1 || len > MAX_PIDDIGITS )
	{
		free( pidstr );
		return xstrdup( "unknown" );
	}

	/* NUL-terminate the string. */
	pidstr[len] = '\0';

	/* Strip any trailing \n */
	if( pidstr[len - 1] == '\n' )
		pidstr[--len] = '\0';

	/* Any non-digits in the string? Bail out. */
	for( i = 0; i < len; i++ )
		if( !isdigit( pidstr[i] ) )
		{
			free( pidstr );
			return xstrdup( "unknown" );
		}

	return pidstr;
}



extern void world_write_pid_to_file( World *wld, pid_t pid )
{
	char *pidstr;

	xasprintf( &pidstr, "%li\n", (long) pid );

	ftruncate( wld->lockfile_fd, 0 );
	write( wld->lockfile_fd, pidstr, strlen( pidstr ) );

	free( pidstr );
}



extern void world_remove_lockfile( World *wld )
{
	close( wld->lockfile_fd );
	wld->lockfile_fd = -1;

	unlink( wld->lockfile );
	free( wld->lockfile );
	wld->lockfile = NULL;
}



extern void sighandler_sigterm( int signum )
{
	World **wldlist;
	int wldcount;

	world_get_list( &wldcount, &wldlist );
	world_start_shutdown( wldlist[0], signum == SIGQUIT, 1 );
}
