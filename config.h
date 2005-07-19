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



#ifndef MOOPROXY__HEADER__CONFIG
#define MOOPROXY__HEADER__CONFIG



#include "world.h"



#define SET_KEY_OK 0
#define SET_KEY_NF 1
#define SET_KEY_PERM 2
#define SET_KEY_BAD 3

#define GET_KEY_OK 0
#define GET_KEY_NF 1
#define GET_KEY_PERM 2



/* This function parses the commandline options (argc and argv), and
 * acts accordingly. It places the name of the world in the third arg.
 * On failure, it returns non-zero and places the error in the last arg. */
extern int parse_command_line_options( int, char **, char **, char ** );

/* This function attempts to load the configuration file of the World.
 * On failure, it returns non-zero and places the error in the last arg. */
extern int world_load_config( World *, char ** );

/* Place a list of key names (except the hidden ones) in the string array 
 * pointer and return the number of keys. The list must be freed. */
extern int world_get_key_list( World *, char *** );

/* This function searches for the key name in the database. If it matches,
 * it places the corresponding value in the string pointer.
 * Returns GET_KEY_??? */
extern int world_get_key( World *, char *, char ** );

/* This function searches for the key name in the database. If it matches,
 * it places the value in the appropriate variable of the World.
 * Returns SET_KEY_???
 * When returning SET_KEY_BAD, the string pointer contains a message. */
extern int world_set_key( World *, char *, char *, char ** );

/* This function attempts to create the configuration dirs for mooproxy.
 * On failure, it returns non-zero and places the error in the last arg. */
extern int create_configdirs( char ** );



#endif  /* ifndef MoOPROXY__HEADER__CONFIG */
