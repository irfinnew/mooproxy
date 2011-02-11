/*
 *
 *  mooproxy - a buffering proxy for MOO connections
 *  Copyright (C) 2001-2011 Marcel L. Moreaux <marcelm@qvdr.net>
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



#ifndef MOOPROXY__HEADER__MCP
#define MOOPROXY__HEADER__MCP



#include "world.h"
#include "line.h"



/* Checks if line is a MCP command. Returns 1 if it is, 0 if not. */
extern int world_is_mcp( char *line );

/* Handle a MCP line from the client. line is consumed. */
extern void world_do_mcp_client( World *wld, Line *line );

/* Handle a MCP line from the server. line is consumed. */
extern void world_do_mcp_server( World *wld, Line *line );

/* Call when we just connected to the server, to handle MCP administration. */
extern void world_mcp_server_connect( World *wld );

/* Call when the client connected, to handle MCP administration */
extern void world_mcp_client_connect( World *wld );



#endif  /* ifndef MOOPROXY__HEADER__MCP */
