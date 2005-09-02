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



#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "accessor.h"
#include "world.h"
#include "log.h"
#include "misc.h"
#include "crypt.h"



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
	/* If it's set from the user, we're already bound to a port.
	 * And for now, we don't rebind while running. */
	if( src == ASRC_USER )
		return SET_KEY_PERM;
 
	return set_long_ranged( value, &wld->listenport, err, 1, 65535,
			"Port numbers" );
}



extern int aset_auth_md5hash( World *wld, char *key, char *value,
		int src, char **err )
{
	char *val, *origval, *newhash, *oldliteral = NULL;
	int ret;

	/* Get the first word (the new hash) in newhash.
	 * Also, get the rest of the string (the old literal if coming from
	 * the user, empty if coming from file) in oldliteral */
	origval = val = xstrdup( value );
	newhash = xstrdup( get_one_word( &val ) );
	val = trim_whitespace( val );
	val = remove_enclosing_quotes( val );
	oldliteral = xstrdup( val );
	free( origval );

	if( !looks_like_md5hash( newhash ) )
	{
		*err = xstrdup( "That doesn't look like a valid MD5 hash." );
		free( newhash );
		free( oldliteral );
		return SET_KEY_BAD;
	}

	if( match_string_md5hash( "", newhash ) )
	{
		*err = xstrdup( "The authentication string may not be empty." );
		free( newhash );
		free( oldliteral );
		return SET_KEY_BAD;
	}

	if( src == ASRC_USER )
	{
		if( !strcmp( oldliteral, "" ) )
		{
			*err = xstrdup( "The new hash must be followed by the "
					"old literal authentication string." );
			free( newhash );
			free( oldliteral );
			return SET_KEY_BAD;
		}

		if( !match_string_md5hash( oldliteral, wld->auth_md5hash ) )
		{
			*err = xstrdup( "The old literal authentication string"
					" was not correct." );
			free( newhash );
			free( oldliteral );
			return SET_KEY_BAD;
		}
	}

	free( wld->auth_literal );
	wld->auth_literal = NULL;

	ret = set_string( newhash, &wld->auth_md5hash, err );

	free( newhash );
	free( oldliteral );

	return ret;
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



extern int aset_autologin( World *wld, char *key, char *value,
		int src, char **err )
{
	return set_bool( value, &wld->autologin, err );
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
	return set_bool( value, &wld->logging_enabled, err );
}



extern int aset_context_on_connect( World *wld, char *key, char *value,
		int src, char **err )
{
	return set_long_ranged( value, &wld->context_on_connect, err, 0,
			LONG_MAX / 1024, "Context on connect" );
}



extern int aset_max_buffered_size( World *wld, char *key, char *value,
		int src, char **err )
{
	return set_long_ranged( value, &wld->max_buffered_size, err, 0,
			LONG_MAX / 1024, "Max buffered size" );
}



extern int aset_max_history_size( World *wld, char *key, char *value,
		int src, char **err )
{
	return set_long_ranged( value, &wld->max_history_size, err, 0,
			LONG_MAX / 1024, "Max history size" );
}



extern int aset_strict_commands( World *wld, char *key, char *value,
		int src, char **err )
{
	return set_bool( value, &wld->strict_commands, err );
}



/* ----------------------------- getters -------------------------------- */



extern int aget_listenport( World *wld, char *key, char **value, int src )
{
	return get_long( wld->listenport, value );
}



extern int aget_auth_md5hash( World *wld, char *key, char **value, int src )
{
	/* For security reasons, the user may not view the MD5 hash. */
	if( src == ASRC_USER )
		return GET_KEY_PERM;

	return get_string( wld->auth_md5hash, value );
}



extern int aget_dest_host( World *wld, char *key, char **value, int src )
{
	return get_string( wld->dest_host, value );
}



extern int aget_dest_port( World *wld, char *key, char **value, int src )
{
	return get_long( wld->dest_port, value );
}



extern int aget_autologin( World *wld, char *key, char **value, int src )
{
	return get_bool( wld->autologin, value );
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



/* ------------------------- helper functions --------------------------- */

/* The setters return SET_KEY_OK or SET_KEY_BAD.
 * When returning SET_KEY_BAD, err will be set. Err should be free()d.
 * The setters do not consume the src string.
 *
 * The getters always return GET_KEY_OK.
 * The string they place in dest should be free()d. */



static int set_string( char *src, char **dest, char **err )
{
	free( *dest );
	*dest = xstrdup( src );
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

	*err = xstrdup( "Integers must be simple decimal or hexadecimal "
			"numbers." );
	return SET_KEY_BAD;
}



/* Like set_long, but the allowed value is limited in range. */
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
		xasprintf( err, "%s must be a positive number.", name );
	else
		xasprintf( err, "%s must be between %li and %li inclusive.",
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

	*err = xstrdup( "Booleans must be one of true, yes, on, "
			"false, no, or off." );
	return SET_KEY_BAD;
}



static int get_string( char *src, char **dest )
{
	xasprintf( dest, "\"%s\"", src );
	return GET_KEY_OK;
}



static int get_long( long src, char **dest )
{
	xasprintf( dest, "%li", src );
	return GET_KEY_OK;
}



static int get_bool( int src, char **dest )
{
	*dest = xstrdup( src ? "true" : "false" );
	return GET_KEY_OK;
}
