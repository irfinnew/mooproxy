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
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h> /* FIXME: remove */

#include "world.h"
#include "log.h"



static void log_line( World *, char *, long );
static char *strip_ansi( char *, long, long * );



extern int world_log_init( World *wld )
{
	char *file, *home = get_homedir();
	int fd;

	world_log_close( wld );

	asprintf( &file, "%s.mooproxy/logs/%s - %s.log", home, wld->name,
			time_string( time( NULL ), "%F" ) );
	free( home );

	/* FIXME: error checking */
	fd = open( file, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR );

	wld->log_fd = fd;

	free( file );
	return 0;
}



extern void world_log_close( World *wld )
{
	if( wld->log_fd != -1 )
		close( wld->log_fd );

	wld->log_fd = -1;
}



extern void world_log_server_line( World *wld, Line *line )
{
	log_line( wld, line->str, line->len );
}



static void log_line( World *wld, char *ostr, long olen )
{
	char *str;
	long len;

	if( wld->log_fd == -1 )
		return;

	str = strip_ansi( ostr, olen, &len );

	/* FIXME: error checking */
	write( wld->log_fd, str, len );

	free( str );
}



static char *strip_ansi( char *orig, long len, long *nlen )
{
	char *str, *newstr = malloc( len + 2 );
	int escape = 0;

	for( str = newstr; *orig; orig++ )
	{
		if( escape == 0 && *orig == '' )
			escape = 1;
		if( escape == 1 && *orig == '[' )
			escape = 2;
		if( escape == 0 && isprint( *orig ) )
			*str++ = *orig;
		if( escape == 1 && *orig != '' )
			escape = 0;
		if( escape == 2 && isalpha( *orig ) )
			escape = 0;
	}
	*str++ = '\n';
	*str = '\0';

	*nlen = str - newstr;
	return newstr;
}
