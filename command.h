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



#ifndef MOOPROXY__HEADER__COMMAND
#define MOOPROXY__HEADER__COMMAND



/* Checks if the given string is a valid command for the given world.
 * If it's a valid command, it's executed (and output is sent to the client).
 * The return value is 1 for a succesful command, 0 for an invalid one.
 * If the command is succesful, the line is freed, otherwise not. */
extern int world_do_command( World *, char * );



#endif  /* ifndef MOOPROXY__HEADER_COMMAND */
