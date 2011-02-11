/*
 *
 *  mooproxy - a buffering proxy for MOO connections
 *  Copyright (C) 2001-2011 Marcel L. Moreaux <marcelm@luon.net>
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



#ifndef MOOPROXY__HEADER__CRYPT
#define MOOPROXY__HEADER__CRYPT



#include "world.h"



/* Prompt the user for an authentication string, twice. Compare the strings,
 * and if they're not equal, complain. Otherwise, create an MD5 hash (with
 * random salt) from the string, and print that. */
extern void prompt_to_md5hash( void );

/* Check if str looks like an MD5 hash. This is not foolproof; it is intended
 * to protect from simple mistakes.
 * Returns 1 if str looks like a MD5 hash, and 0 if not. */
extern int looks_like_md5hash( char *str );

/* Checks if str is the correct authentication string for wld.
 * If wld->auth_literal exists, str is checked against this.
 * Otherwise, it's hashed and checked against wld->auth_hash.
 * If str matches, and wld->auth_literal does not exist, it is created. 
 * Returns 1 if str matches, 0 if it doesn't. */
extern int world_match_authentication( World *wld, const char *str );

/* Matches str against md5hash.
 * Returns 1 if str matches, 0 if it doesn't. */
extern int match_string_md5hash( const char *str, const char *md5hash );



#endif  /* ifndef MOOPROXY__HEADER__CRYPT */
