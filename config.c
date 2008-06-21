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



#include <stdlib.h>
#include <string.h>
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
#define SEPARATOR_CHAR '='
#define COMMENT_CHAR '#'



static int read_configfile( char *file, char **contents, char **err );
static int set_key_value( World *wld, char *key, char *value, char **err );
static int set_key_internal( World *wld, char *key, char *value, int src,
		char **err );
static int get_key_internal( World *wld, char *key, char **value, int src );



/* The list of settable/gettable options. The hidden flag indicates if the
 * option is accessible to the user. The setters and getters are functions
 * that actually set or query the option, and are located in accessor.c. */
static const struct
{
	int hidden;
	char *keyname;
	int (*setter)( World *, char *, char *, int, char ** );
	int (*getter)( World *, char *, char **, int );
} key_db[] =
{
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
	{ 0, "autoreconnect",
		&aset_autoreconnect,
		&aget_autoreconnect },

	{ 0, "commandstring",
		&aset_commandstring,
		&aget_commandstring },
	{ 0, "infostring",
		&aset_infostring,
		&aget_infostring },
	{ 0, "newinfostring",
		&aset_newinfostring,
		&aget_newinfostring },

	{ 0, "logging_enabled",
		&aset_logging_enabled,
		&aget_logging_enabled },

	{ 0, "context_on_connect",
		&aset_context_on_connect,
		&aget_context_on_connect },
	{ 0, "max_buffer_size",
		&aset_max_buffer_size,
		&aget_max_buffer_size },
	{ 0, "max_logbuffer_size",
		&aset_max_logbuffer_size,
		&aget_max_logbuffer_size },

	{ 0, "strict_commands",
		&aset_strict_commands,
		&aget_strict_commands },

	{ 0, "timestamped_logs",
		&aset_timestamped_logs,
		&aget_timestamped_logs },

	{ 0, NULL, NULL, NULL }
};

/* Command line options. */
static const char short_opts[] = ":hVLw:md";
static const struct option long_opts[] = {
	{ "help", 0, NULL, 'h' },
	{ "version", 0, NULL, 'V' },
	{ "license", 0, NULL, 'L' },
	{ "world", 1, NULL, 'w' },
	{ "md5crypt", 0, NULL, 'm' },
	{ "no-daemon", 0, NULL, 'd' },
	{ NULL, 0, NULL, 0 }
};



extern void parse_command_line_options( int argc, char **argv, Config *config )
{
	int result;

	config->action = 0;
	config->worldname = NULL;
	config->no_daemon = 0;
	config->error = NULL;

	opterr = 0;

	while( ( result = getopt_long( argc, argv, short_opts, long_opts,
			NULL ) ) != -1 )
	{
		switch( result )
		{
		/* -h, --help */
		case 'h':
		config->action = PARSEOPTS_HELP;
		return;

		/* -L, --license */
		case 'L':
		config->action = PARSEOPTS_LICENSE;
		return;

		/* -V, --version */
		case 'V':
		config->action = PARSEOPTS_VERSION;
		return;

		/* -m, --md5crypt */
		case 'm':
		config->action = PARSEOPTS_MD5CRYPT;
		return;

		/* -w, --world */
		case 'w':
		free( config->worldname );
		config->worldname = xstrdup( optarg );
		break;

		/* -d, --no-daemon */
		case 'd':
		config->no_daemon = 1;
		break;

		/* Unrecognised */
		case '?':
		/* On unrecognised short option, optopt is the unrecognized
		 * char. On unrecognised long option, optopt is 0 and optind
		 * contains the number of the unrecognized argument.
		 * Sigh... */
		if( optopt == 0 )
			xasprintf( &config->error, "Unrecognized option: `%s'"
				". Use --help for help.", argv[optind - 1] );
		else
			xasprintf( &config->error, "Unrecognized option: `-%c'"
				". Use --help for help.", optopt );
		config->action = PARSEOPTS_ERROR;
		return;

		/* Option without required argument. */
		case ':':
		xasprintf( &config->error, "Option `%s' expects an argument.",
				argv[optind - 1] );
		config->action = PARSEOPTS_ERROR;
		return;

		/* We shouldn't get here */
		default:
		break;
		}
	}

	/* Any non-options left? */
	if( optind < argc )
	{
		xasprintf( &config->error, "Unexpected argument: `%s'. "
				"Use --help for help.", argv[optind] );
		config->action = PARSEOPTS_ERROR;
		return;
	}
}



extern int world_load_config( World *wld, char **err )
{
	char *config, *cfg, *line, *sep, *key, *value, *seterr;
	long lineno = 0;

	if( read_configfile( wld->configfile, &config, err ) )
		return 1;

	/* We use and modify cfg. Config is used to free the block later. */
	cfg = config;

	for(;;)
	{
		/* We read a line */
		line = read_one_line( &cfg );
		lineno++;

		/* No more lines? Done */
		if( !line )
			break;

		/* Is the line only whitespace or comment? Skip it */
		line = trim_whitespace( line );
		if( line[0] == '\0' || line[0] == COMMENT_CHAR )
		{
			free( line );
			continue;
		}

		/* Search for the separator character */
		sep = strchr( line, SEPARATOR_CHAR );
		if( sep == NULL )
		{
			/* No separator character? We don't understand */
			xasprintf( err, "Error in %s, line %li,\n   "
				"parse error: `%s'",
				wld->configfile, lineno, line );
			free( line );
			free( config );
			return 1;
		}

		/* Split the line in two strings, and extract key and value */
		*sep = '\0';
		key = xstrdup( line );
		value = xstrdup( sep + 1 );

		/* Clean up key and value */
		key = trim_whitespace( key );
		value = trim_whitespace( value );
		value = remove_enclosing_quotes( value );

		/* We use err to test if an error occured. */
		*err = NULL;

		/* Try and set the key */
		switch( set_key_value( wld, key, value, &seterr ) )
		{
			case SET_KEY_NF:
			xasprintf( err, "Error in %s, line %li,\n    "
					"unknown key `%s'",
					wld->configfile, lineno, key );
			break;

			case SET_KEY_PERM:
			xasprintf( err, "Error in %s, line %li,\n    "
					"setting key `%s' not allowed.",
					wld->configfile, lineno, key );
			break;

			case SET_KEY_BAD:
			xasprintf( err, "Error in %s, line %li,\n    "
					"key `%s': %s",
					wld->configfile, lineno, key, seterr );
			free( seterr );
			break;

			case SET_KEY_OK:
			break;

			default:
			xasprintf( err, "Huh? set_key_value( %s ) returned "
					"weird value...", key );
			break;
		}

		/* Clean up */
		free( key );
		free( value );
		free( line );

		/* If there was an error, abort */
		if( *err )
		{
			free( config );
			return 1;
		}
	}

	/* We're done! */
	free( config );
	return 0;
}



/* Read file (with max length CONFIG_MAXLENGTH KB) in a block of memory,
 * and put the address of this block in contents. The block should be freed.
 * On success, return 0.
 * On error, no block is places in contents, an error is placed in err, and
 * 1 is returned. */
static int read_configfile( char *file, char **contentp, char **err )
{
	int fd, r;
	off_t size;
	char *contents;

	if( file == NULL )
	{
		*err = xstrdup( "No configuration file defined." );
		return 1;
	}

	/* Open the file, and bail out on error */
	fd = open( file, O_RDONLY );
	if( fd == -1 )
	{
		xasprintf( err, "Error opening `%s':\n    %s",
				file, strerror( errno ) );
		return 1;
	}

	/* Check the length of the file */
	size = lseek( fd, 0, SEEK_END );
	if( size > CONFIG_MAXLENGTH * 1024 )
	{
		xasprintf( err, "Error: `%s' is larger than %lu kilobytes.",
				file, CONFIG_MAXLENGTH );
		close( fd );
		return 1;
	}

	/* Reset the offset to the start of file */
	lseek( fd, 0, SEEK_SET );

	/* Allocate memory, and read file contents */
	contents = xmalloc( size + 1 );
	r = read( fd, contents, size );
	if( r == -1 )
	{
		xasprintf( err, "Error reading from `%s':\n    %s",
				file, strerror( errno ) );
		free( contents );
		close( fd );
		return 1;
	}

	/* Nul-terminate the string */
	contents[size] = '\0';

	/* Clean up, and done */
	close( fd );
	*contentp = contents;
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
	char *path, *errstr = NULL;
	*err = NULL;

	xasprintf( &path, "%s/%s", get_homedir(), CONFIGDIR );
	if( attempt_createdir( path, &errstr ) )
		goto create_failed;
	free( path );

	xasprintf( &path, "%s/%s/%s", get_homedir(), CONFIGDIR, WORLDSDIR );
	if( attempt_createdir( path, &errstr ) )
		goto create_failed;
	free( path );

	xasprintf( &path, "%s/%s/%s", get_homedir(), CONFIGDIR, LOGSDIR );
	if( attempt_createdir( path, &errstr ) )
		goto create_failed;
	free( path );

	xasprintf( &path, "%s/%s/%s", get_homedir(), CONFIGDIR, LOCKSDIR );
	if( attempt_createdir( path, &errstr ) )
		goto create_failed;
	free( path );

	return 0;

create_failed:
	xasprintf( err, "Error creating `%s': %s", path, errstr );
	free( path );
	free( errstr );
	return 1;
}



extern int check_configdir_perms( char **warn, char **err )
{
	char *path;
	struct stat fileinfo;

	*warn = NULL;

	/* Construct the path */
	xasprintf( &path, "%s/%s", get_homedir(), CONFIGDIR );

	/* Get the information. */
	if( stat( path, &fileinfo ) == -1 )
	{
		xasprintf( err, "Could not stat `%s': %s",
				path, strerror( errno ) );
		free( path );
		return 1;
	}
	
	/* Are the permissions ok? */
	if( ( fileinfo.st_mode & ( S_IRWXG | S_IRWXO ) ) == 0 )
	{
		free( path );
		return 0;
	}

	/* The permissions are not ok, construct a message.
	 * OMG! It's the xasprintf from hell! */
	xasprintf( warn, "\n"
		"----------------------------------------------------------"
		"--------------------\nWARNING! The mooproxy configuration "
		"directory has weak permissions:\n"
		"\n    %s%s%s%s%s%s%s%s%s %s\n\n"
		"This means other users on your system may be able to read "
		"your mooproxy files.\nIf this is intentional, make sure the "
		"permissions on the files and directories\nit contains are "
		"sufficiently strict.\n-------------------------------------"
		"-----------------------------------------\n\n",
		(fileinfo.st_mode & S_IRUSR ) ? "r" : "-",
		(fileinfo.st_mode & S_IWUSR ) ? "w" : "-",
		(fileinfo.st_mode & S_IXUSR ) ? "x" : "-",
		(fileinfo.st_mode & S_IRGRP ) ? "R" : "-",
		(fileinfo.st_mode & S_IWGRP ) ? "W" : "-",
		(fileinfo.st_mode & S_IXGRP ) ? "X" : "-",
		(fileinfo.st_mode & S_IROTH ) ? "R" : "-",
		(fileinfo.st_mode & S_IWOTH ) ? "W" : "-",
		(fileinfo.st_mode & S_IXOTH ) ? "X" : "-",
		path );

	free( path );
	return 0;
}
