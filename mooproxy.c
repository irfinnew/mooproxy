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



#include "daemon.h"
#include "global.h"
#include "config.h"
#include "world.h"
#include "network.h"



int main( int argc, char **argv )
{
	World *world = NULL;
	int i;
	char *err, *worldname = NULL;
	
	printf( "Starting mooproxy " VERSIONSTR "\n" );

	set_up_signal_handlers();

	i = parse_command_line_options( argc, argv, &worldname, &err );
	if( i != EXIT_OK )
	{
		fprintf( stderr, "%s\n", err );
		exit( i );
	}
	
	if( worldname == NULL || worldname[0] == '\0' )
	{
		fprintf( stderr, "You must supply a world name.\n" );
		exit( EXIT_NOWORLD );
	}

	world = world_create( worldname );

	world_configfile_from_name( world );

	if( create_configdirs( &err ) )
	{
		fprintf( stderr, "%s\n", err );
		exit( EXIT_CONFIGDIRS );
	}

	printf( "Loading config...\n" );
	i = world_load_config( world, &err );
	if( i != EXIT_OK )
	{
		fprintf( stderr, "%s\n", err );
		exit( i );
	}
	
	printf( "Binding port...\n" );
	i = world_bind_port( world, &err );
	if( i != EXIT_OK )
	{
		fprintf( stderr, "%s\n", err );
		exit( i );
	}

	printf( "opened world %s\n", world->name );

	world_mainloop( world );
	
	world_destroy( world );
	
	exit( EXIT_OK );
}
