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



#ifndef MOOPROXY__HEADER__CONFIG
#define MOOPROXY__HEADER__CONFIG



#include "world.h"



/* Collection of settings from the commandline. */
typedef struct _Config Config;
struct _Config
{
	int action;
	char *worldname;
	int no_daemon;

	char *error;
};

/* Return values for parse_command_line_options.
 * Indicates what action to take. */
#define PARSEOPTS_START 0
#define PARSEOPTS_HELP 1
#define PARSEOPTS_VERSION 2
#define PARSEOPTS_LICENSE 3
#define PARSEOPTS_MD5CRYPT 4
#define PARSEOPTS_ERROR 5

/* Return values for attempting to set a key. */
#define SET_KEY_OK 1
#define SET_KEY_OKSILENT 2
#define SET_KEY_NF 3
#define SET_KEY_PERM 4
#define SET_KEY_BAD 5

/* Return values for attempting to query a key. */
#define GET_KEY_OK 1
#define GET_KEY_NF 2
#define GET_KEY_PERM 3



/* Parse the commandline arguments, extracting options and actions.
 * argc, argv:  Arguments from main().
 * config:      Config struct. All members of this struct will be set.
 *              If config->action is PARSEOPTS_ERROR, config->error
 *              will contain an error. config and its members should be
 *              freed.
 */
extern void parse_command_line_options( int argc, char **argv, Config *config );

/* Attempts to load the configuration file for this world.
 * On succes, returns 0.
 * On failure, returns non-zero and sets err to point to an error string.
 * The error string should be free()d. */
extern int world_load_config( World *wld, char **err );

/* Place a list of key names (except the hidden ones) in list (a pointer to
 * an array of strings). All strings, and the array, should be free()d.
 * Returns the number of keys in the list. */
extern int world_get_key_list( World *wld, char ***list );

/* Searches the option database for the key name, and attempts to set it
 * to the new value.
 * Arguments:
 *   key:    The name of the key to set. (string not consumed)
 *   value:  String representation of the new value. (string not consumed)
 *   err:    On SET_KEY_BAD, contains error msg. The string should be free()d.
 * Return values:
 *   SET_KEY_OK:    Everything ok, the key was set.
 *   SET_KEY_NF:    Key doesn't exist (or is hidden).
 *   SET_KEY_PERM:  Key may not be written.
 *   SET_KEY_BAD:   New value was bad. See err. */
extern int world_set_key( World *wld, char *key, char *value, char **err );

/* Searches the option database for the key name, and attempts to query
 * its value.
 * Arguments:
 *   key:    The name of the key to query. (string not consumed)
 *   value:  On GET_KEY_OK, is set to the string representation of the
 *           value. The string should be free()d.
 * Return values:
 *   GET_KEY_OK:    Everything ok, the value is read.
 *   GET_KEY_NF:    Key doesn't exist (or is hidden).
 *   GET_KEY_PERM:  Key may not be read. */
extern int world_get_key( World *wld, char *key, char **value );

/* This function attempts to create the configuration dirs for mooproxy.
 * On failure, it returns non-zero and places the error in err (the string
 * should be free()d). */
extern int create_configdirs( char **err );

/* Check the permissions on the configuration directory. On error, returns
 * non-zero and places the error in err (the string should be free()d).
 * If the permissions are not satisfactory, a warning string is placed in
 * warn (which should be free()d). Otherwise, warn is set to NULL. */
extern int check_configdir_perms( char **warn, char **err );



#endif  /* ifndef MOOPROXY__HEADER__CONFIG */
