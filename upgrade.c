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
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "upgrade.h"
#include "world.h"
#include "misc.h"



extern int upgrade_server_start( World *wld, char **err )
{
	int listen_fd, master_fd;
	struct sockaddr_un sock_addr;

	sock_addr.sun_family = AF_UNIX;
	if( snprintf( sock_addr.sun_path, sizeof( sock_addr.sun_path ),
			"%s/%s/%s", get_homedir(), CONFIGDIR, "upgrade" )
			>= sizeof( sock_addr.sun_path ) )
	{
		xasprintf( err, "Socket pathname too long." );
		return 1;
	}

	listen_fd = socket( AF_UNIX, SOCK_STREAM, 0 );
	if( listen_fd < 0 )
	{
		xasprintf( err, "Creating socket failed: %s",
			strerror( errno ) );
		return 1;
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
		return 1;
	}

	if( listen( listen_fd, 1 ) < 0 )
	{
		xasprintf( err, "Listening on socket failed: %s",
			strerror( errno ) );
		return 1;
	}

	master_fd = accept( listen_fd, NULL, NULL );
	if( master_fd < 0 )
	{
		xasprintf( err, "Accepting new connection failed: %s",
			strerror( errno ) );
		return 1;
	}

	printf( "Woo, got connection!\n" );

	return 0;
}



extern int upgrade_client_start( World *wld )
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
		return 1;
	}

	slave_fd = socket( AF_UNIX, SOCK_STREAM, 0 );
	if( slave_fd < 0 )
	{
		world_msg_client( wld, "Creating socket failed: %s",
			strerror( errno ) );
		return 1;
	}

	if( connect( slave_fd, (struct sockaddr *) &sock_addr,
				sizeof( sock_addr.sun_path ) ) )
	{
		world_msg_client( wld, "Couldn't connect to upgrade socket: %s",
			strerror( errno ) );
		world_msg_client( wld, "Make sure a mooproxy is ready by "
			"running: mooproxy -uw %s", wld->name );
		return 1;
	}
	
	world_msg_client( wld, "Woo!" );
	return 0;
}
