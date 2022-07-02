/*
 *
 *  mooproxy - a buffering proxy for MOO connections
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



#ifndef MOOPROXY__HEADER__RECALL
#define MOOPROXY__HEADER__RECALL



#include "world.h"



/* The recall command. Parse argstr, and recall lines (or print an error
 * to the user). */
extern void world_recall_command( World *wld, char *argstr );



#endif  /* ifndef MOOPROXY__HEADER__RECALL */
