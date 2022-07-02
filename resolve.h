/*
 *
 *  mooproxy - a smart proxy for MUD/MOO connections
 *  Copyright 2001-2011 Marcel Moreaux
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



#ifndef MOOPROXY__HEADER__RESOLVE
#define MOOPROXY__HEADER__RESOLVE



#include "world.h"



/* Start the process of resolving wld->server_host. Create a comminucation pipe,
 * fork the resolver slave, and set wld->server_status to ST_RESOLVING. */
extern void world_start_server_resolve( World *wld );

/* Abort the current resolving attempt. The resolver slave is killed. */
extern void world_cancel_server_resolve( World *wld );

/* When the resolver slave is done, it'll write its data to
 * wld->server_resolver_fd. Activity on this FD should be handled by this
 * function. It will read data from the FD, construct the list of addresses
 * or report an error, and terminate the resolver slave. */
extern void world_handle_resolver_fd( World *wld );



#endif  /* ifndef MOOPROXY__HEADER__RESOLVE */
