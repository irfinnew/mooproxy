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



#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include "resolve.h"
#include "world.h"
#include "misc.h"
#include "network.h"



#define RESOLVE_SUCCESS 'a'
#define RESOLVE_ERROR 'b'



static void slave_main( World *, int );
static void slave_msg_to_parent( char *, int );



extern void world_start_server_resolve( World *wld )
{
	int filedes[2];
	pid_t pid;

	/* If we are already resolving, ignore the request. */
	if( wld->server_status != ST_DISCONNECTED )
		return;

	/* Attempt to create the comminucation pipe */
	if( pipe( filedes ) < 0 )
	{
		world_msg_client( wld, "Could not create pipe: %s",
				strerror( errno ) );
		return;
	}

	/* Fork off the slave */
	pid = fork();
	if( pid == -1 )
	{
		world_msg_client( wld, "Could not create resolver slave: %s",
				strerror( errno ) );
		return;
	}

	/* We're the slave, go do our stuff */
	if( pid == 0 )
	{
		close( filedes[0] );
		slave_main( wld, filedes[1] );
		exit( 0 );
	}

	/* We're the parent. */
	close( filedes[1] );
	wld->server_resolver_fd = filedes[0];
	wld->server_resolver_pid = pid;
	wld->server_status = ST_RESOLVING;
}



extern void world_cancel_server_resolve( World *wld )
{
	/* If we aren't resolving, ignore the request */
	if( wld->server_status != ST_RESOLVING )
		return;

	/* Just kill the slave and clean up. */
	kill( wld->server_resolver_pid, SIGKILL );
	waitpid( wld->server_resolver_pid, NULL, 0 );

	close( wld->server_resolver_fd );

	wld->server_resolver_pid = -1;
	wld->server_resolver_fd = -1;
	wld->server_status = ST_DISCONNECTED;
}



extern void world_handle_resolver_fd( World *wld )
{
	char *addresses = NULL;
	int len = 0, i = 1;

	/* Read from the pipe. We should be able to read everything quickly.
	 * wld->server_resolver_fd is blocking, so when there's more data
	 * than fitted in the kernel socket buffers, the read() will block,
	 * but the slave's write() becomes unblocked, so the rest of the
	 * data gets transferred.
	 * If the resolver slave should write something to the socket but
	 * not close it, we'll hang here indefinetely. */
	while( i > 0 )
	{
		addresses = realloc( addresses, len + 1024 );
		i = read( wld->server_resolver_fd, addresses + len, 1024 );

		if( i > 0 )
			len += i;
	}

	/* Make it a NUL-terminated string */
	addresses = realloc( addresses, len + 1 );
	addresses[len++] = '\0';

	/* The first character should say whether the lookup was succesful
	 * or not. */
	switch( addresses[0] )
	{
		case RESOLVE_SUCCESS:
		wld->server_addresslist = xstrdup( addresses + 2 );
		wld->flags |= WLD_SERVERCONNECT;
		break;

		case RESOLVE_ERROR:
		world_msg_client( wld, "%s", addresses + 2 );
		break;

		/* This shouldn't happen */
		default:
		world_msg_client( wld, "Resolver slave went wacko." );
		break;
	}

	/* Clean up, kill resolver slave. */
	free( addresses );
	kill( wld->server_resolver_pid, SIGKILL );
	waitpid( wld->server_resolver_pid, NULL, 0 );

	wld->server_resolver_pid = -1;
	wld->server_resolver_fd = -1;

	/* We're still not connected, but not in the process of connecting
	 * either. The wld->flags |= WLD_SERVERCONNECT will take care of that */
	wld->server_status = ST_DISCONNECTED;
}



/* The resolver slave code. Do the DNS lookup, format the data in a nice
 * string, and send that string back to the parent. */
static void slave_main( World* wld, int commfd  )
{
	struct addrinfo hints, *ailist, *ai;
	char *msg, hostbuf[NI_MAXHOST + 1];
	int ret, msglen, len;

	/* Specify the socket addresses we want. Any AF the system supports,
	 * STREAM socket type, TCP protocol. */
	memset( &hints, '\0', sizeof( hints ) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	/* Get the socket addresses */
	ret = getaddrinfo( wld->server_host, NULL, &hints, &ailist );
	if( ret != 0 )
	{
		xasprintf( &msg, "%c\nResolving failed: %s",
				RESOLVE_ERROR, gai_strerror( ret ) );
		slave_msg_to_parent( msg, commfd );
		return;
	}

	msglen = 3;
	msg = xstrdup( " \n" );
	msg[0] = RESOLVE_SUCCESS;

	/* Loop through the socket addresses, convert them to numeric
	 * address representations, and append those to the msg string */
	for( ai = ailist; ai != NULL; ai = ai->ai_next )
	{
		if( getnameinfo( ai->ai_addr, ai->ai_addrlen, hostbuf,
				NI_MAXHOST, NULL, 0, NI_NUMERICHOST ) != 0 )
			continue;

		len = strlen( hostbuf );
		msg = realloc( msg, msglen + len + 1 );
		strcat( msg, hostbuf );
		strcat( msg, "\n" );
		msglen += len + 1;
	}

	freeaddrinfo( ailist );

	slave_msg_to_parent( msg, commfd );
}



/* Write str over fd to the parent. */
static void slave_msg_to_parent( char *str, int fd )
{
	int i, len = strlen( str ) + 1;

	/* fd is (or should be) non-blocking. When there's more data to send
	 * than fits in the kernel socket buffers, write() will block, but
	 * the parent will become unblocked and will read from the FD. Then
	 * our write becomes unblocked. */
	while( len > 0 )
	{
		i = write( fd, str, len );

		if( i > 0 )
			len -= i;
		else
			break;
	}

	free( str );
}
