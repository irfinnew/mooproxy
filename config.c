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

#include "config.h"
#include "global.h"
#include "misc.h"
#include "world.h"



#define SEPARATOR '='

#define K_LONG 1
#define K_BOOL 2
#define K_STR 3

#define K_HIDE 1
#define K_NONE 2
#define K_R 3
#define K_W 4
#define K_RW 5



static World defwld;
static const struct
{
	char *keyname;
	char type;
	char access;
	void *var;
} key_db[] = {
	{ "listenport",		K_LONG,	K_RW,	&defwld.listenport },
	{ "authstring",		K_STR,	K_W,	&defwld.authstring },

	{ "host",		K_STR,	K_RW,	&defwld.host },
	{ "port",		K_LONG,	K_RW,	&defwld.port },

	{ "commandstring",	K_STR,	K_RW,	&defwld.commandstring },
	{ "infostring",		K_STR,	K_RW,	&defwld.infostring },

	{ NULL, 0, 0, NULL }
};
static const char short_opts[] = ":hVLw:";
static const struct option long_opts[] = {
	{ "help", 0, NULL, 'h' },
	{ "version", 0, NULL, 'V' },
	{ "license", 0, NULL, 'L' },
	{ "world", 1, NULL, 'w' },
	{ NULL, 0, NULL, 0 }
};



static char *print_help_text( void );
static char *print_version_text( void );
static char *print_license_text( void );
static int add_key_value( World *, char *, char *, int );
static int get_key_value( World *, char *, char **, int );
static void store_value_long( World *, void *, char * );
static void store_value_bool( World *, void *, char * );
static void store_value_string( World *, void *, char * );
static void get_value_long( World *, void *, char ** );
static void get_value_bool( World *, void *, char ** );
static void get_value_string( World *, void *, char ** );
static int attempt_createdir( char *, char ** );
	


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
	char *line, *key, *value, *s, *contents, *cnt;
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

	contents = cnt = malloc( 102400 );
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

			/* Strip off a single leading and trailing " */
			value = remove_enclosing_quotes( value );

			if( add_key_value( wld, key, value, 0 ) )
			{
				asprintf( err, "%s: line %li: unknown key `%s'"
					, wld->configfile, wline, key );
				free( contents );
				free( line );
				free( key );
				free( value );
				return EXIT_CONFIGERR;
			};

			free( key );
			free( value );
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
		if( key_db[i].access == K_HIDE )
			continue;

		*list = realloc( *list, ++num * sizeof( char * ) );
		( *list )[num - 1] = strdup( key_db[i].keyname );
	}

	return num;
}



/* This function searches for the key name in the database. If it matches,
 * it places the value in the appropriate variable of the World.
 * On success, it returns 0.
 * If the key isn't found, it returns 1.
 * If the key cannot be written, it returns 2. */
extern int world_set_key( World *wld, char *key, char *value )
{
	return add_key_value( wld, key, value, 1 );
}



/* This function searches for the key name in the database. If it matches,
 * it places the corresponding value in the string pointer.
 * On success, it returns 0.
 * If the key isn't found, it returns 1.
 * If the key cannot be read, it returns 2. */
extern int world_get_key( World *wld, char *key, char **value )
{
	return get_key_value( wld, key, value, 1 );
}



static int add_key_value( World *wld, char *key, char *value, int ac )
{
	int i;

	for( i = 0; key_db[i].keyname; i++ )
	{
		if( !strcmp( key, key_db[i].keyname ) )
		{
			if( ac && key_db[i].access == K_HIDE )
				return 1;

			if( ac && key_db[i].access != K_W &&
					key_db[i].access != K_RW )
				return 2;

			switch( key_db[i].type )
			{
			case K_LONG:
				store_value_long( wld, key_db[i].var, value );
				break;

			case K_BOOL:
				store_value_bool( wld, key_db[i].var, value );
				break;

			case K_STR:
				store_value_string( wld, key_db[i].var,
						value );
				break;
			}

			return 0;
		}
	};

	return 1;
}



static int get_key_value( World *wld, char *key, char **value, int ac )
{
	int i;

	for( i = 0; key_db[i].keyname; i++ )
	{
		if( !strcmp( key, key_db[i].keyname ) )
		{
			if( ac && key_db[i].access == K_HIDE )
				return 1;

			if( ac && key_db[i].access != K_R &&
					key_db[i].access != K_RW )
				return 2;

			switch( key_db[i].type )
			{
			case K_LONG:
				get_value_long( wld, key_db[i].var, value );
				break;

			case K_BOOL:
				get_value_bool( wld, key_db[i].var, value );
				break;

			case K_STR:
				get_value_string( wld, key_db[i].var,	value );
				break;
			}

			return 0;
		}
	};

	return 1;
}



void store_value_long( World *world, void *ptr, char *value )
{
	ptr = ( ptr - (void *) &defwld ) + (void *) world;
	*(long *) ptr = strtol( value, NULL, 0 );
}



void store_value_bool( World *world, void *ptr, char *value )
{
	ptr = ( ptr - (void *) &defwld ) + (void *) world;
	*(char *) ptr = true_or_false( value ) == 1 ? 1 : 0;
}



void store_value_string( World *world, void *ptr, char *value )
{
	ptr = ( ptr - (void *) &defwld ) + (void *) world;
	free( *(char **) ptr );
	*(char **) ptr = strdup( value );
}



void get_value_long( World *world, void *ptr, char **value )
{
	ptr = ( ptr - (void *) &defwld ) + (void *) world;
	asprintf( value, "%li", *(long *) ptr );
}



void get_value_bool( World *world, void *ptr, char **value )
{
	ptr = ( ptr - (void *) &defwld ) + (void *) world;
	if( *(char *) ptr == 0 )
		*value = strdup( "true" );
	else
		*value = strdup( "false" );
}



void get_value_string( World *world, void *ptr, char **value )
{
	ptr = ( ptr - (void *) &defwld ) + (void *) world;
	*value = NULL;
	if( *(char **) ptr )
		*value = strdup( *(char **) ptr );
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
	if( ( i = attempt_createdir( LOCKSDIR, err ) ) )
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
	
	if( !mkdir( path, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP ) )
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
