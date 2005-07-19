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
#include <limits.h>

#include "accessor.h"
#include "world.h"
#include "log.h"



static int set_string( char *, char **, char ** );
static int set_long( char *, long *, char ** );
static int set_long_ranged( char *, long *, char **, long, long, char * );
static int set_bool( char *, int *, char ** );
static int get_string( char *, char ** );
static int get_long( long, char ** );
static int get_bool( int, char ** );



extern int aset_listenport( World *wld, char *key, char *value,
		int src, char **err )
{
	/* FIXME: actually listen on other port */
	return set_long_ranged( value, &wld->listenport, err, 1, 65535,
			"Port numbers" );
}



extern int aset_authstring( World *wld, char *key, char *value,
		int src, char **err )
{
	char *str = NULL;

	if( set_string( value, &str, err ) == SET_KEY_BAD )
		return SET_KEY_BAD;

	if( *str != '\0' )
	{
		free( wld->authstring );
		wld->authstring = str;
		return SET_KEY_OK;
	}

	free( str );
	*err = strdup( "The authstring may not be empty." );
	return SET_KEY_BAD;
}



extern int aset_dest_host( World *wld, char *key, char *value,
		int src, char **err )
{
	return set_string( value, &wld->dest_host, err );
}



extern int aset_dest_port( World *wld, char *key, char *value,
		int src, char **err )
{
	return set_long_ranged( value, &wld->dest_port, err, 1, 65535,
			"Port numbers" );
}



extern int aset_commandstring( World *wld, char *key, char *value,
		int src, char **err )
{
	return set_string( value, &wld->commandstring, err );
}



extern int aset_infostring( World *wld, char *key, char *value,
		int src, char **err )
{
	return set_string( value, &wld->infostring, err );
}



extern int aset_logging_enabled( World *wld, char *key, char *value,
		int src, char **err )
{
	if( set_bool( value, &wld->logging_enabled, err ) == SET_KEY_BAD )
		return SET_KEY_BAD;

	world_log_init( wld );
	return SET_KEY_OK;
}



extern int aset_context_on_connect( World *wld, char *key, char *value,
		int src, char **err )
{
	return set_long_ranged( value, &wld->context_on_connect, err, 0,
			LONG_MAX, "Context on connect" );
}



extern int aset_max_buffered_size( World *wld, char *key, char *value,
		int src, char **err )
{
	return set_long_ranged( value, &wld->max_buffered_size, err, 0,
			LONG_MAX, "Max buffered size" );
}



extern int aset_max_history_size( World *wld, char *key, char *value,
		int src, char **err )
{
	return set_long_ranged( value, &wld->max_history_size, err, 0,
			LONG_MAX, "Max history size" );
}



extern int aset_strict_commands( World *wld, char *key, char *value,
		int src, char **err )
{
	return set_bool( value, &wld->strict_commands, err );
}



/* getters */



extern int aget_listenport( World *wld, char *key, char **value, int src )
{
	return get_long( wld->listenport, value );
}



extern int aget_authstring( World *wld, char *key, char **value, int src )
{
	if( src == ASRC_USER )
		return GET_KEY_PERM;

	return get_string( wld->authstring, value );
}



extern int aget_dest_host( World *wld, char *key, char **value, int src )
{
	return get_string( wld->dest_host, value );
}



extern int aget_dest_port( World *wld, char *key, char **value, int src )
{
	return get_long( wld->dest_port, value );
}



extern int aget_commandstring( World *wld, char *key, char **value, int src )
{
	return get_string( wld->commandstring, value );
}



extern int aget_infostring( World *wld, char *key, char **value, int src )
{
	return get_string( wld->infostring, value );
}



extern int aget_logging_enabled( World *wld, char *key, char **value, int src )
{
	return get_bool( wld->logging_enabled, value );
}



extern int aget_context_on_connect( World *wld, char *key, char **value,
		int src )
{
	return get_long( wld->context_on_connect, value );
}



extern int aget_max_buffered_size( World *wld, char *key, char **value,
		int src )
{
	return get_long( wld->max_buffered_size, value );
}



extern int aget_max_history_size( World *wld, char *key, char **value, int src )
{
	return get_long( wld->max_history_size, value );
}



extern int aget_strict_commands( World *wld, char *key, char **value, int src )
{
	return get_bool( wld->strict_commands, value );
}



/* Helper functions.
 * The setters only return SET_KEY_OK or SET_KEY_BAD.
 * The getters only return GET_KEY_OK. */



static int set_string( char *src, char **dest, char **err )
{
	free( *dest );
	*dest = strdup( src );
	return SET_KEY_OK;
}



static int set_long( char *src, long *dest, char **err )
{
	char *endptr;
	long val;

	val = strtol( src, &endptr, 0 );

	/* Yay! String is non-empty and valid number. */
	if( *src != '\0' && *endptr == '\0' )
	{
		*dest = val;
		return SET_KEY_OK;
	}

	*err = strdup( "Integers must be simple decimal or hexadecimal "
			"numbers." );
	return SET_KEY_BAD;
}



static int set_long_ranged( char *src, long *dest, char **err, long low,
		long high, char *name )
{
	long val;

	if( set_long( src, &val, err ) == SET_KEY_BAD )
		return SET_KEY_BAD;

	if( val >= low && val <= high )
	{
		*dest = val;
		return SET_KEY_OK;
	}

	if( low == 0 && high == LONG_MAX )
		asprintf( err, "%s must be a positive number.", name );
	else
		asprintf( err, "%s must be between %li and %li inclusive.",
			name, low, high );

	return SET_KEY_BAD;
}



static int set_bool( char *src, int *dest, char **err )
{
	int val;

	val = true_or_false( src );

	if( val != -1 )
	{
		*dest = val;
		return SET_KEY_OK;
	}

	*err = strdup( "Booleans must be true/yes/on/1 or false/no/off/0." );
	return SET_KEY_BAD;
}



static int get_string( char *src, char **dest )
{
	*dest = strdup( src );
	return GET_KEY_OK;
}



static int get_long( long src, char **dest )
{
	asprintf( dest, "%li", src );
	return GET_KEY_OK;
}



static int get_bool( int src, char **dest )
{
	*dest = strdup( src ? "true" : "false" );
	return GET_KEY_OK;
}
