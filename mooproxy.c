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



#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "daemon.h"
#include "global.h"
#include "config.h"
#include "world.h"
#include "network.h"



static void die( int, char * );



int main( int argc, char **argv )
{
	World *world = NULL;
	int i;
	char *err, *worldname = NULL;
	time_t now = time( NULL );
	
	printf( "Starting mooproxy " VERSIONSTR " at %s", ctime( &now ) );

	set_up_signal_handlers();

	i = parse_command_line_options( argc, argv, &worldname, &err );
	if( i != EXIT_OK )
		die( i, err );
	
	if( worldname == NULL || worldname[0] == '\0' )
		die( EXIT_NOWORLD, "You must supply a world name." );

	world = world_create( worldname );

	world_configfile_from_name( world );

	if( create_configdirs( &err ) )
		die( EXIT_CONFIGDIRS, err );

	printf( "Loading config...\n" );
	i = world_load_config( world, &err );
	if( i != EXIT_OK )
		die( i, err );
	
	/* Refuse if nonexistent / empty authstring */
	if( !world->authstring || !world->authstring[0] )
		die( EXIT_NOAUTH, "Authstring must be non-empty."
				"Refusing to start." );

	printf( "Binding port...\n" );
	i = world_bind_port( world, &err );
	if( i != EXIT_OK )
		die( i, err );

	printf( "opened world %s\n", world->name );
	printf( "Ready for connections\n" );

	world_mainloop( world );
	
	world_destroy( world );
	
	exit( EXIT_OK );
}



static void die( int ret, char *err )
{
	fprintf( stderr, "%s\n", err );
	exit( ret );
}
