/*
 *
 *  mooproxy - a buffering proxy for MOO connections
 *  Copyright (C) 2001-2011 Marcel L. Moreaux <marcelm@qvdr.net>
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



#define _XOPEN_SOURCE /* For crypt(). */
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <termios.h>

#include "crypt.h"
#include "misc.h"



static char *prompt_for_password( char *msg );



/* Used to encode 6-bit integers into human-readable ASCII characters */
static const char *seedchars =
		"./0123456789ABCDEFGHIJKLMNOPQRST"
		"UVWXYZabcdefghijklmnopqrstuvwxyz";




extern void prompt_to_md5hash( void )
{
	struct timeval tv1, tv2;
	char salt[] = "$1$........";
	char *password, *password2;

	gettimeofday( &tv1, NULL );
	password = prompt_for_password( "Authentication string: " );
	password2 = prompt_for_password( "Re-type authentication string: " );
	gettimeofday( &tv2, NULL );

	printf( "\n" );

	/* No typos allowed! */
	if( strcmp( password, password2 ) )
	{
		printf( "The supplied strings are not equal, aborting.\n" );
		free( password );
		free( password2 );
		return;
	}

	/* FIXME: I should really use /dev/random or something for this. */
	/* Construct the salt.
	 * Bits 47..36: lowest 12 bits of mooproxy's PID. */
	salt[ 3] = seedchars[( getpid() / 64 ) % 64];
	salt[ 4] = seedchars[getpid() % 64];
	/* Bits 35..24: lowest 12 bits of time(). */
	salt[ 5] = seedchars[( time( NULL ) / 64 ) % 64 ];
	salt[ 6] = seedchars[time( NULL ) % 64];
	/*  bits 23..12:  lowest 12 bits of microseconds in gettimeofday()
	 *                _before_ prompting for strings. */
	salt[ 7] = seedchars[( tv1.tv_usec / 64 ) % 64];
	salt[ 8] = seedchars[tv1.tv_usec % 64];
	/*  bits 11..0:  lowest 12 bits of microseconds in gettimeofday()
	 *                _after_ prompting for strings. */
	salt[ 9] = seedchars[( tv2.tv_usec / 64 ) % 64];
	salt[10] = seedchars[tv2.tv_usec % 64];

	printf( "MD5 hash: %s\n", crypt( password, salt ) );

	if( !strcmp( password, "" ) )
		printf( "Note: this is a hash of an empty string; "
				"it will be refused by mooproxy.\n" );

	free( password );
	free( password2 );
	return;
}



/* Print msg to stdout, disable terminal input echo, and read password from 
 * stdin. */
char *prompt_for_password( char *msg )
{
	struct termios tio_orig, tio_new;
	int n;
	char *password = malloc( NET_MAXAUTHLEN + 1 );

	/* Check if we're connected to a terminal. */
	if( !isatty( 0 ) )
	{
		fprintf( stderr, "Password prompting requires a terminal.\n" );
		exit( EXIT_FAILURE );
	}

	/* Get terminal settings. */
	if( tcgetattr( 0, &tio_orig ) != 0 )
	{
		fprintf( stderr, "Failed to disable terminal echo "
				"(tcgetattr said %s).", strerror( errno ) );
		exit( EXIT_FAILURE );
	}

	/* Disable terminal input echo. */
	memcpy( &tio_new, &tio_orig, sizeof( struct termios ) );
	tio_new.c_lflag &= ~ECHO;
	if( tcsetattr( 0, TCSAFLUSH, &tio_new ) != 0 )
	{
		fprintf( stderr, "Failed to disable terminal echo "
				"(tcsetattr said %s).", strerror( errno ) );
		exit( EXIT_FAILURE );
	}

	/* Write the prompt. */
	write( 0, msg, strlen( msg ) );

	/* Read the password (and bail out if something fails). */
	n = read( 0, password, NET_MAXAUTHLEN );
	if( n < 0 )
	{
		fprintf( stderr, "Failed to read password from terminal "
				"(read said %s).", strerror( errno ) );
		exit( EXIT_FAILURE );
	}

	/* nul-terminate the read password. */
	password[n] = '\0';

	/* Strip off any trailing \n. */
	if( n >= 1 && password[n - 1] == '\n' )
		password[n - 1] = '\0';

	/* Strip off any trailing \r. */
	if( n >= 2 && password[n - 2] == '\r' )
		password[n - 2] = '\0';

	/* Restore terminal settings. */
	tcsetattr( 0, TCSAFLUSH, &tio_orig );

	/* Write a newline to the terminal. */
	write( 0, "\n", sizeof( "\n" ) - 1 );

	return password;
}



/* An MD5 hash looks like $1$ssssssss$hhhhhhhhhhhhhhhhhhhhhh,
 * where 's' are salt characters, and 'h' hash characters. */
extern int looks_like_md5hash( char *str )
{
	int i;

	/* A hash should be 34 characters long. */
	if( strlen( str ) != 34 )
		return 0;

	/* A hash should start with "$1$", and its 12th char should be '$' */
	if( str[0] != '$' || str[1] != '1' || str[2] != '$' || str[11] != '$' )
		return 0;

	/* The salt should contain only valid characters. */
	for( i = 3; i < 11; i++ )
		if( strchr( seedchars, str[i] ) == NULL )
			return 0;

	/* The hash should contain only valid characters. */
	for( i = 12; i < 34; i++ )
		if( strchr( seedchars, str[i] ) == NULL )
			return 0;

	/* All tests passed, it's probably ok. */
	return 1;
}



extern int world_match_authentication( World *wld, const char *str )
{
	/* A literal exists. Check against that, it's way faster. */
	if( wld->auth_literal != NULL )
		return !strcmp( wld->auth_literal, str );

	/* Match against the MD5 hash */
	if( !match_string_md5hash( str, wld->auth_hash ) )
		return 0;

	/* If we got here, str is valid. Cache the literal for later. */
	wld->auth_literal = xstrdup( str );
	return 1;
}



extern int match_string_md5hash( const char *str, const char *md5hash )
{
	char *strhash;

	strhash = crypt( str, md5hash );

	return !strcmp( md5hash, strhash );
}
