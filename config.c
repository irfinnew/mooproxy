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
	char *shortdesc;
	char *longdesc;
}
key_db[] =
{
	{ 0, "listenport", aset_listenport, aget_listenport,
	"The network port mooproxy listens on.",
	"The network port mooproxy listens on for client connections." },

	{ 0, "auth_hash", aset_auth_hash, aget_auth_hash,
	"The \"password\" a client needs to connect.",
	"Clients connecting to mooproxy have to provide a string\n"
	"(such as \"connect <player> <password>\") to authenticate\n"
	"themselves. This setting contains a hash of this string.\n"
	"\n"
	"To change auth_hash, you need to specify the new hash as\n"
	"well as the old literal authentication string, like so:\n"
	"\n"
	"/auth_hash <new hash> \"<old literal string>\"\n"
	"\n"
	"You can generate a new hash by running mooproxy --md5crypt.\n"
	"See the README for more details on authentication." },

	{ 0, "host", aset_dest_host, aget_dest_host,
	"The hostname of the server to connect to.",
	NULL },

	{ 0, "port", aset_dest_port, aget_dest_port,
	"The port to connect to on the server.",
	NULL },

	{ 0, "autologin", aset_autologin, aget_autologin,
	"Log in automatically after connecting.",
	"If true, mooproxy will try to log you in after connecting to\n"
	"the server. For this, it will use the client authentication\n"
	"string (see auth_hash)." },

	{ 0, "autoreconnect", aset_autoreconnect, aget_autoreconnect,
	"Reconnect when the server disconnects us.",
	"If true, mooproxy will attempt to reconnect to the server\n"
	"whenever the connection to the server is lost or fails to be\n"
	"established. It uses exponential back-off.\n"
	"\n"
	"Mooproxy will not reconnect if establishing a connection\n"
	"fails directly after the /connect command has been issued.\n"
	"\n"
	"Autoreconnect usually only makes sense if autologin is on." },

	{ 0, "commandstring", aset_commandstring, aget_commandstring,
	"How mooproxy recognizes commands.",
	"Lines from the client starting with this string are\n"
	"interpreted as commands (but see also: strict_commands).\n"
	"This string does not need to be one character, it can be any\n"
	"length (even empty)." },

	{ 0, "strict_commands", aset_strict_commands, aget_strict_commands,
	"Ignore invalid commands.",
	"If mooproxy receives a string from the client that starts\n"
	"with the commandstring, but is not a valid command, this\n"
	"setting determines what mooproxy will do.\n"
	"\n"
	"If true, mooproxy will complain that the command is invalid.\n"
	"If false, mooproxy will pass the line through to the server\n"
	"as if it was a regular line." },

	{ 0, "infostring", aset_infostring, aget_infostring,
	"Prefix for all messages from mooproxy.",
	"Informational messages from mooproxy to the user are\n"
	"prefixed by this string.\n"
	"\n"
	"Use the following sequences to get colours:\n"
	"  %b -> blue   %g -> green  %m -> magenta  %w -> white\n"
	"  %c -> cyan   %k -> black  %r -> red      %y -> yellow\n"
	"Use uppercase to get the bold/bright variant of that colour.\n"
	"Use %% to get a literal %." },

	{ 0, "newinfostring", aset_newinfostring, aget_newinfostring,
	"Prefix for history/new lines messages.",
	"Mooproxy prefixes this string to the \"context history\",\n"
	"\"possibly new lines\", \"certainly new lines\" and \"end of new\n"
	"lines\" messages. Setting this to something colourful might\n"
	"make it easier to spot these messages.\n"
	"\n"
	"This string accepts the same colour sequences as infostring.\n"
	"If this string is not set, or it is set to \"\", mooproxy will\n"
	"use the regular infostring for these messages." },
	
	{ 0, "context_lines", aset_context_lines, aget_context_lines,
	"Context lines to provide when you connect.",
	"When a client connects, mooproxy can reproduce lines from\n"
	"history to the client, in order to provide the user with\n"
	"context. This setting sets the number of reproduced lines." },

	{ 0, "buffer_size", aset_buffer_size, aget_buffer_size,
	"Max memory to spend on new/history lines.",
	"The maximum amount of memory in KB used to hold history\n"
	"lines (lines you have already read) and new lines (lines you\n"
	"have not yet read).\n"
	"\n"
	"If the amount of lines mooproxy receives from the server\n"
	"while the client is not connected exceeds this amount of\n"
	"memory, mooproxy will have to drop unread lines (but it will\n"
	"still attempt to log those lines, see logbuffer_size)." },

	{ 0, "logbuffer_size", aset_logbuffer_size, aget_logbuffer_size,
	"Max memory to spend on unlogged lines.",
	"The maximum amount of memory in KB used to hold loggable\n"
	"lines that have not yet been written to disk.\n"
	"\n"
	"If your disk space runs out, and the amount of unlogged\n"
	"lines exceeds this amount of memory, new lines will NOT be\n"
	"logged." },

	{ 0, "logging", aset_logging, aget_logging,
	"Log everything from the server.",
	"If true, mooproxy will log all lines from the server (and a\n"
	"select few messages from mooproxy) to file." },

	{ 0, "log_timestamps", aset_log_timestamps, aget_log_timestamps,
	"Prefix every logged line by a timestamp.",
	"If true, all logged lines are prefixed with a [HH:MM:SS]\n"
	"timestamp." },

	{ 0, NULL, NULL, NULL, NULL, NULL }
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
		( *list )[num - 1] = key_db[i].keyname;
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



extern int world_desc_key( World *wld, char *key,
		char **shortdesc, char **longdesc )
{
	int i;

	for( i = 0; key_db[i].keyname; i++ )
		if( !strcmp_under( key, key_db[i].keyname ) )
		{
			if( key_db[i].hidden )
				return GET_KEY_NF;

			*shortdesc = key_db[i].shortdesc;
			*longdesc = key_db[i].longdesc;
			return GET_KEY_OK;
		}

	return GET_KEY_NF;
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
		if( !strcmp_under( key, key_db[i].keyname ) )
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
		if( !strcmp_under( key, key_db[i].keyname ) )
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
