/*
 *
 *  mooproxy - a buffering proxy for MOO connections
 *  Copyright (C) 2001-2011 Marcel L. Moreaux <marcelm@luon.net>
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

/*
 * For some more information about MCP, see the README.
 */



#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mcp.h"
#include "misc.h"
#include "line.h"



static int mcp_is_init( Line *line );
static int mcp_is_negotiate_can( Line *line );
static char *extract_mcpkey( Line *line );
static char *skip_mcp_value( char *str );



extern int world_is_mcp( char *line )
{
	return line[0] == '#' && line[1] == '$' && line[2] == '#';
}



extern void world_do_mcp_client( World *wld, Line *line )
{
	char *mcpkey = NULL, *tmp;
	Line *msg;

	/* First of all, pass the line on to the server. */
	line->flags = LINE_MCP;
	linequeue_append( wld->server_toqueue, line );

	/* If we're already negotiated, we're done. */
	if( wld->mcp_negotiated )
		return;

	/* Check if it's the "mcp" message. */
	if( mcp_is_init( line ) )
	{
		mcpkey = extract_mcpkey( line );

		/* If the returned key is NULL, don't kill any existing key. */
		if( mcpkey != NULL )
		{
			free( wld->mcp_key );
			wld->mcp_key = mcpkey;
		}
	}

	/* Check if it's the mcp-negotiate-can message. */
	if( mcp_is_negotiate_can( line ) && wld->mcp_key != NULL )
	{
		wld->mcp_negotiated = 1;

		/* Insert a client->server and server->client
		 * mcp-negotiate-can message. This is necessary to get the
		 * server and the client listening for mcpreset commands. */
		xasprintf( &tmp, "#$#mcp-negotiate-can %s package: "
				"dns-nl-icecrew-mcpreset min-version: 1.0 "
				"max-version: 1.0", wld->mcp_key );
		msg = line_create( tmp, -1 );
		msg->flags = LINE_MCP;
		linequeue_append( wld->server_toqueue, msg );
		linequeue_append( wld->client_toqueue, line_dup( msg ) );
	}
}



extern void world_do_mcp_server( World *wld, Line *line )
{
	/* Flag the line as MCP, and pass it on. */
	line->flags = LINE_MCP;
	linequeue_append( wld->client_toqueue, line );

	/* Now, if we don't have an initmsg yet, and this is the "mcp"
	 * message, save this line for later. */
	if( wld->mcp_initmsg == NULL && mcp_is_init( line ) )
		wld->mcp_initmsg = xstrdup( line->str );
}



extern void world_mcp_server_connect( World *wld )
{
	char *str;
	Line *line;

	/* If we don't have a key, MCP is simply uninitialized. The server
	 * should greet us with the MCP handshake, and the client should
	 * reply, getting everything going. */

	/* If we have a key, but we're not negotiated, negotiate now. */
	if( wld->mcp_key != NULL && !wld->mcp_negotiated )
	{
		xasprintf( &str, "#$#mcp-negotiate-can %s package: "
				"dns-nl-icecrew-mcpreset min-version: 1.0 "
				"max-version: 1.0", wld->mcp_key );
		line = line_create( str, -1 );
		line->flags = LINE_MCP;
		linequeue_append( wld->client_toqueue, line );
		xasprintf( &str, "#$#mcp-negotiate-end %s", wld->mcp_key );
		line = line_create( str, -1 );
		line->flags = LINE_MCP;
		linequeue_append( wld->client_toqueue, line );
		wld->mcp_negotiated = 1;
	}

	/* If we have a key and we're negotiated, do the mcp reset. */
	if( wld->mcp_key != NULL && wld->mcp_negotiated )
	{
		/* Send the mcp reset. */
		xasprintf( &str, "#$#dns-nl-icecrew-mcpreset-reset %s",
				wld->mcp_key );
		line = line_create( str, -1 );
		line->flags = LINE_MCP;
		linequeue_append( wld->client_toqueue, line );
	}

	/* Erase all MCP administration. */
	wld->mcp_negotiated = 0;

	free( wld->mcp_key );
	wld->mcp_key = NULL;

	free( wld->mcp_initmsg );
	wld->mcp_initmsg = NULL;
}



extern void world_mcp_client_connect( World *wld )
{
	char *str;
	Line *line;

	/* If we don't have an MCP key, the best we can do is relay the MCP
	 * handshake from the server to the client. The client should then
	 * react (if it supports MCP) to get the whole thing going. */
	if( wld->mcp_key == NULL )
	{
		/* If we have not captured a handshake msg from the server,
		 * either the server does not support MCP, or something's
		 * screwed. */
		if( wld->mcp_initmsg != NULL )
		{
			line = line_create( xstrdup( wld->mcp_initmsg ), -1 );
			line->flags = LINE_MCP;
			linequeue_append( wld->client_toqueue, line );
		}
	}

	/* If we have a key, but we're not negotiated, negotiate now. */
	if( wld->mcp_key != NULL && !wld->mcp_negotiated )
	{
		xasprintf( &str, "#$#mcp-negotiate-can %s package: "
				"dns-nl-icecrew-mcpreset min-version: 1.0 "
				"max-version: 1.0", wld->mcp_key );
		line = line_create( str, -1 );
		line->flags = LINE_MCP;
		linequeue_append( wld->server_toqueue, line );
		xasprintf( &str, "#$#mcp-negotiate-end %s", wld->mcp_key );
		line = line_create( str, -1 );
		line->flags = LINE_MCP;
		linequeue_append( wld->server_toqueue, line );
		wld->mcp_negotiated = 1;
	}

	/* If we have a key and we're negotiated, do the mcp reset. */
	if( wld->mcp_key != NULL && wld->mcp_negotiated )
	{
		/* Send the mcp reset. */
		xasprintf( &str, "#$#dns-nl-icecrew-mcpreset-reset %s",
				wld->mcp_key );
		line = line_create( str, -1 );
		line->flags = LINE_MCP;
		linequeue_append( wld->server_toqueue, line );

		/* We're not negotiated anymore. */
		wld->mcp_negotiated = 0;

		/* Any old key is now invalid. */
		free( wld->mcp_key );
		wld->mcp_key = NULL;

		/* We'll receive a new "mcp" message. */
		free( wld->mcp_initmsg );
		wld->mcp_initmsg = NULL;
	}
}



/* Check if this is the "mcp" message. Returns a boolean. */
static int mcp_is_init( Line *line )
{
	return !strncasecmp( line->str, "#$#mcp ", 7 );
}



/* Check if this is the "mcp-negotiate-can" message. Returns a boolean. */
static int mcp_is_negotiate_can( Line *line )
{
	/* Note that we do _not_ check the MCP key on this message.
	 * We only check the mcp-negotiate-can from the client, which is
	 * assumed to be trusted. */
	return !strncasecmp( line->str, "#$#mcp-negotiate-can ", 21 );
}



/* Attempts to extract the value associated with the "authentication-key"
 * key from this message. The message should be a "mcp" message.
 * Returns a copy of the authentication key (which should be freed), or
 * NULL on failure. */
static char *extract_mcpkey( Line *line )
{
	char *str = line->str, *end, *mcpkey;
	int foundkey;

	/* If this is not a "mcp" message, we can't parse it. */
	if( strncasecmp( str, "#$#mcp ", 7 ) )
		return NULL;

	/* Skip past the first part of the string, we parsed that. */
	str += 6;

	for(;;)
	{
		/* Skip whitespace */
		while( *str == ' ' )
			str++;

		/* End of string, we fail. */
		if( *str == '\0' )
			return NULL;

		/* See if the current key is "authentication-key", and
		 * remember that for later. */
		foundkey = !strncasecmp( str, "authentication-key: ", 20 );

		/* Skip over the key. */
		while( *str != ':' && *str != ' ' && *str != '\0' )
			str++;

		/* A key should really be followed by ':'. */
		if( *str != ':' )
			return NULL;

		str++;
		/* Skip whitespace. */
		while( *str == ' ' )
			str++;

		/* If the key was not the one we're looking for, just skip
		 * over the value and continue. */
		if( !foundkey )
		{
			str = skip_mcp_value( str );
			/* Fail on error. */
			if( str == NULL )
				return NULL;
			continue;
		}

		/* The key was the one we're looking for!
		 * First, find the end of the value. */
		end = str;
		while( *end != ' ' && *end != '\0' )
			end++;

		/* Duplicate it, and return it (stripping off quotes). */
		mcpkey = xmalloc( end - str + 1 );
		strncpy( mcpkey, str, end - str );
		mcpkey[end - str] = '\0';
		return remove_enclosing_quotes( mcpkey );
	}
}



static char *skip_mcp_value( char *str )
{
	/* If the value does not start with ", it's a simple value.
	 * Just skip until the first space. */
	if( *str != '"' )
	{
		while( *str != ' ' && *str != '\0' )
			str++;

		return str;
	}

	/* It starts with ", so it's a quoted value. First, skip over the ". */
	str++;

	/* Scan until a " or end of string. */
	while( *str != '"' && *str != '\0' )
	{
		/* If we encounter a \, the following character is escaped,
		 * so we skip the next character. */
		if( *str == '\\' && str[1] != '\0' )
			str++;

		str++;
	}

	/* It's a parse error if we hit the end of the string. */
	if( *str != '"' )
		return NULL;

	return str + 1;
}
