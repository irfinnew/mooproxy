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



#ifndef MOOPROXY__HEADER__LOG
#define MOOPROXY__HEADER__LOG



#include "world.h"



/* Submit line for logging.
 * line is not consumed, and can be processed further. */
extern void world_log_line( World *wld, Line *line );

/* Attempt to flush all accumulated lines to the logfile(s).
 * Should be called regularly. */
extern void world_flush_client_logqueue( World *wld );



#endif  /* ifndef MOOPROXY__HEADER__LOG */
