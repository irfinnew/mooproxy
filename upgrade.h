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



#ifndef MOOPROXY__HEADER__UPGRADE
#define MOOPROXY__HEADER__UPGRADE



#include "world.h"



/* FIXME */
extern int upgrade_do_server( World *wld, char **err );

/* FIXME */
extern void upgrade_do_client_init( World *wld );

/* FIXME */
extern void upgrade_do_client_transfer( World *wld );



#endif  /* ifndef MOOPROXY__HEADER__UPGRADE */
