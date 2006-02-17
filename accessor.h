/*
 *
 *  mooproxy - a buffering proxy for moo-connections
 *  Copyright (C) 2001-2006 Marcel L. Moreaux <marcelm@luon.net>
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



#ifndef MOOPROXY__HEADER__ACCESSOR
#define MOOPROXY__HEADER__ACCESSOR



#include "config.h"
#include "world.h"



#define ASRC_FILE 0  /* Originator: configuration file */
#define ASRC_USER 1  /* Originator: user (/get and /set commands) */



/* The setter functions. These functions attempt to set a single key
 * to the given value, and report back errors if this fails.
 *
 * Arguments: World *wld, char *key, char *value, int src, char **err
 *   wld:    the world to change this key in
 *   key:    the name of the key to set
 *   value:  string-representation of the new value
 *   src:    indicate the originator of the request. One of ASRC_FILE, ASRC_USER
 *   err:    points to a char *, which will be set to an error string on
 *           SET_KEY_BAD. If err is set, it should be free()d.
 *
 * Return values:
 *   SET_KEY_OK:    Everything OK, new value stored
 *   SET_KEY_NF:    The key was not found
 *   SET_KEY_PERM:  This key may not be written (by the given originator)
 *   SET_KEY_BAD:   The given value was not acceptable. err has been set.
 */

extern int aset_listenport( World *, char *, char *, int, char ** );
extern int aset_auth_md5hash( World *, char *, char *, int, char ** );
extern int aset_dest_host( World *, char *, char *, int, char ** );
extern int aset_dest_port( World *, char *, char *, int, char ** );
extern int aset_autologin( World *, char *, char *, int, char ** );
extern int aset_commandstring( World *, char *, char *, int, char ** );
extern int aset_infostring( World *, char *, char *, int, char ** );
extern int aset_logging_enabled( World *, char *, char *, int, char ** );
extern int aset_context_on_connect( World *, char *, char *, int, char ** );
extern int aset_max_buffered_size( World *, char *, char *, int, char ** );
extern int aset_max_history_size( World *, char *, char *, int, char ** );
extern int aset_strict_commands( World *, char *, char *, int, char ** );



/* The getter functions. These functions get the value of a single given key,
 * and report back errors if this fails.
 *
 * Arguments: World *wld, char *key, char **value, int src
 *   wld:    the world to get this key from
 *   key:    the name of the key to get
 *   value:  pointer to the location to store the string representation of the
 *           requested value. This will only be set on GET_KEY_OK.
 *           The string should be free()d.
 *   src:    indicate the originator of the request. One of ASRC_FILE, ASRC_USER
 *
 * Return values:
 *   GET_KEY_OK:    Everything OK, 
 *   GET_KEY_NF:    The key was not found
 *   GET_KEY_PERM:  This key may not be read (by the given originator)
 */

extern int aget_listenport( World *, char *, char **, int );
extern int aget_auth_md5hash( World *, char *, char **, int );
extern int aget_dest_host( World *, char *, char **, int );
extern int aget_dest_port( World *, char *, char **, int );
extern int aget_autologin( World *, char *, char **, int );
extern int aget_commandstring( World *, char *, char **, int );
extern int aget_infostring( World *, char *, char **, int );
extern int aget_logging_enabled( World *, char *, char **, int );
extern int aget_context_on_connect( World *, char *, char **, int );
extern int aget_max_buffered_size( World *, char *, char **, int );
extern int aget_max_history_size( World *, char *, char **, int );
extern int aget_strict_commands( World *, char *, char **, int );



#endif  /* ifndef MOOPROXY__HEADER__ACCESSOR */
