/*
 *
 *  mooproxy - a buffering proxy for moo-connections
 *  Copyright (C) 2001-2005 Marcel L. Moreaux <marcelm@luon.net>
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



#ifndef MOOPROXY__HEADER__COMMAND
#define MOOPROXY__HEADER__COMMAND



/* Execute a command.
 * If the lined is recognised as a command, it's executed, the line is
 * consumed, and the return value is 1.
 * If it was not recognised, the line is not consumed (and should be processed
 * further), and the return value is 0. */
extern int world_do_command( World *wld, char *line );



#endif  /* ifndef MOOPROXY__HEADER_COMMAND */
