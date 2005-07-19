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



#define SEPARATOR '='



static char *print_help_text( void );
static char *print_version_text( void );
static char *print_license_text( void );
static int set_key_value( World *, char *, char *, char ** );
/* static int get_key_value( World *, char *, char ** ); */
static int set_key_internal( World *, char *, char *, int, char ** );
static int get_key_internal( World *, char *, char **, int );
static int attempt_createdir( char *, char ** );



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
	{ 0, "authstring",
		&aset_authstring,
		&aget_authstring },

	{ 0, "host",
		&aset_dest_host,
		&aget_dest_host },
	{ 0, "port",
		&aset_dest_port,
		&aget_dest_port },

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

static const char short_opts[] = ":hVLw:";
static const struct option long_opts[] = {
	{ "help", 0, NULL, 'h' },
	{ "version", 0, NULL, 'V' },
	{ "license", 0, NULL, 'L' },
	{ "world", 1, NULL, 'w' },
	{ NULL, 0, NULL, 0 }
};
	


/* This function parses the commandline options (argc and argv), and
 * acts accordingly. It places the name of the world in the third arg.
 * On failure, it returns non-zero and places the error in the last arg. */
int parse_command_line_options( int argc, char **argv,
		char **worldname, char **err )
{
	int result, i;

	opterr = 0;

	while( ( result = getopt_long( argc, argv, short_opts, long_opts,
			NULL ) ) != -1 )
	{
		switch( result )
		{
		case 'h':
			*err = print_help_text();
			return EXIT_HELP;
		case 'L':
			*err = print_license_text();
			return EXIT_HELP;
		case 'V':
			*err = print_version_text();
			return EXIT_HELP;
		case 'w':
			if( *worldname != NULL )
				free( *worldname );
			*worldname = strdup( optarg );
			break;
		case '?':
			asprintf( err, "Unknown option: `%s'.",
					argv[optind - 1] );
			return EXIT_UNKNOWNOPT;
		case ':':
			asprintf( err, "Option `%s' expects an argument.",
					argv[optind - 1] );
			return EXIT_UNKNOWNOPT;
		default:
			break;
		}
	}

	if( optind < argc )
	{
		for( i = optind; i < argc; i++ )
			asprintf( err, "Unexpected argument: `%s'.",
					argv[i] );
		return EXIT_UNKNOWNOPT;
	}

	return EXIT_OK;
}



char *print_help_text( void )
{
	return strdup(
	"mooproxy - a proxy for moo/mud connections (C) 2002 by "
			"Marcel L. Moreaux\n\n"
	"usage: mooproxy [options]\n\n"
	"-h, --help\t\tshows this help screen and exits\n"
	"-V, --version\t\tshows version information and exits\n"
	"-L, --license\t\tshows licensing information and exits\n"
	"-w, --world\t\tworld to load\n"
	"\nreleased under the GPL 2.0, report bugs to <marcelm@luon.net>\n"
	"mooproxy comes with ABSOLUTELY NO WARRANTY; for details run "
			"mooproxy --license" );
}



static char *print_version_text( void )
{
	return strdup( "mooproxy version " VERSIONSTR " Copyright (C) 2004 "
			"Marcel L Moreaux" );
}



static char *print_license_text( void )
{
	return strdup(
	"mooproxy - a proxy for moo/mud connections\n"
	"Copyright (C) 2002 Marcel L Moreaux\n"
	"This program is free software; you can redistribute it and/or "
			"modify\n"
	"it under the terms of the GNU General Public License as "
			"published by\n"
	"the Free Software Foundation; either version 2 of the License, or\n"
	"(at your option) any later version.\n\n"
	"This program is distributed in the hope that it will be useful,\n"
	"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
	"GNU General Public License for more details." );
}



/* This function attempts to load the configuration file of the World.
 * On failure, it returns non-zero and places the error in the last arg. */
int world_load_config( World *wld, char **err )
{
	int fd, i;
	char *line, *key, *value, *s, *contents, *cnt, *tmp;
	long wline;
	size_t l;
	
	if( wld->configfile == NULL )
	{
		asprintf( err, "Could not open (null).\nNo such world `%s'",
				wld->name );
		return EXIT_NOSUCHWORLD;
	}

	fd = open( wld->configfile, O_RDONLY );
	if( fd == -1 )
	{
		asprintf( err, "Error opening `%s': %s\nNo such world `%s'",
			wld->configfile, strerror_n( errno ), wld->name );
		return EXIT_NOSUCHWORLD;
	}

	contents = cnt = malloc( 102401 );
	i = read( fd, contents, 102400 );
	if( i == - 1 )
	{
		asprintf( err, "Error reading from `%s': %s\nNo such world `%s'"
			, wld->configfile, strerror_n( errno ), wld->name );
		free( contents );
		return EXIT_NOSUCHWORLD;
	}

	contents[i] = '\0';
	close( fd );
	
	for( wline = 1; ( line = read_one_line( &cnt ) ); wline++ )
	{
		line = trim_whitespace( line );

		if( line[0] == '\0' || line[0] == '#' )
			;
		else if( ( s = strchr( line, SEPARATOR ) ) )
		{
			key = malloc( ( l = s - line ) + 1 );
			strncpy( key, line, l );
			key[l] = '\0';
			value = malloc( ( l = strlen( line ) - l - 1 ) + 1 );
			strncpy( value, ++s, l );
			value[l] = '\0';

			key = trim_whitespace( key );
			value = trim_whitespace( value );

			/* Strip off a single leading and trailing quote */
			value = remove_enclosing_quotes( value );

			switch( set_key_value( wld, key, value, err ) )
			{
				case SET_KEY_NF:
				asprintf( err, "%s: line %li: unknown key `%s'"
					, wld->configfile, wline, key );
				break;

				case SET_KEY_PERM:
				asprintf( err, "%s: line %li: setting key `%s'"
					" not allowed.", wld->configfile,
					wline, key );
				break;

				case SET_KEY_BAD:
				tmp = *err;
				asprintf( err, "%s: line %li: setting key `%s'"
					": %s", wld->configfile, wline,
					key, tmp );
				free( tmp );
				break;
			}

			free( key );
			free( value );

			if( *err )
			{
				free( contents );
				free( line );
				return EXIT_CONFIGERR;
			}
		}
		else
		{
			asprintf( err, "%s: line %li, parse error: `%s'", 
				wld->configfile, wline, line );
			free( contents );
			free( line );
			return EXIT_CONFIGERR;
		}
		
		free( line );
	}

	free( contents );
	return EXIT_OK;
}



/* Place a list of key names (except the hidden ones) in the string pointer
 * and return the number of keys. The list must be freed. */
extern int world_get_key_list( World *wld, char ***list )
{
	int i, num = 0;
	*list = NULL;

	for( i = 0; key_db[i].keyname; i++ )
	{
		if( key_db[i].hidden )
			continue;

		*list = realloc( *list, ++num * sizeof( char * ) );
		( *list )[num - 1] = strdup( key_db[i].keyname );
	}

	return num;
}



/* This function searches for the key name in the database. If it matches,
 * it places the value in the appropriate variable of the World.
 * Returns SET_KEY_???
 * When returning SET_KEY_BAD, the string pointer contains a message. */
extern int world_set_key( World *wld, char *key, char *value, char **err )
{
	return set_key_internal( wld, key, value, ASRC_USER, err );
}



/* This function searches for the key name in the database. If it matches,
 * it places the corresponding value in the string pointer.
 * Returns GET_KEY_??? */
extern int world_get_key( World *wld, char *key, char **value )
{
	return get_key_internal( wld, key, value, ASRC_USER );
}



extern int set_key_value( World *wld, char *key, char *value, char **err )
{
	return set_key_internal( wld, key, value, ASRC_FILE, err );
}



/* extern int get_key_value( World *wld, char *key, char **value )
{
	return get_key_internal( wld, key, value, ASRC_FILE );
} */



static int set_key_internal( World *wld, char *key, char *value, int src,
		char **err )
{
	int i;

	*err = NULL;

	for( i = 0; key_db[i].keyname; i++ )
		if( !strcmp( key, key_db[i].keyname ) )
		{
			if( key_db[i].hidden )
				return SET_KEY_NF;

			return (*key_db[i].setter)( wld, key, value, src, err );
		}

	return SET_KEY_NF;
}



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



int create_configdirs( char **err )
{
	int i;

	if( ( i = attempt_createdir( CONFIGDIR, err ) ) )
		return i;
	if( ( i = attempt_createdir( WORLDSDIR, err ) ) )
		return i;
	if( ( i = attempt_createdir( LOGSDIR, err ) ) )
		return i;

	return EXIT_OK;
}



int attempt_createdir( char *dirname, char **err )
{
	char *path, *homedir = get_homedir();
	struct stat fileinfo;

	path = malloc( strlen( homedir ) + strlen( dirname ) + 1 );
	strcpy( path, homedir );
	strcat( path, dirname );
	free( homedir );
	
	if( !mkdir( path, S_IRUSR | S_IWUSR | S_IXUSR ) )
	{
		free( path );
		return EXIT_OK;
	}

	if( errno != EEXIST )
	{
		asprintf( err, "Could not create directory `%s': %s",
				path, strerror_n( errno ) );
		free( path );
		return EXIT_CONFIGDIRS;
	}

	if( stat( path, &fileinfo ) == -1 )
	{
		asprintf( err, "Could not stat `%s': %s",
				path, strerror_n( errno ) );
		free( path );
		return EXIT_CONFIGDIRS;
	}
	
	if( !S_ISDIR( fileinfo.st_mode ) )
	{
		asprintf( err, "`%s' exists, but it is not a directory.",
				path );
		free( path );
		return EXIT_CONFIGDIRS;
	}

	free( path );
	return EXIT_OK;
}
