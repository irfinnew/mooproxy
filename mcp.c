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



/* FIXME: This code is not very elegant. It should be rewritten.
 * Many eery interdependancies, arbitrary limits and unreadable or
 * unmaintanable code. Comments are also lacking. */



#include <stdio.h>  /* FIXME: remove */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mcp.h"
#include "misc.h"
#include "line.h"



/* FIXME: Don't arbitrarily limit the number of key-value pairs. */
#define MAX_KEYVALS 32

enum mcp_types { MCP_NORMAL, MCP_MULTI, MCP_MULTI_END };


typedef struct _MCPmsg MCPmsg;
struct _MCPmsg
{
	int type;
	char *name;
	char *key;
	int nkv;
	char *keys[MAX_KEYVALS];
	char *vals[MAX_KEYVALS];
};



static MCPmsg *factor_mcp_msg( char *, long );
static int factor_mcp_get_keyval( char **, char **, char ** );



extern int world_is_mcp( char *line )
{
	return line[0] == '#' && line[1] == '$' && line[2] == '#';
}



extern void world_do_mcp_client( World *wld, Line *line )
{
	char *str, *tmp;
	long len, i;
	MCPmsg *msg;

	/* Ugly hack: append a space to the string, the parsing code expects
	 * something bogus or whitespace before \0...
	 * This really needs to be re-written. */
	str = xmalloc( line->len + 2 );
	strcpy( str, line->str );
	strcat( str, " " );
	len = line->len + 1;

	linequeue_append( wld->server_toqueue, line );

	msg = factor_mcp_msg( str, len );

	if( !msg || msg->type != MCP_NORMAL )
	{
		free( msg );
		free( str );
		return;
	}

	/* Check for key */
	if( !wld->mcp_negotiated && !strcmp( msg->name, "mcp" ) )
		for( i = 0; i < msg->nkv; i++ )
			if( !strcmp( msg->keys[i], "authentication-key" ) )
			{
				free( wld->mcp_key );
				wld->mcp_key = xstrdup( msg->vals[i] );
			}

	/* Check for negotiate-can */
	if( !wld->mcp_negotiated && !strcmp( msg->name, "mcp-negotiate-can" ) )
	{
		wld->mcp_negotiated = 1;

		xasprintf( &tmp, "#$#mcp-negotiate-can %s package: "
				"dns-nl-icecrew-mcpreset min-version: 1.0 "
				"max-version: 1.0", wld->mcp_key );
		linequeue_append( wld->server_toqueue, line_create( tmp, -1 ) );
	}

	free( str );
	free( msg );
}



extern void world_do_mcp_server( World *wld, Line *line )
{
	line->flags = LINE_MCP;
	linequeue_append( wld->client_toqueue, line );
}



/* Assumes that the line starts with #$#
 * The string will be garbage afterwards, even on failure
 * Returns 0 if it could not parse the MCP line */
static MCPmsg *factor_mcp_msg( char *line, long len )
{
	MCPmsg *msg = xmalloc( sizeof( MCPmsg ) );
	int i;

	/* We assume it starts with #$# */
	line += 3;

	/* Get the MCP command name */
	for( i = 0; line[i] != ' '; i++ )
	{
		if( line[i] == '\0' )
		{
			free( msg );
			return NULL;
		}
		line[i] = tolower( line[i] );
	}

	msg->name = line;
	line += i;
	*line++ = '\0';

	/* If the message name is "mcp", it's the keyless handshake */
	if( strcmp( msg->name, "mcp" ) )
	{
		/* Skip whitespace */
		while( *line == ' ' )
			line++;

		/* Get the MCP key */
		for( i = 0; line[i] != '\0' && line[i] != ' '; i++ )
			;

		msg->key = line;
		line += i;
		*line++ = '\0';
	}
	else
	{
		msg->key = line - 1;
	}

	/* If the name is ':', it's a multiline end */
	if( msg->name[0] == ':' && msg->name[1] == '\0' )
	{
		msg->type = MCP_MULTI_END;
		msg->nkv = 0;
		return msg;
	}

	/* If the name is '*', it's a multiline */
	if( msg->name[0] == '*' && msg->name[1] == '\0' )
	{
		msg->type = MCP_MULTI;

		/* Get the key name */
		for( i = 0; line[i] != ' '; i++ )
		{
			if( line[i] == '\0' )
			{
				free( msg );
				return NULL;
			}
			line[i] = tolower( line[i] );
		}

		/* The key should really end with ':' */
		if( i < 1 || line[i - 1] != ':' )
		{
			free( msg );
			return NULL;
		}

		msg->keys[0] = line;
		line += i;
		*(line++ - 1) = '\0';

		/* Get the value */
		msg->vals[0] = line;

		msg->nkv = 1;
		return msg;
	}

	/* Well, it must be a normal msg */
	msg->type = MCP_NORMAL;
	msg->nkv = 0;

	for(;;)
	{
		switch( factor_mcp_get_keyval( &line, msg->keys + msg->nkv,
					msg->vals + msg->nkv ) )
		{
		case 0:
			/* Success; We have another keyval */
			msg->nkv++;
			break;
		case 1:
			/* EOL, we're done */
			return msg;
			break;
		case 2:
			/* Parse error; clean up and return */
			free( msg );
			return NULL;
			break;
		}

		if( msg->nkv == MAX_KEYVALS )
		{
			printf( "Maximum number of key-value pairs for a MCP "
					"message exceeded!\n" );
			free( msg );
			return NULL;
		}
	}
}



/* Returns 0 on succes, 1 on valid eol, 2 on parse error.
 * key and val are filled on success, garbage otherwise */
static int factor_mcp_get_keyval( char **linep, char **key, char **val )
{
	int i, quote = 0;
	char *line = *linep;

	/* Skip whitespace */
	while( line[0] == ' ' )
		line++;

	/* EOL? */
	if( line[0] == '\0' )
		return 1;

	/* Get the key */
	for( i = 0; line[i] != ' '; i++ )
	{
		if( line[i] == '\0' )
			return 2;
		line[i] = tolower( line[i] );
	}

	if( i < 1 || line[i - 1] != ':' )
		return 2;

	*key = line;
	line += i;
	*(line++ - 1) = '\0';

	/* Skip whitespace */
	while( line[0] == ' ' )
		line++;

	if( line[0] == '\0' )
		return 2;

	/* Value, unquoted string */
	if( line[0] != '"' )
	{
		for( i = 0; line[i] != '\0' && line[i] != ' '; i++ )
			;

		*val = line;
		line += i;
		*line++ = '\0';

		*linep = line;
		return 0;
	}

	/* Value, quoted string */
	line++;

	for( i = 0; line[i] != '\0'; i++ )
	{
		if( line[i] == '\\' )
			quote = 1 - quote;
		if( line[i] == '"' && quote == 0 )
			break;
		if( line[i] == '"' && quote == 1 )
			quote = 0;
	}

	/* Did we find a closing quote ? */
	if( line[i] == '\0' )
		return 2;

	/* Save the key */

	*val = line;
	line += i;
	*line++ = '\0';

	*linep = line;
	return 0;
}



extern void world_send_mcp_reset( World *wld )
{
	char *str;

	/* If the MCP negotiation has not taken place, do it now. */
	if( !wld->mcp_negotiated )
	{
		str = xstrdup( "#$#mcp authentication-key: mehkey version: "
				"1.0 to: 2.1" );
		linequeue_append( wld->server_toqueue, line_create( str, -1 ) );
		str = xstrdup( "#$#mcp-negotiate-can mehkey package: "
				"dns-nl-icecrew-mcpreset min-version: 1.0 "
				"max-version: 1.0" );
		linequeue_append( wld->server_toqueue, line_create( str, -1 ) );
		str = xstrdup( "#$#mcp-negotiate-end mehkey" );
		linequeue_append( wld->server_toqueue, line_create( str, -1 ) );

		free( wld->mcp_key );
		wld->mcp_key = xstrdup( "mehkey" );
	}

	/* Send the MCP reset */
	xasprintf( &str, "#$#dns-nl-icecrew-mcpreset-reset %s",wld->mcp_key );
	linequeue_append( wld->server_toqueue, line_create( str, -1 ) );

	free( wld->mcp_key );
	wld->mcp_key = NULL;
	wld->mcp_negotiated = 0;
}
