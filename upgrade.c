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

#include "upgrade.h"
#include "world.h"
#include "misc.h"



extern int upgrade_server_start( World *wld, char **err )
{
	int listen_fd, master_fd;
	struct sockaddr_un sock_addr;

	sock_addr.sun_family = AF_UNIX;
	strncpy( sock_addr.sun_path, "", sizeof( sock_addr.sun_path ) );
	// local.sun_path[UNIX_PATH_MAX - 1] = '\0';

	listen_fd = socket( AF_UNIX, SOCK_STREAM, 0 );
	if( listen_fd < 0 )
	{
		xasprintf( err, "Creating socket failed: %s",
				strerror( errno ) );
		return 1;
	}





	return 0;
}
