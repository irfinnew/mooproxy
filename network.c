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



#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include "global.h"
#include "network.h"
#include "misc.h"
#include "mcp.h"
#include "log.h"
#include "timer.h"
#include "resolve.h"
#include "crypt.h"



/* Maximum length of queue of pending kernel connections. */
#define NET_BACKLOG 4

/* Authentication messages */
#define NET_AUTHSTRING "Welcome, please authenticate."
#define NET_AUTHFAIL "Authentication failed, goodbye."



static int create_fdset( World *, fd_set *, fd_set * );
static void handle_connecting_fd( World * );
static void server_connect_error( World *, int, const char * );
static void handle_listen_fd( World *, int );
static void handle_auth_fd( World *, int );
static void remove_auth_connection( World *, int, int );
static void verify_authentication( World *, int );
static void promote_auth_connection( World *, int );
static void handle_client_fd( World * );
static void handle_server_fd( World * );



extern void wait_for_network( World *wld )
{
	struct timeval tv;
	fd_set rset, wset;
	int high, i;

	/* If there is a correctly authenticated authconn, promote that
	 * connection and be done. */
	for( i = 0; i < wld->auth_connections; i++ )
		if( wld->auth_correct[i] )
		{
			promote_auth_connection( wld, i );
			return;
		}

	/* If there are unprocessed lines in the *_toqueue queues, return.
	 * The main loop will move these lines to their respective transmit
	 * queues, after which we'll be called again. We will handle the
	 * transmit queues. */
	if( wld->server_toqueue->count + wld->client_toqueue->count > 0 )
		return;

	/* Prepare for select() */
	high = create_fdset( wld, &rset, &wset );
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	if( select( high + 1, &rset, &wset, NULL, &tv ) < 0 )
		printf( "Select() said %s!\n", strerror( errno ) );

	/* Check all of the monitored FD's, and handle them appropriately. */

	if( wld->server_resolver_fd != -1 &&
			FD_ISSET( wld->server_resolver_fd, &rset ) )
		world_handle_resolver_fd( wld );

	if( wld->server_fd != -1 && FD_ISSET( wld->server_fd, &rset ) )
		handle_server_fd( wld );

	if( wld->client_fd != -1 && FD_ISSET( wld->client_fd, &rset ) )
		handle_client_fd( wld );

	for( i = 0; wld->listen_fds[i] != -1; i++ )
		if( FD_ISSET( wld->listen_fds[i], &rset ) )
			handle_listen_fd( wld, i );

	for( i = wld->auth_connections - 1; i >= 0; i-- )
		if( FD_ISSET( wld->auth_fd[i], &rset ) )
			handle_auth_fd( wld, i );

	if( wld->server_connecting_fd != -1 )
		if( FD_ISSET( wld->server_connecting_fd, &wset ) )
			handle_connecting_fd( wld );
}



/* Construct the fd_set's for the select() call.
 * Put all the FD's that are watched for reading in rset, and all those that
 * are watched for writing in wset. Return the highest FD. */
static int create_fdset( World *wld, fd_set *rset, fd_set *wset )
{
	int high = 0, i;

	/* -------- Readable FD's -------- */
	FD_ZERO( rset );

	/* Add resolver slave FD */
	if( wld->server_resolver_fd != -1 )
		FD_SET( wld->server_resolver_fd, rset );
	if( wld->server_resolver_fd > high )
		high = wld->server_resolver_fd;

	/* Add server FD */
	if( wld->server_fd != -1 )
		FD_SET( wld->server_fd, rset );
	if( wld->server_fd > high )
		high = wld->server_fd;

	/* Add listening FDs */
	for( i = 0; wld->listen_fds[i] != -1; i++ )
	{
		FD_SET( wld->listen_fds[i], rset );
		if( wld->listen_fds[i] > high )
			high = wld->listen_fds[i];
	}

	/* Add the authenticating FDs */
	for( i = wld->auth_connections - 1; i >= 0; i-- )
	{
		FD_SET( wld->auth_fd[i], rset );
		if( wld->auth_fd[i] > high )
			high = wld->auth_fd[i];
	}

	/* Add client FD */
	if( wld->client_fd != -1 )
		FD_SET( wld->client_fd, rset );
	if( wld->client_fd > high )
		high = wld->client_fd;

	/* -------- Writable FD's -------- */
	FD_ZERO( wset );

	/* Add the non-blocking connecting FD */
	if( wld->server_connecting_fd != -1 )
		FD_SET( wld->server_connecting_fd, wset );
	if( wld->server_connecting_fd > high )
		high = wld->server_connecting_fd;

	/* If there is data to be written to the server, we want to
	 * know if the FD is writable. */
	if( wld->server_txqueue->count > 0 || wld->server_txfull > 0 )
	{
		if( wld->server_fd != -1 )
			FD_SET( wld->server_fd, wset );
		if( wld->server_fd > high )
			high = wld->server_fd;
	}

	/* If there is data to be written to the client, we want to
	 * know if the FD is writable. */
	if( wld->client_txqueue->count > 0 || wld->client_txfull > 0 )
	{
		if( wld->client_fd != -1 )
			FD_SET( wld->client_fd, wset );
		if( wld->client_fd > high )
			high = wld->client_fd;
	}

	return high;
}



extern int world_bind_port( World *wld, char **err )
{
	struct addrinfo hints, *ailist, *ai;
	int numfds = 0, fd, yes = 1, ret, i;
	char hostbuf[NI_MAXHOST + 1], port[NI_MAXSERV + 1];

	/* Clean up existing listen FDs. */
	if( wld->listen_fds != NULL )
		for( i = 0; wld->listen_fds[i] != -1; i++ )
			close( wld->listen_fds[i] );

	free( wld->listen_fds );

	/* We start with a list of one FD (the -1 terminator) */
	wld->listen_fds = malloc( sizeof( int ) );
	wld->listen_fds[0] = -1;

	if( wld->listenport == -1 )
	{
		xasprintf( err, "No port defined to listen on." );
		return 1;
	}

	/* Specify the socket addresses we want. Any AF the system supports,
	 * STREAM socket type, TCP protocol, listening socket, and our port. */
	memset( &hints, '\0', sizeof( hints ) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	snprintf( port, NI_MAXSERV, "%li", wld->listenport );

	/* Get the socket addresses */
	ret = getaddrinfo( NULL, port, &hints, &ailist );
	if( ret != 0 )
	{
		xasprintf( err, "Getting address information failed: %s",
				gai_strerror( ret ) );
		return 1;
	}

	/* Loop over the socket addresses, and attempt to create, bind(),
	 * and listen() them all. */
	for( ai = ailist; ai != NULL; ai = ai->ai_next )
	{
		/* Get the numeric representation of this address
		 * (e.g. "0.0.0.0" or "::") */
		ret = getnameinfo( ai->ai_addr, ai->ai_addrlen, hostbuf,
				NI_MAXHOST, NULL, 0, NI_NUMERICHOST );
		if( ret != 0 )
		{
			printf( "Getting name information failed: %s\n",
					gai_strerror( ret ) );
			continue;
		}

		/* Create the socket */
		fd = socket( ai->ai_family, ai->ai_socktype, ai->ai_protocol );
		if( fd < 0 )
			goto world_bind_port_warn;

		/* Set the socket to non-blocking */
		if( fcntl( fd, F_SETFL, O_NONBLOCK ) < 0 )
			goto world_bind_port_warn;

		/* Reuse socket address */
		if( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &yes,
					sizeof( int ) ) < 0 )
			goto world_bind_port_warn;

		/* Try IPV6_V6ONLY, so IPv4 won't get mapped onto IPv6 */
		#if defined( PF_INET6 ) && defined( IPV6_V6ONLY )
		if( ai->ai_family == AF_INET6 && setsockopt( fd, IPPROTO_IPV6,
				IPV6_V6ONLY, &yes, sizeof( yes ) ) < 0 )
			goto world_bind_port_warn;
		#endif

		/* Bind to and listen on the port */
		if( bind( fd, ai->ai_addr, ai->ai_addrlen ) < 0 )
			goto world_bind_port_warn;

		if( listen( fd, NET_BACKLOG ) < 0 )
			goto world_bind_port_warn;

		/* Report succes, and add the FD to the list */
		printf( "Listening on %s port %s.\n", hostbuf, port );
		wld->listen_fds = xrealloc( wld->listen_fds,
				sizeof( int ) * ( numfds + 2 ) );
		wld->listen_fds[numfds++] = fd;
		wld->listen_fds[numfds] = -1;
		continue;

	world_bind_port_warn:
		/* Single error handling and cleanup point */
		printf( "Listening on %s port %s failed: %s\n", hostbuf,
				port, strerror( errno ) );
		if( fd > -1 )
			close( fd );
	}

	freeaddrinfo( ailist );

	/* We need at least one FD, otherwise we're not listening on a port */
	if( numfds == 0 )
	{
		xasprintf( err, "Listening on port %s failed for all "
				"protocol families.", port );
		return 1;
	}

	return 0;
}



extern void world_start_server_connect( World *wld )
{
	struct addrinfo hints, *ai = NULL;
	char *tmp;
	int len, ret, fd = -1;

	/* Clean up any previous address */
	free( wld->server_address );
	wld->server_address = NULL;

	/* This shouldn't happen */
	if( wld->server_addresslist == NULL )
		return;

	/* If we reached the end of the list of addresses, then none of the
	 * addresses in the list worked. Bail out with error. */
	if( wld->server_addresslist[0] == '\0' )
	{
		free( wld->server_addresslist );
		wld->server_addresslist = NULL;
		world_msg_client( wld, "Connecting failed, giving up." );
		wld->server_status = ST_DISCONNECTED;
		return;
	}

	/* wld->server_addresslist is a C string containing a list of
	 * \n-separated addresses. Isolate the first address, put it in
	 * wld->server_address, and remove it from the list. */
	tmp = wld->server_addresslist;
	len = strlen( tmp );
	while( *tmp != '\n' && *tmp != '\0' )
		tmp++;

	*tmp = '\0';
	wld->server_address = xstrdup( wld->server_addresslist );
	memmove( wld->server_addresslist, tmp + 1, len -
			strlen( wld->server_addresslist ) );

	/* Bravely announce our attempt at the next address... */
	world_msg_client( wld, "   Trying %s", wld->server_address );

	/* Specify the socket address we want. Any AF that fits the address,
	 * STREAM socket type, TCP protocol, only numeric addresses. */
	memset( &hints, '\0', sizeof( hints ) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_NUMERICHOST;

	/* Get the socket addresses */
	ret = getaddrinfo( wld->server_address, wld->server_port, &hints, &ai );
	if( ret != 0 )
		return server_connect_error( wld, fd, gai_strerror( ret ) );

	/* Since we fed getaddrinfo() a numeric address string (e.g. "1.2.3.4")
	 * we should get only one socket address (namely for the AF that
	 * goes with the particular address string format).
	 * Therefore, we only use the first of the returned list. */
	fd = socket( ai->ai_family, ai->ai_socktype, ai->ai_protocol );
	if( fd < 0 )
	{
		freeaddrinfo( ai );
		return server_connect_error( wld, fd, strerror( errno ) );
	}

	/* Non-blocking connect, please */
	if( fcntl( fd, F_SETFL, O_NONBLOCK ) < 0 )
	{
		freeaddrinfo( ai );
		return server_connect_error( wld, fd, strerror( errno ) );
	}

	/* Connect. Failure with EINPROGRESS is ok (and even expected) */
	ret = connect( fd, ai->ai_addr, ai->ai_addrlen );
	if( ret < 0 && errno != EINPROGRESS )
	{
		freeaddrinfo( ai );
		return server_connect_error( wld, fd, strerror( errno ) );
	}

	/* Connection in progress! */
	wld->server_connecting_fd = fd;
	wld->server_status = ST_CONNECTING;
	freeaddrinfo( ai );
}



/* Handle activity on the non-blocking connecting FD. */
static void handle_connecting_fd( World *wld )
{
	int so_err, fd = wld->server_connecting_fd;
	socklen_t optlen = sizeof( so_err );

	/* Get the SO_ERROR option from the socket. This option holds the
	 * error code for the connect() in the case of connection failure.
	 * Also, check for other getsockopt() errors. */
	if( getsockopt( fd, SOL_SOCKET, SO_ERROR, &so_err, &optlen ) < 0 )
		return server_connect_error( wld, fd, strerror( errno ) );
	if( optlen != sizeof( so_err ) )
		return server_connect_error( wld, fd, "getsockopt weirdness" );
	if( so_err != 0 )
		return server_connect_error( wld, fd, strerror( so_err ) );

	/* Connecting succesful, we have a FD. Now make it non-blocking. */
	if( fcntl( fd, F_SETFL, O_NONBLOCK ) < 0 )
		return server_connect_error( wld, fd, strerror( errno ) );

	/* Clean up addresslist */
	free( wld->server_addresslist );
	wld->server_addresslist = NULL;

	/* Transfer the FD */
	wld->server_connecting_fd = -1;
	wld->server_fd = fd;

	/* Flag and announce connectedness. */
	wld->server_status = ST_CONNECTED;

	world_msg_client( wld, "      Succes." );
	world_msg_client( wld, "Connected to world %s.", wld->name );

	/* Auto-login */
	world_login_server( wld, 0 );
}



/* This function gets called from world_start_server_connect() and
 * handle_connecting_fd() on error. It displays a warning to the user,
 * cleans stuff up, and flags the world for the next connection attempt
 * (so that the next address in wld->server_addresslist is attempted) */
static void server_connect_error( World *wld, int fd, const char *err )
{
	world_msg_client( wld, "      Failure: %s", err );

	if( fd != -1 )
		close( fd );
	wld->server_connecting_fd = -1;

	free( wld->server_address );
	wld->server_address = NULL;

	wld->flags |= WLD_SERVERCONNECT;
}



extern void world_cancel_server_connect( World *wld )
{
	/* We aren't connecting, abort */
	if( wld->server_connecting_fd == -1 )
		return;

	close( wld->server_connecting_fd );
	wld->server_connecting_fd = -1;

	free( wld->server_address );
	wld->server_address = NULL;

	free( wld->server_addresslist );
	wld->server_addresslist = NULL;

	wld->server_status = ST_DISCONNECTED;
}



extern void world_disconnect_server( World *wld )
{
	if( wld->server_fd > -1 )
		close( wld->server_fd );

	wld->server_fd = -1;
	wld->server_txfull = 0;
	wld->server_rxfull = 0;

	free( wld->server_address );
	wld->server_address = NULL;

	wld->mcp_negotiated = 0;

	wld->server_status = ST_DISCONNECTED;
}



extern void world_disconnect_client( World *wld )
{
	if( wld->client_fd != -1 )
		close( wld->client_fd );

	free( wld->client_prev_address );
	wld->client_prev_address = wld->client_address;
	wld->client_address = NULL;

	wld->client_last_connected = current_time();

	wld->client_fd = -1;
	wld->client_txfull = 0;
	wld->client_rxfull = 0;

	wld->client_status = ST_DISCONNECTED;
}



/* Accepts a new connection on the listening FD, and places the new connection
 * in the list of authentication connections. */
static void handle_listen_fd( World *wld, int which )
{
	struct sockaddr_storage sa;
	socklen_t sal = sizeof( sa );
	int newfd, i, ret;
	char hostbuf[NI_MAXHOST + 1];

	/* Accept the new connection */
	newfd = accept( wld->listen_fds[which], (struct sockaddr *) &sa, &sal );
	if( newfd == -1 )
	{
		printf( "accept(): %s !\n", strerror( errno ) );
		return;
	}

	/* Who's connecting to us? If getting the address fails, abort */
	ret = getnameinfo( (struct sockaddr *) &sa, sal, hostbuf, NI_MAXHOST,
			NULL, 0, NI_NUMERICHOST );
	if( ret != 0 )
	{
		printf( "getnameinfo(): %s\n", gai_strerror( ret ) );
		close( newfd );
		return;
	}

	/* Make the new fd nonblocking */
	if( fcntl( newfd, F_SETFL, O_NONBLOCK ) == -1 )
	{
		printf( "Could not fcntl(): %s", strerror( errno ) );
		close( newfd );
		return;
	}

	printf( "Accepted connection from %s.\n", hostbuf );

	/* If all auth slots are full, kick oldest */
	if( wld->auth_connections == NET_MAXAUTHCONN )
		remove_auth_connection( wld, 0, 0 );

	/* Use the next available slot */
	i = wld->auth_connections++;

	/* Initialize carefully. auth_correct = 0 is especially important. */
	wld->auth_fd[i] = newfd;
	wld->auth_correct[i] = 0;
	wld->auth_read[i] = 0;
	wld->auth_address[i] = xstrdup( hostbuf );

	/* Zero the buffer, just to be sure.
	 * Now an erroneous compare won't check against garbage (or, worse,
	 * a correct authentication string) still in the buffer. */
	memset( wld->auth_buf[i], 0, NET_MAXAUTHLEN );

	/* "Hey you!" */
	write( newfd, NET_AUTHSTRING "\r\n",
			sizeof( NET_AUTHSTRING "\r\n" ) - 1);
}



/* Handles activity on an authenticating FD. Reads any data from the FD and
 * place it in auth buffer. If the auth buffer is full, or a \n is read from
 * the FD, verify the authentication attempt. */
static void handle_auth_fd( World *wld, int wa )
{
	int i, n;

	/* If this connection has already been authenticated, ignore it. */
	if( wld->auth_correct[wa] )
		return;

	/* Read into the buffer */
	n = read( wld->auth_fd[wa], wld->auth_buf[wa] +
		wld->auth_read[wa], NET_MAXAUTHLEN - wld->auth_read[wa] );

	if( n == -1 && ( errno == EINTR || errno == EAGAIN ) )
		return;

	/* Connection closed, or error */
	if( n <= 0 )
	{
		remove_auth_connection( wld, wa, 0 );
		return;
	}

	/* Search for the first newline in the newly read part. */
	for( i = wld->auth_read[wa]; i < wld->auth_read[wa] + n; i++ )
		if( wld->auth_buf[wa][i] == '\n' )
			break;

	/* If we didn't encounter a newline, and we didn't read
	 * too many characters, return */
	if( i == wld->auth_read[wa] + n && i < NET_MAXAUTHLEN )
	{
		wld->auth_read[wa] += n;
		return;
	}

	/* We encountered a newline or read too many characters */
	wld->auth_read[wa] += n;
	verify_authentication( wld, wa );
}



/* Close and remove one authentication connection from the array, shift
 * the rest to make the array consecutive again, and decrease the counter.*/
static void remove_auth_connection( World *wld, int wa, int donack )
{
	char *buf;
	int i;

	/* Tell the peer the authentication string was wrong? */
	if( donack )
		write( wld->auth_fd[wa], NET_AUTHFAIL "\r\n",
				sizeof( NET_AUTHFAIL "\r\n" ) - 1);

	/* Free allocated resources */
	free( wld->auth_address[wa] );
	if( wld->auth_fd[wa] != -1 )
		close( wld->auth_fd[wa] );

	/* Remember the buffer address of the kicked connection for later */
	buf = wld->auth_buf[wa];

	/* Shift the later part of the array */
	for( i = wa; i < wld->auth_connections - 1; i++ )
	{
		wld->auth_fd[i] = wld->auth_fd[i + 1];
		wld->auth_read[i] = wld->auth_read[i + 1];
		wld->auth_buf[i] = wld->auth_buf[i + 1];
		wld->auth_address[i] = wld->auth_address[i + 1];
		wld->auth_correct[i] = wld->auth_correct[i + 1];
	}

	wld->auth_connections--;
	/* Initialise the newly freed auth connection */
	wld->auth_read[wld->auth_connections] = 0;
	wld->auth_fd[wld->auth_connections] = -1;
	wld->auth_buf[wld->auth_connections] = buf;
	wld->auth_address[wld->auth_connections] = NULL;
	wld->auth_correct[wld->auth_connections] = 0;
}



/* Check the received buffer against the authstring.
 * If they match, the auth connection is flagged as authenticated, and
 * the client connection is flagged for disconnection.
 * Otherwise, the auth connection is torn down. */
static void verify_authentication( World *wld, int wa )
{
	char *buffer = wld->auth_buf[wa];
	int alen, maxlen, buflen = wld->auth_read[wa];

	/* If the authentication string is nonexistent, reject everything */
	if( wld->auth_md5hash == NULL )
	{
		remove_auth_connection( wld, wa, 1 );
		return;
	}

	/* Determine the maximum number of characters to scan */
	maxlen = buflen + 2;
	if( maxlen > NET_MAXAUTHLEN )
		maxlen = NET_MAXAUTHLEN;

	/* Search for the first newline */
	for( alen = 0; alen < maxlen - 1; alen++ )
		if( buffer[alen] == '\n' )
			break;

	/* If we didn't find a newline, trash it. */
	if( buffer[alen] != '\n' )
	{
		remove_auth_connection( wld, wa, 1 );
		return;
	}

	/* Replace the newline by a NUL, so the part of the buffer before
	 * the newline becomes a C string. */
	buffer[alen] = '\0';
	/* If there was a \r before the newline, replace that too. */
	if( alen > 0 && buffer[alen - 1] == '\r' )
		buffer[alen - 1] = '\0';

	/* Now, check if the string before the newline is correct. */
	if( !world_match_authentication( wld, buffer ) )
	{
		remove_auth_connection( wld, wa, 1 );
		return;
	}

	/* Alen contains the length of the authentication string including
	 * the newline character(s), minus 1. Thus, buffer[alen] used to be
	 * \n and is now \0. We increase alen, so that buffer[alen] now
	 * is the first character after the authentication string including
	 * newline. */
	alen++;

	/* Move anything after the (correct) authentication string to the 
	 * start of the buffer. */
	memmove( buffer, buffer + alen, buflen - alen );
	wld->auth_read[wa] -= alen;

	/* Tell the client (if any) that the connection is taken over,
	 * and flag the client connection for disconnection. */
	if( wld->client_fd != -1 )
	{
		world_msg_client( wld, "Connection taken over from %s.",
				wld->auth_address[wa] );
		wld->flags |= WLD_CLIENTQUIT;
	}

	/* Finally, flag this connection as correctly authenticated. */
	wld->auth_correct[wa] = 1;
}



/* Promote the (correct) authconn to the client connection.
 * Clean up old auth stuff, transfer anything left in buffer to
 * client buffer. */
static void promote_auth_connection( World *wld, int wa )
{
	/* If the world is not correctly authenticated, ignore it. */
	if( !wld->auth_correct[wa] )
		return;

	/* The client shouln't be connected.
	 * If it is, flag for disconnection and let 'em come back to us.  */
	if( wld->client_status != ST_DISCONNECTED )
	{
		wld->flags |= WLD_CLIENTQUIT;
		return;
	}

	wld->client_status = ST_CONNECTED;

	/* Transfer connection */
	wld->client_fd = wld->auth_fd[wa];
	wld->auth_fd[wa] = -1;
	wld->client_address = wld->auth_address[wa];
	wld->auth_address[wa] = NULL;

	/* In order to copy anything left in the authbuffer to the rxbuffer,
	 * the rxbuffer must be large enough. */
	#if (NET_BBUFFER_LEN < NET_MAXAUTHLEN)
	  #error NET_BBUFFER_LEN < NET_MAXAUTHLEN
	#endif

	/* Copy anything left in the authbuf to client buffer, and process */
	memcpy( wld->client_rxbuffer, wld->auth_buf[wa], wld->auth_read[wa] );
	wld->client_rxfull = buffer_to_lines( wld->client_rxbuffer, 0,
			wld->auth_read[wa], wld->client_rxqueue );

	/* Clean up stuff left of the auth connection */
	remove_auth_connection( wld, wa, 0 );

	/* Confirm the connection to the authenticating client */
	world_msg_client( wld, "Authentication succesful, welcome." );
	if( wld->client_prev_address != NULL )
	{
		world_msg_client( wld, "Last connected at %s, from %s.",
				time_string( wld->client_last_connected,
				FULL_TIME ), wld->client_prev_address );
	}

	/* Show context history and pass buffered text */
	world_recall_and_pass( wld );

	/* Do the MCP reset, so MCP is set up correctly */
	if( wld->server_fd != -1 )
		world_send_mcp_reset( wld );
}



/* FIXME: handle_client_fd() and handle_server_fd() have duplicate code.
 * Perhaps merge them? */
/* Handles any waiting data on the client FD. Read data, parse in to lines,
 * append to RX queue. If the connection died, close FD. */
static void handle_client_fd( World *wld )
{
	int n, offset = wld->client_rxfull;
	char *buffer = wld->client_rxbuffer;

	/* Read into the buffer */
	n = read( wld->client_fd, buffer + offset, NET_BBUFFER_LEN - offset );

	/* Failure with EINTR or EAGAIN is acceptable. Just let it go. */
	if( n == -1 && ( errno == EINTR || errno == EAGAIN ) )
		return;

	if( n == 0 || n == -1 )
	{
		/* Ok, the connection dropped dead */
		world_disconnect_client( wld );
		return;
	}

	/* Parse to lines, and place in queue */
	wld->client_rxfull =
		buffer_to_lines( buffer, offset, n, wld->client_rxqueue );
}



/* Handles any waiting data on the client FD. Read data, parse in to lines,
 * append to RX queue. If the connection died, close FD. */
static void handle_server_fd( World *wld )
{
	int err, n, offset = wld->server_rxfull;
	char *buffer = wld->server_rxbuffer;

	/* Read into buffer */
	n = read( wld->server_fd, buffer + offset, NET_BBUFFER_LEN - offset );

	/* Failure with EINTR or EAGAIN is acceptable. Just let it go. */
	if( n == -1 && ( errno == EINTR || errno == EAGAIN ) )
		return;

	if( n < 1 )
	{
		/* Ok, the connection dropped dead */
		err = errno;
		world_disconnect_server( wld );

		/* Notify the client */
		world_msg_client( wld, "Connection to server lost (%s).",
				n ? strerror( err ) : "connection closed" );

		return;
	}

	/* Parse to lines, and place in queue */
	wld->server_rxfull =
		buffer_to_lines( buffer, offset, n, wld->server_rxqueue );
}



extern void world_flush_client_txbuf( World *wld )
{
	/* If we're not connected, do nothing */
	if( wld->client_fd == -1 )
		return;

	flush_buffer( wld->client_fd, wld->client_txbuffer,
			&( wld->client_txfull ), wld->client_txqueue, 
			wld->history_lines, 1, NULL );
}



extern void world_flush_server_txbuf( World *wld )
{
	/* If there is nothing to send, do nothing */
	if( wld->server_txqueue->count == 0 && wld->server_txfull == 0 )
		return;

	/* If we're not connected, discard and notify client */
	if( wld->server_fd == -1 )
	{
		linequeue_clear( wld->server_txqueue );
		wld->flags |= WLD_NOTCONNECTED;
		return;
	}

	flush_buffer( wld->server_fd, wld->server_txbuffer,
			&( wld->server_txfull ), wld->server_txqueue, NULL,
			1, NULL );
}
