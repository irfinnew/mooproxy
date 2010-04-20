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



#ifndef MOOPROXY__HEADER__COMMAND
#define MOOPROXY__HEADER__COMMAND



/* Try to execute a command.
 * If the line is recognised as a command, execute it and return 1.
 * If the line is not recognised and should be processed further, return 0.
 * line will not be modified or consumed. */
extern int world_do_command( World *wld, char *line );



#endif  /* ifndef MOOPROXY__HEADER__COMMAND */
