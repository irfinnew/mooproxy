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



#ifndef MOOPROXY__HEADER__ACCESSOR
#define MOOPROXY__HEADER__ACCESSOR



#include "config.h"
#include "world.h"



#define ASRC_FILE 0
#define ASRC_USER 1



/* Arguments: world, keyname, value, source, errorptr.
 * Source indicates if the request comes from the user or the system.
 * These functions return SET_KEY_???.
 * When returning SET_KEY_BAD, errorptr contains a message. */

extern int aset_listenport( World *, char *, char *, int, char ** );
extern int aset_authstring( World *, char *, char *, int, char ** );
extern int aset_dest_host( World *, char *, char *, int, char ** );
extern int aset_dest_port( World *, char *, char *, int, char ** );
extern int aset_commandstring( World *, char *, char *, int, char ** );
extern int aset_infostring( World *, char *, char *, int, char ** );
extern int aset_logging_enabled( World *, char *, char *, int, char ** );
extern int aset_context_on_connect( World *, char *, char *, int, char ** );
extern int aset_max_buffered_size( World *, char *, char *, int, char ** );
extern int aset_max_history_size( World *, char *, char *, int, char ** );
extern int aset_strict_commands( World *, char *, char *, int, char ** );



/* Arguments: world, keyname, valueptr, source.
 * These functions return GET_KEY_???. */

extern int aget_listenport( World *, char *, char **, int );
extern int aget_authstring( World *, char *, char **, int );
extern int aget_dest_host( World *, char *, char **, int );
extern int aget_dest_port( World *, char *, char **, int );
extern int aget_commandstring( World *, char *, char **, int );
extern int aget_infostring( World *, char *, char **, int );
extern int aget_logging_enabled( World *, char *, char **, int );
extern int aget_context_on_connect( World *, char *, char **, int );
extern int aget_max_buffered_size( World *, char *, char **, int );
extern int aget_max_history_size( World *, char *, char **, int );
extern int aget_strict_commands( World *, char *, char **, int );



#endif  /* ifndef MOOPROXY__HEADER__ACCESSOR */
