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
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "global.h"
#include "config.h"
#include "accessor.h"
#include "misc.h"
#include "world.h"



/* The separator for keys and values in the configfile. */
#define SEPARATOR '='
/* The maximum length of the configuration file, in bytes. */
#define MAX_CONFIG_LENGTH (32 * 1024)



static int set_key_value( World *, char *, char *, char ** );
static int set_key_internal( World *, char *, char *, int, char ** );
static int get_key_internal( World *, char *, char **, int );
static int attempt_createdir( char *, char ** );



/* The list of settable/gettable options. The hidden flag indicates if the
 * option is accessible to the user. The setters and getters are functions
 * that actually set or query the option, and are located in accessor.c. */
static const struct
{
	int hidden;
	char *keyname;
	int (*setter)( World *, char *, char *, int, char ** );
	int (*getter)( World *, char *, char **, int );
} key_db[] = {

	{ 0, "listenport",
		&aset_listenport,
		&aget_listenport },
	{ 0, "auth_md5hash",
		&aset_auth_md5hash,
		&aget_auth_md5hash },

	{ 0, "host",
		&aset_dest_host,
		&aget_dest_host },
	{ 0, "port",
		&aset_dest_port,
		&aget_dest_port },
	{ 0, "autologin",
		&aset_autologin,
		&aget_autologin },

	{ 0, "commandstring",
		&aset_commandstring,
		&aget_commandstring },
	{ 0, "infostring",
		&aset_infostring,
		&aget_infostring },

	{ 0, "logging_enabled",
		&aset_logging_enabled,
		&aget_logging_enabled },

	{ 0, "context_on_connect",
		&aset_context_on_connect,
		&aget_context_on_connect },
	{ 0, "max_buffered_size",
		&aset_max_buffered_size,
		&aget_max_buffered_size },
	{ 0, "max_history_size",
		&aset_max_history_size,
		&aget_max_history_size },

	{ 0, "strict_commands",
		&aset_strict_commands,
		&aget_strict_commands },

	{ 0, NULL, NULL, NULL }
};

/* Command line options. */
static const char short_opts[] = ":hVLw:m";
static const struct option long_opts[] = {
	{ "help", 0, NULL, 'h' },
	{ "version", 0, NULL, 'V' },
	{ "license", 0, NULL, 'L' },
	{ "world", 1, NULL, 'w' },
	{ "md5crypt", 0, NULL, 'm' },
	{ NULL, 0, NULL, 0 }
};



extern int parse_command_line_options( int argc, char **argv,
		char **worldname, char **err )
{
	int result;

	opterr = 0;

	while( ( result = getopt_long( argc, argv, short_opts, long_opts,
			NULL ) ) != -1 )
	{
		switch( result )
		{
		/* -h, --help */
		case 'h':
		return PARSEOPTS_HELP;
		break;

		/* -L, --license */
		case 'L':
		return PARSEOPTS_LICENSE;
		break;

		/* -V, --version */
		case 'V':
		return PARSEOPTS_VERSION;
		break;

		/* -m, --md5crypt */
		case 'm':
		return PARSEOPTS_MD5CRYPT;
		break;

		/* -w, --world */
		case 'w':
		free( *worldname );
		*worldname = xstrdup( optarg );
		break;

		/* Unrecognised */
		case '?':
		/* On unrecognised short option, optopt is the unrecognized
		 * char. On unrecognised long option, optopt is 0 and optind
		 * contains the number of the unrecognized argument.
		 * Sigh... */
		if( optopt == 0 )
			xasprintf( err, "Unrecognized option: `%s'. Use "
					"--help for help.", argv[optind - 1] );
		else
			xasprintf( err, "Unrecognized option: `-%c'. Use "
					"--help for help.", optopt );
		return PARSEOPTS_ERROR;
		break;

		/* Option without required argument. */
		case ':':
		xasprintf( err, "Option `%s' expects an argument.",
				argv[optind - 1] );
		return PARSEOPTS_ERROR;
		break;

		/* We shouldn't get here */
		default:
		break;
		}
	}

	/* Any non-options left? */
	if( optind < argc )
	{
		xasprintf( err, "Unexpected argument: `%s'. Use --help for "
				"help.", argv[optind] );
		return PARSEOPTS_ERROR;
	}

	return PARSEOPTS_START;
}



/* FIXME: this isn't pretty. Rewrite some time. */
extern int world_load_config( World *wld, char **err )
{
	int fd, i;
	char *line, *key, *value, *s, *contents, *cnt, *tmp;
	long wline;
	size_t l;

	/* We need this to be NULL or valid error later on. */
	*err = NULL;

	if( wld->configfile == NULL )
	{
		xasprintf( err, "Could not open (null).\nNo such world, `%s'",
				wld->name );
		return 1;
	}

	fd = open( wld->configfile, O_RDONLY );
	if( fd == -1 )
	{
		xasprintf( err, "Error opening `%s': %s\nNo such world, `%s'",
			wld->configfile, strerror( errno ), wld->name );
		return 1;
	}

	/* We read the entire configuration file in one go, and then parse
	 * everything in memory. */
	contents = cnt = xmalloc( MAX_CONFIG_LENGTH + 1 );
	i = read( fd, contents, MAX_CONFIG_LENGTH );

	/* Error! */
	if( i == - 1 )
	{
		xasprintf( err, "Error reading from `%s': %s\nNo such world, "
			"`%s'", wld->configfile, strerror( errno ), wld->name );
		free( contents );
		return 1;
	}

	/* If the file is longer than we can read, refuse as well. */
	if( i == MAX_CONFIG_LENGTH )
	{
		xasprintf( err, "Error: `%s' is greater than %li bytes.",
				wld->configfile, MAX_CONFIG_LENGTH );
		free( contents );
		return 1;
	}

	/* NUL-terminate the string. */
	contents[i] = '\0';
	close( fd );

	for( wline = 1; ( line = read_one_line( &cnt ) ); wline++ )
	{
		/* Get a line, trim the whitespace, and parse. */
		line = trim_whitespace( line );

		/* If the line is empty, or starts with #, ignore */
		if( line[0] == '\0' || line[0] == '#' )
			;
		else if( ( s = strchr( line, SEPARATOR ) ) )
		{
			/* Isolate the key and value */
			key = xmalloc( ( l = s - line ) + 1 );
			strncpy( key, line, l );
			key[l] = '\0';
			value = xmalloc( ( l = strlen( line ) - l - 1 ) + 1 );
			strncpy( value, ++s, l );
			value[l] = '\0';

			/* Trim key and value */
			key = trim_whitespace( key );
			value = trim_whitespace( value );

			/* Strip off a single leading and trailing quote */
			value = remove_enclosing_quotes( value );

			/* Try and set the key */
			switch( set_key_value( wld, key, value, err ) )
			{
				case SET_KEY_NF:
				xasprintf( err, "%s: line %li: unknown key `%s'"
					, wld->configfile, wline, key );
				break;

				case SET_KEY_PERM:
				xasprintf( err, "%s: line %li: setting key `%s'"
					" not allowed.", wld->configfile,
					wline, key );
				break;

				case SET_KEY_BAD:
				tmp = *err;
				xasprintf( err, "%s: line %li: setting key `%s'"
					": %s", wld->configfile, wline,
					key, tmp );
				free( tmp );
				break;
			}

			/* Clean up */
			free( key );
			free( value );

			if( *err )
			{
				free( contents );
				free( line );
				return 1;
			}
		}
		else
		{
			/* We don't understand that! */
			xasprintf( err, "%s: line %li, parse error: `%s'", 
				wld->configfile, wline, line );
			free( contents );
			free( line );
			return 1;
		}

		free( line );
	}

	free( contents );
	return 0;
}



extern int world_get_key_list( World *wld, char ***list )
{
	int i, num = 0;
	*list = NULL;

	for( i = 0; key_db[i].keyname; i++ )
	{
		/* Hidden? Skip. */
		if( key_db[i].hidden )
			continue;

		*list = xrealloc( *list, ++num * sizeof( char * ) );
		( *list )[num - 1] = xstrdup( key_db[i].keyname );
	}

	return num;
}



extern int world_set_key( World *wld, char *key, char *value, char **err )
{
	return set_key_internal( wld, key, value, ASRC_USER, err );
}



extern int world_get_key( World *wld, char *key, char **value )
{
	return get_key_internal( wld, key, value, ASRC_USER );
}



/* This function is like world_set_key(), but internal to this file.
 * It's for setting from file, not from the user. */
static int set_key_value( World *wld, char *key, char *value, char **err )
{
	return set_key_internal( wld, key, value, ASRC_FILE, err );
}



/* Attempts to set a single key to a new value.
 * For the interface, see world_set_key().
 * Additional arguments:
 *   src:  One of ASRC_FILE, ASRC_USER. Indicates if the set request
 *         is coming from the config file or from the user. */
static int set_key_internal( World *wld, char *key, char *value, int src,
		char **err )
{
	int i;

	for( i = 0; key_db[i].keyname; i++ )
		if( !strcmp( key, key_db[i].keyname ) )
		{
			if( key_db[i].hidden && src == ASRC_USER )
				return SET_KEY_NF;

			return (*key_db[i].setter)( wld, key, value, src, err );
		}

	return SET_KEY_NF;
}



/* Queries a single key. For the interface, see world_get_key().
 * Additional arguments:
 *   src:  One of ASRC_FILE, ASRC_USER. Indicates if the request
 *         is coming from the config file or from the user. */
static int get_key_internal( World *wld, char *key, char **value, int src )
{
	int i;

	for( i = 0; key_db[i].keyname; i++ )
		if( !strcmp( key, key_db[i].keyname ) )
		{
			if( key_db[i].hidden )
				return GET_KEY_NF;

			return (*key_db[i].getter)( wld, key, value, src );
		}

	return GET_KEY_NF;
}



extern int create_configdirs( char **err )
{
	if( attempt_createdir( CONFIGDIR, err ) != 0 )
		return 1;
	if( attempt_createdir( WORLDSDIR, err ) != 0 )
		return 1;
	if( attempt_createdir( LOGSDIR, err ) != 0 )
		return 1;

	return 0;
}



/* Attempt to create the directory ~/dirname.
 * On succes, return 0.
 * On failure, return non-zero, and put error in err (err should be free()d) */
static int attempt_createdir( char *dirname, char **err )
{
	char *path, *homedir = get_homedir();
	struct stat fileinfo;

	/* Construct the path */
	path = xmalloc( strlen( homedir ) + strlen( dirname ) + 1 );
	strcpy( path, homedir );
	strcat( path, dirname );
	free( homedir );

	if( !mkdir( path, S_IRUSR | S_IWUSR | S_IXUSR ) )
	{
		free( path );
		return 0;
	}

	/* If there was an error other than "already exists", complain */
	if( errno != EEXIST )
	{
		xasprintf( err, "Could not create directory `%s': %s",
				path, strerror( errno ) );
		free( path );
		return 1;
	}

	/* The directory already existed. But is it really a dir? */
	if( stat( path, &fileinfo ) == -1 )
	{
		xasprintf( err, "Could not stat `%s': %s",
				path, strerror( errno ) );
		free( path );
		return 1;
	}
	
	if( !S_ISDIR( fileinfo.st_mode ) )
	{
		xasprintf( err, "`%s' exists, but it is not a directory.",
				path );
		free( path );
		return 1;
	}

	free( path );
	return 0;
}
