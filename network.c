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
#include <time.h>

#include "network.h"
#include "global.h"
#include "misc.h"
#include "mcp.h"



/* Maximum length of queue of pending kernel connections */
#define NET_BACKLOG 10



static int create_fdset( fd_set *, World * );
static void handle_listen_fd( World * );
static void handle_auth_fd( World *, int );
static void remove_auth_connection( World *, int, int );
static void verify_authentication( World *, int );
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

	if( wld->host == NULL )
	{
		*err = strdup( "No hostname to connect to." );
		return EXIT_NOHOST;
	}
	
	/* FIXME: Use getnameinfo ? */
	host = gethostbyname( wld->host );
	if( host == NULL )
	{
		asprintf( err, "Could not resolve ``%s'': %s.", wld->host,
				hstrerror( h_errno ) );
		return EXIT_RESOLV;
	}

	if( wld->ipv4_address )
		free( wld->ipv4_address );

	wld->ipv4_address = strdup( inet_ntoa( *( (struct in_addr *)
			host->h_addr ) ) );
	return EXIT_OK;
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
	addr.sin_port = htons( wld->port );
	inet_aton( wld->ipv4_address, &( addr.sin_addr ) );
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
	wld->flags |= WLD_FDCHANGE;
	return EXIT_OK;
}



/* This function disconnects from the server. */
extern void world_disconnect_server( World *wld )
{
	if( wld->server_fd == -1 )
		return;

	world_flush_server_sbuf( wld );
	close( wld->server_fd );
	wld->server_fd = -1;
	wld->flags |= WLD_FDCHANGE;
}



/* This function disconnects the client. */
extern void world_disconnect_client( World *wld )
{
	world_flush_client_sbuf( wld );
	close( wld->client_fd );
	wld->client_fd = -1;
	wld->flags |= WLD_FDCHANGE;
}



/* This is the mainloop. It listens for incoming connetions and handles the
 * actual data over the connections. */
void world_mainloop( World *wld )
{
	struct timeval tv;
	fd_set readset, rset;
	int high, i;
	time_t last_checked = time( NULL ), ltime;

	high = create_fdset( &readset, wld );

	for(;;)
	{
		memcpy( &rset, &readset, sizeof( fd_set ) );
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

		/* See if another second elapsed */
		ltime = time( NULL );
		if( ltime != last_checked )
			world_timer_tick( wld, last_checked = ltime );

		/* If the client is disconnected, turn pass off */
		if( wld->client_fd == -1 )
			wld->flags &= ~WLD_PASSTEXT;

		/* Pass lines, if the client wants that */
		if( wld->flags & WLD_PASSTEXT )
			world_pass_buffered_text( wld, -1 );

		/* Flush the send buffers */
		world_flush_server_sbuf( wld );
		world_flush_client_sbuf( wld );

		/* Does the set of FDs need to be rebuilt? */
		if( wld->flags & WLD_FDCHANGE )
		{
			high = create_fdset( &readset, wld );
			wld->flags &= ~WLD_FDCHANGE;
		}

		/* Do we wanna quit? */
		if( wld->flags & WLD_QUIT )
			return;
	}
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
	int newfd, i, addrlen = sizeof( struct sockaddr_in );

	/* FIXME: more error checking */
	newfd = accept( wld->listen_fd, (struct sockaddr *) &rhost, &addrlen );

	if( newfd == -1 )
	{
		printf( "Error on accept() !\n" );
		return;
	}

	/* If all auth slots are full, kick oldest */
	if( wld->auth_connections == NET_MAXAUTHCONN )
		remove_auth_connection( wld, 0, 0 );

	/* Use the next available slot */
	i = wld->auth_connections++;
	
	wld->auth_fd[i] = newfd;
	wld->auth_read[i] = 0;
	write( newfd, NET_AUTHSTRING "\n", strlen( NET_AUTHSTRING "\n" ) );

	wld->flags |= WLD_FDCHANGE;
	return;
}



/* Handle any events related to the authenticating connections. */
/* FIXME: Inefficient, 1 syscall/character */
static void handle_auth_fd( World *wld, int wa )
{
	int i, n;

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
	wld->flags |= WLD_FDCHANGE;
}



/* Close and remove one authentication connection from the array, shift
 * the rest to make the array consecutive again, and decrease the counter.*/
static void remove_auth_connection( World *wld, int wa, int donack )
{
	char *buf;
	int i;

	if( donack )
		write( wld->auth_fd[wa], NET_AUTHFAIL "\n",
				strlen( NET_AUTHFAIL "\n" ) );

	if( wld->auth_fd[wa] != -1 )
		close( wld->auth_fd[wa] );

	buf = wld->auth_buf[wa];

	for( i = wa; i < wld->auth_connections - 1; i++ )
	{
		wld->auth_fd[i] = wld->auth_fd[i + 1];
		wld->auth_read[i] = wld->auth_read[i + 1];
		wld->auth_buf[i] = wld->auth_buf[i + 1];
	}

	wld->auth_connections--;
	wld->auth_buf[wld->auth_connections] = buf;

	wld->flags |= WLD_FDCHANGE;
}



/* Check the received buffer against the authstring.
 * If they match, the current auth connection is transferred to client_fd.
 * Otherwise, the auth connection is torn down. */
/* FIXME: rewrite this. It's icky. */
static void verify_authentication( World *wld, int wa )
{
	char *str;
	int alen = strlen( wld->authstring );

	/* If the authstring is nonexistent / empty, reject everything */
	if( !wld->authstring || !wld->authstring[0] )
	{
		remove_auth_connection( wld, wa, 1 );
		return;
	}

	/* We will only use the first (NET_MAXAUTHLEN - 2) chars */
	alen = alen > NET_MAXAUTHLEN - 2 ? NET_MAXAUTHLEN - 2 : alen;

	/* Is the authentication string correct? */
	if( wld->auth_read[wa] < alen + 1 ||
			strncmp( wld->auth_buf[wa], wld->authstring, alen ) )
	{
		remove_auth_connection( wld, wa, 1 );
		return;
	}

	/* Ignore a potential \r before the \n */
	if( wld->auth_buf[wa][alen] == '\r' )
		alen++;

	/* If the (correct) authstring is not followed by \n, reject */
	if( wld->auth_read[wa] < alen + 1 || wld->auth_buf[wa][alen] != '\n' )
	{
		remove_auth_connection( wld, wa, 1 );
		return;
	}
	alen++;

	/* Close the old connection */
	if( wld->client_fd != -1 )
	{
		world_message_to_client( wld, NET_CONNTAKEOVER );
		world_flush_client_sbuf( wld );
		close( wld->client_fd );
	}

	/* If there's anything left, put it in the buffer and process it */
	if( wld->auth_read[wa] > alen )
	{
		/* Copy it into the buffer */
		memcpy( wld->client_rbuffer, wld->auth_buf[wa] + alen,
			wld->auth_read[wa] - alen );
		/* Process the buffer */
		wld->client_rbfull = buffer_to_lines( wld->client_rbuffer,
			0, wld->auth_read[wa] - alen, wld->client_rlines );
	}
	else
		wld->client_rbfull = 0;

	/* Transfer connection */
	wld->client_fd = wld->auth_fd[wa];
	wld->auth_fd[wa] = -1;
	remove_auth_connection( wld, wa, 0 );

	/* Confirm the connection to the authenticating client */
	world_message_to_client( wld, NET_AUTHGOOD );

	/* Inform about waiting lines and pass. */
	wld->flags &= ~WLD_PASSTEXT;
	asprintf( &str, "%li lines waiting. Pass is off.",
			wld->buffered_text->count );
	world_message_to_client( wld, str );
	free( str );

	/* Do the MCP reset, so MCP is set up correctly */
	if( wld->server_fd != -1 )
		world_send_mcp_reset( wld );

	/* In case there was something left in the buffer, process that now */
	world_handle_client_queue( wld );
}



/* Handle any events related to the client connection. */
static void handle_client_fd( World *wld )
{
	int n, offset = wld->client_rbfull;
	char *s, *buffer = wld->client_rbuffer;

	/* If the bbuffer is full, pass the entire contents as one line */
	if( offset == NET_BBUFFER_LEN )
	{
		s = malloc( NET_BBUFFER_LEN + 2 );
		memcpy( s, buffer, NET_BBUFFER_LEN );
		s[NET_BBUFFER_LEN] = '\n';
		s[NET_BBUFFER_LEN + 1] = '\0';
		linequeue_append( wld->client_rlines,
				line_create( s, NET_BBUFFER_LEN + 1 ) );
		wld->client_rbfull = 0;
		offset = 0;
	}

	n = read( wld->client_fd, buffer + offset, NET_BBUFFER_LEN - offset );

	if( n == -1 && ( errno == EINTR || errno == EAGAIN ) )
		return;

	if( n == 0 || n == -1 )
	{
		/* Ok, the connection dropped dead */
		close( wld->client_fd );
		wld->client_fd = -1;
		wld->flags |= WLD_FDCHANGE;

		/* If there's something left in the buffer, process it */
		if( offset > 0 )
		{
			s = malloc( offset + 2 );
			memcpy( s, buffer, offset );
			s[offset] = '\n';
			s[offset + 1] = '\0';
			linequeue_append( wld->client_rlines,
					line_create( s, offset + 1 ) );
		}

		return;
	}

	wld->client_rbfull =
		buffer_to_lines( buffer, offset, n, wld->client_rlines );

	/* Actually do something with the received lines */
	world_handle_client_queue( wld );
}



/* Handle any events related to the server connection. */
static void handle_server_fd( World *wld )
{
	int err, n, offset = wld->server_rbfull;
	char *str, *s, *buffer = wld->server_rbuffer;

	/* If the bbuffer is full, pass the entire contents as one line */
	if( offset == NET_BBUFFER_LEN )
	{
		s = malloc( NET_BBUFFER_LEN + 2 );
		memcpy( s, buffer, NET_BBUFFER_LEN );
		s[NET_BBUFFER_LEN] = '\n';
		s[NET_BBUFFER_LEN + 1] = '\0';
		linequeue_append( wld->server_rlines,
				line_create( s, NET_BBUFFER_LEN + 1 ) );
		wld->server_rbfull = 0;
		offset = 0;		
	}

	n = read( wld->server_fd, buffer + offset, NET_BBUFFER_LEN - offset );

	if( n == -1 && ( errno == EINTR || errno == EAGAIN ) )
		return;

	if( n < 1 )
	{
		/* Ok, the connection dropped dead */
		err = errno;
		close( wld->server_fd );
		wld->server_fd = -1;
		wld->flags |= WLD_FDCHANGE;

		/* If there's something left in the buffer, process it */
		if( offset > 0 )
		{
			s = malloc( offset + 2 );
			memcpy( s, buffer, offset );
			s[offset] = '\n';
			s[offset + 1] = '\0';
			linequeue_append( wld->server_rlines,
					line_create( s, offset + 1 ) );
		}

		world_handle_server_queue( wld );

		/* Notify the client */
		asprintf( &str, "Connection to server lost (%s).",
				n ? strerror_n( err ) : "connection closed" );
		world_message_to_client( wld, str );
		world_message_to_client_buf( wld, str );
		free( str );

		wld->server_rbfull = 0;
		return;
	}

	wld->server_rbfull =
		buffer_to_lines( buffer, offset, n, wld->server_rlines );

	/* Actually do something with the received lines */
	world_handle_server_queue( wld );
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
extern void world_flush_client_sbuf( World *wld )
{
	Line *line = NULL;
	char *buffer = wld->client_sbuffer, *s;
	long offset = wld->client_sbfull, len;
	int r;

	/* If there is nothing to send, do nothing */
	if( wld->client_slines->length == 0 && wld->client_sbfull == 0 )
		return;

	/* If we're not connected, discard lines and exit */
	if( wld->client_fd == -1 )
	{
		linequeue_clear( wld->client_slines );
		return;
	}

	/* FIXME: It's scary how double free's are avoided. */
	/* FIXME: 1 write() per line. Subpar... */
	while( offset > 0 || ( line = linequeue_pop( wld->client_slines ) ) )
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
			len -= r == -1 ? 0 : r;
			/* Don't move if not needed */
			if( buffer != s )
				memmove( buffer, s, len );
			offset = len;
			break;
		}

		if( line )
			world_store_history_line( wld, line );
		offset = 0;
	}

	if( line )
		world_store_history_line( wld, line );

	wld->client_sbfull = offset;
}



/* Try to flush the queued lines into the send buffer, and the send buffer
 * to the server. If this fails, leave the contents of the buffer in the
 * queued lines, mark the fd as write-blocked, and signal an FD change.
 * If the socket is closed, discard contents, and announce
 * disconnectedness. */
extern void world_flush_server_sbuf( World *wld )
{
	Line *line = NULL;
	char *buffer = wld->server_sbuffer, *s;
	long offset = wld->server_sbfull, len;
	int r;

	/* If there is nothing to send, do nothing */
	if( wld->server_slines->length == 0 && wld->server_sbfull == 0 )
		return;

	/* If we're not connected, discard and notify client */
	if( wld->server_fd == -1 )
	{
		linequeue_clear( wld->server_slines );
		world_message_to_client( wld, "Not connected to server!" );
		return;
	}

	/* FIXME: It's scary how double free's are avoided. */
	/* FIXME: 1 write() per line. Subpar... */
	while( offset > 0 || ( line = linequeue_pop( wld->server_slines ) ) )
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
			len -= r == -1 ? 0 : r;
			/* Don't move if not needed */
			if( buffer != s )
				memmove( buffer, s, len );
			offset = len;
			break;
		}

		/* Free the line, if it exists */
		if( line )
			free( line->str );
		free( line );
		offset = 0;
	}

	/* Free the line, if it exists */
	if( line )
		free( line->str );
	free( line );

	wld->server_sbfull = offset;
}



#if 0
/* Doesn't handle partial writes... But I'm not happy with the current
 * either, so let's keep this around, just in case */
	while( ( line = linequeue_pop( wld->server_slines ) ) )
	{
		loff = 0;
		while( loff < line->len )
		{
			/* Copy the line (or as much as will fit) in the buf */
			todo = line->len - loff;
			if( todo > NET_BBUFFER_LEN - offset )
				todo = NET_BBUFFER_LEN - offset;
			memcpy( buffer + offset, line->str + loff, todo );
			loff += todo;
			offset += todo;

			/* If the buffer is full or last line, send it */
			if( offset == NET_BBUFFER_LEN || !line->next )
			{
				r = write( wld->server_fd, buffer, offset );
				if( r > 0 )
					offset -= r;
			}
		}
		free( line->str );
		free( line );
	}
#endif
