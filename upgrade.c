/*
 *
 *  mooproxy - a buffering proxy for moo-connections
 *  Copyright (C) 2001-2007 Marcel L. Moreaux <marcelm@luon.net>
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



#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "upgrade.h"
#include "world.h"
#include "misc.h"



#define UPGRADE_HELO 0x4d505859
#define UPGRADE_ERROR 0x00000001
#define UPGRADE_OK 0x00000002
#define UPGRADE_OPTION 0x00000003



static int server_start( World *wld, char **err );
static int server_wait( World *wld, char **err );
static int client_start( World *wld );
static void client_handshake( World *wld, int fd );



extern int upgrade_do_server( World *wld, char **err )
{
	int fd;

	fd = server_start( wld, err );
	if( fd == -1 )
		return 1;

	return server_wait( wld, err );
}



extern void upgrade_do_client_init( World *wld )
{
	int fd;

	fd = client_start( wld );
	client_handshake( wld, fd );
}



extern void upgrade_do_client_transfer( World *wld )
{
}



static int server_start( World *wld, char **err )
{
	int listen_fd, master_fd;
	struct sockaddr_un sock_addr;

	sock_addr.sun_family = AF_UNIX;
	if( snprintf( sock_addr.sun_path, sizeof( sock_addr.sun_path ),
			"%s/%s/%s", get_homedir(), CONFIGDIR, "upgrade" )
			>= sizeof( sock_addr.sun_path ) )
	{
		xasprintf( err, "Error: upgrade socket pathname too long." );
		return -1;
	}

	listen_fd = socket( AF_UNIX, SOCK_STREAM, 0 );
	if( listen_fd < 0 )
	{
		xasprintf( err, "Creating socket failed: %s",
			strerror( errno ) );
		return -1;
	}

	if( bind( listen_fd, (struct sockaddr *) &sock_addr,
			sizeof( struct sockaddr_un ) ) < 0 )
	{
		/* FIXME: better message, or unlink()? */
		if( errno == EADDRINUSE )
			xasprintf( err, "Socket already in use; only one "
				"upgrade-instance of mooproxy may be run." );
		else
			xasprintf( err, "Binding to socket failed: %s",
				strerror( errno ) );
		close( listen_fd );
		return -1;
	}

	if( listen( listen_fd, 1 ) < 0 )
	{
		xasprintf( err, "Listening on socket failed: %s",
			strerror( errno ) );
		close( listen_fd );
		return -1;
	}

	master_fd = accept( listen_fd, NULL, NULL );
	if( master_fd < 0 )
	{
		xasprintf( err, "Accepting new connection failed: %s",
			strerror( errno ) );
		close( listen_fd );
		return -1;
	}

	close( listen_fd );
	return master_fd;
}



static int server_wait( World *wld, char **err )
{


	return 0;
}



static int client_start( World *wld )
{
	int slave_fd;
	struct sockaddr_un sock_addr;

	sock_addr.sun_family = AF_UNIX;
	if( snprintf( sock_addr.sun_path, sizeof( sock_addr.sun_path ),
			"%s/%s/%s", get_homedir(), CONFIGDIR, "upgrade" )
			>= sizeof( sock_addr.sun_path ) )
	{
		world_msg_client( wld, "Error: upgrade socket pathname too "
			"long." );
		return -1;
	}

	slave_fd = socket( AF_UNIX, SOCK_STREAM, 0 );
	if( slave_fd < 0 )
	{
		world_msg_client( wld, "Creating socket failed: %s",
			strerror( errno ) );
		close( slave_fd );
		return -1;
	}

	if( connect( slave_fd, (struct sockaddr *) &sock_addr,
				sizeof( sock_addr.sun_path ) ) )
	{
		world_msg_client( wld, "Couldn't connect to upgrade socket: %s",
			strerror( errno ) );
		world_msg_client( wld, "Make sure an upgrade is ready by "
			"running: mooproxy -uw %s", wld->name );
		close( slave_fd );
		return -1;
	}

	return slave_fd;
}



static void client_handshake( World *wld, int fd )
{

}
