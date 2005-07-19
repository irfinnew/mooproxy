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
#include <stdio.h> /* FIXME: remove */
#include <stdlib.h>

#include "global.h"
#include "network.h"
#include "misc.h"
#include "mcp.h"
#include "log.h"
#include "timer.h"



/* Maximum length of queue of pending kernel connections */
#define NET_BACKLOG 10



static int create_fdset( fd_set *, World * );
static void handle_listen_fd( World * );
static void handle_auth_fd( World *, int );
static void remove_auth_connection( World *, int, int );
static void verify_authentication( World *, int );
static void promote_auth_connection( World *, int );
static void handle_client_fd( World * );
static void handle_server_fd( World * );
static int buffer_to_lines( char *, int, int, Linequeue * );



/* This function opens the port on the local machine and listens to it.
 * On failure, it returns non-zero and places the error in the last arg. */
int world_bind_port( World *wld, char **err )
{
	int fd;
	struct sockaddr_in src;
	int yes = 1, i;

	if( wld->listenport == -1 )
	{
		asprintf( err, "No port defined to listen on." );
		return EXIT_BIND;
	}
	
	if( ( fd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 )
	{
		asprintf( err, "Could not create socket: %s",
				strerror_n( errno ) );
		return EXIT_SOCKET;
	}

	if( fcntl( fd, F_SETFL, O_NONBLOCK ) == -1 )
	{
		close( fd );
		asprintf( err, "Could not fcntl(): %s", strerror_n( errno ) );
		return EXIT_SOCKET;
	}

	i = setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof( int ) );
	if( i == -1 )
	{
		close( fd );
		asprintf( err, "Could not set socket option: %s",
				strerror_n( errno ) );
		return EXIT_SOCKET;
	}

	src.sin_family = AF_INET;
	src.sin_port = htons( wld->listenport );
	src.sin_addr.s_addr = htonl( INADDR_ANY );
	memset( &( src.sin_zero ), '\0', 8 );

	i = bind( fd, (struct sockaddr *) &src, sizeof( struct sockaddr ) );
	if( i == -1 )
	{
		close( fd );
		asprintf( err, "Could not bind on port %i: %s",
				wld->listenport, strerror_n( errno ) );
		return EXIT_BIND;
	}

	if( listen( fd, NET_BACKLOG ) == -1 )
	{
		close( fd );
		asprintf( err, "Could not listen on port %i: %s",
				wld->listenport, strerror_n( errno ) );
		return EXIT_LISTEN;
	}

	wld->listen_fd = fd;
	return EXIT_OK;
}



/* This function resolves the remote host to a useful address.
 * On failure, it returns non-zero and places the error in the last arg. */
int world_resolve_server( World *wld, char **err )
{
	struct hostent *host;

	if( wld->dest_host == NULL )
	{
		*err = strdup( "No hostname to connect to." );
		return EXIT_NOHOST;
	}
	
	/* FIXME: Use getnameinfo ? */
	host = gethostbyname( wld->dest_host );
	if( host == NULL )
	{
		asprintf( err, "Could not resolve ``%s'': %s.", wld->dest_host,
				hstrerror( h_errno ) );
		return EXIT_RESOLV;
	}

	if( wld->dest_ipv4_address )
		free( wld->dest_ipv4_address );

	wld->dest_ipv4_address = strdup( inet_ntoa( *( (struct in_addr *)
			host->h_addr ) ) );
	return EXIT_OK;
}



/* Return 1 if we're connected to the client, 0 if not. */
extern int world_connected_to_client( World *wld )
{
	return wld->client_fd != -1;
}



/* Return 1 if we're connected to the server, 0 if not. */
extern int world_connected_to_server( World *wld )
{
	return wld->server_fd != -1;
}



/* This function connects to the resolved address.
 * On failure, it returns non-zero and places the error in the last arg. */
extern int world_connect_server( World *wld, char **err )
{
	int fd;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons( wld->dest_port );
	inet_aton( wld->dest_ipv4_address, &( addr.sin_addr ) );
	memset( &( addr.sin_zero ), '\0', 8 );

	if( ( fd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 )
	{
		asprintf( err, "Could not create socket: %s",
				strerror_n( errno ) );
		return EXIT_SOCKET;
	}

	if( connect( fd, (struct sockaddr *) &addr,
			sizeof( struct sockaddr ) ) == -1 )
	{
		close( fd );
		asprintf( err, "Could not connect: %s", strerror_n( errno ) );
		return EXIT_CONNECT;
	}

	if( fcntl( fd, F_SETFL, O_NONBLOCK ) == -1 )
	{
		close( fd );
		asprintf( err, "Could not fcntl(): %s", strerror_n( errno ) );
		return EXIT_SOCKET;
	}

	wld->server_fd = fd;
	return EXIT_OK;
}



/* This function disconnects from the server. */
extern void world_disconnect_server( World *wld )
{
	if( wld->server_fd == -1 )
		return;

	close( wld->server_fd );
	wld->server_fd = -1;
	wld->server_txfull = 0;
	wld->server_rxfull = 0;
}



/* This function disconnects the client. */
extern void world_disconnect_client( World *wld )
{
	if( wld->client_fd != -1 )
		close( wld->client_fd );
	wld->client_fd = -1;
	wld->client_txfull = 0;
	wld->client_rxfull = 0;
}



/* Wait for data from the network or timeout.
 * Parse data into lines, return lines to main loop. */
extern void wait_for_network( World *wld )
{
	struct timeval tv;
	fd_set rset;
	int high, i;

	/* If there is a correctly authenticated authconn, promote that
	 * and be done. */
	for( i = 0; i < wld->auth_connections; i++ )
		if( wld->auth_correct[i] )
		{
			promote_auth_connection( wld, i );
			return;
		}

	high = create_fdset( &rset, wld );
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	/* FIXME: should check for EBADF ? */
	select( high, &rset, NULL, NULL, &tv );

	if( wld->server_fd != -1 && FD_ISSET( wld->server_fd, &rset ) )
		handle_server_fd( wld );

	if( wld->client_fd != -1 && FD_ISSET( wld->client_fd, &rset ) )
		handle_client_fd( wld );

	if( wld->listen_fd != -1 && FD_ISSET( wld->listen_fd, &rset ) )
		handle_listen_fd( wld );

	for( i = wld->auth_connections - 1; i >= 0; i-- )
		if( FD_ISSET( wld->auth_fd[i], &rset ) )
			handle_auth_fd( wld, i );
}



/* This function creates the set of FDs that need to be watched. */
int create_fdset( fd_set *set, World *wld )
{
	int high = 0, i;

	FD_ZERO( set );

	/* Add server FD */
	if( wld->server_fd != -1 )
		FD_SET( wld->server_fd, set );
	high = wld->server_fd > high ? wld->server_fd : high;

	/* Add listening FD */
	if( wld->listen_fd != -1 )
		FD_SET( wld->listen_fd, set );
	high = wld->listen_fd > high ? wld->listen_fd : high;

	/* Add the authenticating FDs */
	for( i = wld->auth_connections - 1; i >= 0; i-- )
	{
		FD_SET( wld->auth_fd[i], set );
		high = wld->auth_fd[i] > high ? wld->auth_fd[i] : high;
	}

	/* Add client FD */
	if( wld->client_fd != -1 )
		FD_SET( wld->client_fd, set );
	high = wld->client_fd > high ? wld->client_fd : high;

	return high + 1;
}



/* Handle any events related to the listening connection. */
static void handle_listen_fd( World *wld )
{
	struct sockaddr_in rhost;
	int newfd, i;
	socklen_t addrlen = sizeof( struct sockaddr_in );

	/* FIXME: more error checking */
	newfd = accept( wld->listen_fd, (struct sockaddr *) &rhost, &addrlen );

	if( newfd == -1 )
	{
		printf( "accept(): %s !\n", strerror_n( errno ) );
		return;
	}

	/* If all auth slots are full, kick oldest */
	if( wld->auth_connections == NET_MAXAUTHCONN )
		remove_auth_connection( wld, 0, 0 );

	/* Use the next available slot */
	i = wld->auth_connections++;
	
	/* Initialize carefully. auth_correct = 0 is especially important. */
	wld->auth_fd[i] = newfd;
	wld->auth_correct[i] = 0;
	wld->auth_read[i] = 0;

	/* Zero the buffer, just to be sure.
	 * Now an erroneous compare won't check against garbage (or, worse,
	 * a correct authstring) still in the buffer. */
	memset( wld->auth_buf[i], 0, NET_MAXAUTHLEN );

	/* "Hey you!" */
	write( newfd, NET_AUTHSTRING "\n", strlen( NET_AUTHSTRING "\n" ) );

	return;
}



/* Handle any events related to the authenticating connections. */
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

	/* Tell the peer the authstring was wrong? */
	if( donack )
		write( wld->auth_fd[wa], NET_AUTHFAIL "\r\n",
				strlen( NET_AUTHFAIL "\r\n" ) );

	if( wld->auth_fd[wa] != -1 )
		close( wld->auth_fd[wa] );

	/* Save the buffer of the kicked connection for later */
	buf = wld->auth_buf[wa];

	/* Shift the later part of the array */
	for( i = wa; i < wld->auth_connections - 1; i++ )
	{
		wld->auth_fd[i] = wld->auth_fd[i + 1];
		wld->auth_read[i] = wld->auth_read[i + 1];
		wld->auth_buf[i] = wld->auth_buf[i + 1];
	}

	wld->auth_connections--;
	wld->auth_read[wld->auth_connections] = 0;
	wld->auth_fd[wld->auth_connections] = -1;
	wld->auth_buf[wld->auth_connections] = buf;
}



/* Check the received buffer against the authstring.
 * If they match, the auth connection is flagged as authenticated, and
 * the client connection is flagged for disconnection.
 * Otherwise, the auth connection is torn down. */
static void verify_authentication( World *wld, int wa )
{
	int alen = strlen( wld->authstring );

	/* If the authstring is nonexistent / empty, reject everything */
	if( !wld->authstring || !wld->authstring[0] )
	{
		remove_auth_connection( wld, wa, 1 );
		return;
	}

	/* We will only use the first (NET_MAXAUTHLEN - 2) chars */
	if( alen > NET_MAXAUTHLEN - 2 )
		alen = NET_MAXAUTHLEN - 2;

	/* The given string needs to be at least as long as the authstring
	 * plus a newline. If it's not, dump it. */
	if( wld->auth_read[wa] < alen + 1 )
	{
		remove_auth_connection( wld, wa, 1 );
		return;
	}

	/* Is the authentication string correct? */
	if( strncmp( wld->auth_buf[wa], wld->authstring, alen ) )
	{
		remove_auth_connection( wld, wa, 1 );
		return;
	}

	/* Ignore a potential \r following the (correct) authstring */
	if( wld->auth_buf[wa][alen] == '\r' )
		alen++;

	/* If the (correct) authstring is not followed by \n, reject */
	if( wld->auth_read[wa] < alen + 1 || wld->auth_buf[wa][alen] != '\n' )
	{
		remove_auth_connection( wld, wa, 1 );
		return;
	}
	alen++; /* alen now holds the length of the checked authstring. */

	/* Move anything after the (correct) authstring to the start of
	 * the buffer. */
	memmove( wld->auth_buf[wa], wld->auth_buf[wa] + alen, 
			wld->auth_read[wa] - alen );
	wld->auth_read[wa] -= alen;

	/* Tell the client (if any) that the connection is taken over,
	 * and flag the client connection for disconnection. */
	if( wld->client_fd != -1 )
		world_message_to_client( wld, NET_CONNTAKEOVER );
	wld->flags |= WLD_CLIENTQUIT;

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
	if( world_connected_to_client( wld ) )
	{
		wld->flags |= WLD_CLIENTQUIT;
		return;
	}

	/* Transfer connection */
	wld->client_fd = wld->auth_fd[wa];
	wld->auth_fd[wa] = -1;

	/* Copy anything left in the authbuf to client buffer, and process */
	memcpy( wld->client_rxbuffer, wld->auth_buf[wa], wld->auth_read[wa] );
	wld->client_rxfull = buffer_to_lines( wld->client_rxbuffer, 0,
			wld->auth_read[wa], wld->client_rxqueue );

	/* Clean up stuff left of the auth connection */
	remove_auth_connection( wld, wa, 0 );

	/* Confirm the connection to the authenticating client */
	world_message_to_client( wld, NET_AUTHGOOD );

	/* Show context history and pass buffered text */
	world_recall_and_pass( wld );

	/* Do the MCP reset, so MCP is set up correctly */
	if( wld->server_fd != -1 )
		world_send_mcp_reset( wld );
}



/* Handle any events related to the client connection. */
static void handle_client_fd( World *wld )
{
	int n, offset = wld->client_rxfull;
	char *s, *buffer = wld->client_rxbuffer;

	/* If the bbuffer is full, pass the entire contents as one line */
	if( offset == NET_BBUFFER_LEN )
	{
		s = malloc( NET_BBUFFER_LEN + 2 );
		memcpy( s, buffer, NET_BBUFFER_LEN );
		s[NET_BBUFFER_LEN] = '\n';
		s[NET_BBUFFER_LEN + 1] = '\0';
		linequeue_append( wld->client_rxqueue,
				line_create( s, NET_BBUFFER_LEN + 1 ) );
		wld->client_rxfull = 0;
		offset = 0;
	}

	n = read( wld->client_fd, buffer + offset, NET_BBUFFER_LEN - offset );

	if( n == -1 && ( errno == EINTR || errno == EAGAIN ) )
		return;

	if( n == 0 || n == -1 )
	{
		/* Ok, the connection dropped dead */
		world_disconnect_client( wld );
		return;
	}

	wld->client_rxfull =
		buffer_to_lines( buffer, offset, n, wld->client_rxqueue );
}



/* Handle any events related to the server connection. */
static void handle_server_fd( World *wld )
{
	int err, n, offset = wld->server_rxfull;
	char *str, *s, *buffer = wld->server_rxbuffer;

	/* If the bbuffer is full, pass the entire contents as one line */
	if( offset == NET_BBUFFER_LEN )
	{
		s = malloc( NET_BBUFFER_LEN + 2 );
		memcpy( s, buffer, NET_BBUFFER_LEN );
		s[NET_BBUFFER_LEN] = '\n';
		s[NET_BBUFFER_LEN + 1] = '\0';
		linequeue_append( wld->server_rxqueue,
				line_create( s, NET_BBUFFER_LEN + 1 ) );
		wld->server_rxfull = 0;
		offset = 0;
	}

	n = read( wld->server_fd, buffer + offset, NET_BBUFFER_LEN - offset );

	if( n == -1 && ( errno == EINTR || errno == EAGAIN ) )
		return;

	if( n < 1 )
	{
		/* Ok, the connection dropped dead */
		err = errno;
		world_disconnect_server( wld );

		/* Notify the client */
		asprintf( &str, "Connection to server lost (%s).",
				n ? strerror_n( err ) : "connection closed" );
		world_checkpoint_to_client( wld, str );
		free( str );

		return;
	}

	wld->server_rxfull =
		buffer_to_lines( buffer, offset, n, wld->server_rxqueue );
}



/* Process the given buffer into lines. Start scanning at offset, scan
 * only read bytes. The salvaged lines are appended to queue.
 * Return the new offset. */
static int buffer_to_lines( char *buffer, int offset, int read, Linequeue *q )
{
	int i, last = 0;
	char *t, *s = buffer;

	for( i = offset; i < offset + read; i++ )
		if( buffer[i] == '\n' )
		{
			last = i - last + 1;
			t = malloc( last + 1 );
			memcpy( t, s, last );
			t[last] = '\0';
			linequeue_append( q, line_create( t, last ) );

			s = buffer + i + 1;
			last = i + 1;
		}

	/* Shift the remaining buffer to the start. */
	for( i = last; i < offset + read; i++ )
		buffer[i - last] = buffer[i];

	return offset + read - last;
}



/* Try to flush the queued lines into the send buffer, and the send buffer
 * to the client. If this fails, leave the contents of the buffer in the
 * queued lines, mark the fd as write-blocked, and signal an FD change. */
extern void world_flush_client_txbuf( World *wld )
{
	Line *line = NULL;
	char *buffer = wld->client_txbuffer, *s;
	long offset = wld->client_txfull, len;
	int r;

	/* If there is nothing to send, do nothing */
	if( wld->client_txqueue->count == 0 && wld->client_txfull == 0 )
		return;

	/* If we're not connected, do nothing */
	if( wld->client_fd == -1 )
		return;

	/* FIXME: It's scary how double free's are avoided. */
	/* FIXME: 1 write() per line. Subpar... */
	while( offset > 0 || ( line = linequeue_pop( wld->client_txqueue ) ) )
	{
		/* Get pointer/length of to-be-written data */
		s = line ? line->str : buffer;
		len = line ? line->len : offset;

		/* Write it */
		r = write( wld->client_fd, s, len );

		/* Abort on serious error. The read() will catch it */
		if( r == -1 && errno != EAGAIN && errno != EINTR )
			break;

		/* Not all was written. Save in buffer. */
		if( r < len )
		{
			if( r < 0 )
				r = 0;

			len -= r;
			/* Don't move if not needed */
			if( buffer != s + r )
				memmove( buffer, s + r, len );
			offset = len;
			break;
		}

		if( line )
			world_store_history_line( wld, line );
		offset = 0;
	}

	if( line )
		world_store_history_line( wld, line );

	wld->client_txfull = offset;
}



/* Try to flush the queued lines into the send buffer, and the send buffer
 * to the server. If this fails, leave the contents of the buffer in the
 * queued lines, mark the fd as write-blocked, and signal an FD change.
 * If the socket is closed, discard contents, and announce
 * disconnectedness. */
extern void world_flush_server_txbuf( World *wld )
{
	Line *line = NULL;
	char *buffer = wld->server_txbuffer, *s;
	long offset = wld->server_txfull, len;
	int r;

	/* If there is nothing to send, do nothing */
	if( wld->server_txqueue->count == 0 && wld->server_txfull == 0 )
		return;

	/* If we're not connected, discard and notify client */
	if( wld->server_fd == -1 )
	{
		linequeue_clear( wld->server_txqueue );
		world_message_to_client( wld, "Not connected to server!" );
		return;
	}

	/* FIXME: It's scary how double free's are avoided. */
	/* FIXME: 1 write() per line. Subpar... */
	while( offset > 0 || ( line = linequeue_pop( wld->server_txqueue ) ) )
	{
		/* Get pointer/length of to-be-written data */
		s = line ? line->str : buffer;
		len = line ? line->len : offset;

		/* Write it */
		r = write( wld->server_fd, s, len );

		/* Abort on serious error. The read() will catch it */
		if( r == -1 && errno != EAGAIN && errno != EINTR )
			break;

		/* Not all was written. Save in buffer. */
		if( r < len )
		{
			if( r < 0 )
				r = 0;

			len -= r;
			/* Don't move if not needed */
			if( buffer != s + r )
				memmove( buffer, s + r, len );
			offset = len;
			break;
		}

		/* Free the line, if it exists */
		line_destroy( line );
		offset = 0;
	}

	/* Free the line, if it exists */
	line_destroy( line );

	wld->server_txfull = offset;
}
