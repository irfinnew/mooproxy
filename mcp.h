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



#ifndef MOOPROXY__HEADER__MCP
#define MOOPROXY__HEADER__MCP



#include "world.h"
#include "misc.h"



/* Returns non-zero if the string is a MCP command */
extern int world_is_mcp( char * );

/* Handle MCP from the client. The line will be reused or freed. */
extern void world_do_mcp_client( World *, Line * );

/* Handle MCP from the server. The line will be reused or freed. */
extern void world_do_mcp_server( World *, Line * );



#endif  /* ifndef MOOPROXY__HEADER_MCP */
