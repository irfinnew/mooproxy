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



#include <stdlib.h>
#include <string.h>

#include "mcp.h"



static int factor_mcp_msg( char *, char **, char **, char ***, int * );
static int factor_mcp_get_keyval( char **, char **, char ** );



/* Returns non-zero if the string is a MCP command */
extern int world_is_mcp( char *line )
{
	return line[0] == '#' && line[1] == '$' && line[2] == '#';
}



/* Handle MCP from the client. The line will be reused or freed. */
extern void world_do_mcp_client( World *wld, Line *line )
{
	char *str, *name, *key, **keyvals;
	int i, numkv;

	linequeue_append( wld->server_slines, line );

	if( !factor_mcp_msg( line->str, &name, &key, &keyvals, &numkv ) )
		return;

	/* Check for key */
	if( !wld->mcp_negotiated && !strcasecmp( name, "mcp" ) )
		for( i = 0; i < numkv * 2; i += 2 )
			if( !strcasecmp( keyvals[i], "authentication-key" ) )
			{
				world_message_to_client( wld, "Got MCP key!" );
				free( wld->mcp_key );
				wld->mcp_key = strdup( keyvals[i + 1] );
			}

	/* Check for negotiate-can */
	if( !wld->mcp_negotiated && !strcasecmp( name, "mcp-negotiate-can" ) )
	{
		world_message_to_client( wld, "Caught mcp-negotiate-can! "
				"Meddling..." );

		wld->mcp_negotiated = 1;

		asprintf( &str, "#$#mcp-negotiate-can %s package: "
				"dns-nl-icecrew-mcpreset min-version: 1.0 "
				"max-version: 1.0\n", wld->mcp_key );
		linequeue_append( wld->server_slines, line_create( str, -1 ) );
	}

	for( i = numkv * 2 - 1; i >= 0; i-- )
		free( keyvals[i] );

	free( keyvals );
	free( name );
	free( key );
}



/* Handle MCP from the server. The line will be reused or freed. */
extern void world_do_mcp_server( World *wld, Line *line )
{
	linequeue_append( wld->client_slines, line );
}



/* Assumes that the line starts with #$#
 * The line must end with \n\0
 * Returns 0 if it could not parse the MCP line */
static int factor_mcp_msg( char *line, char **name, char **key,
		char ***keyvals, int *num_keyvals )
{
	char *keystr, *valstr, **kv_ar = NULL;
	int i, numkv = 0;

	/* We assume it starts with #$# */
	line += 3;

	/* Get the MCP command name */
	for( i = 0; line[i] != ' '; i++ )
		if( line[i] == '\n' )
			return 0;

	*name = malloc( i + 1 );
	memcpy( *name, line, i );
	( *name )[i] = '\0';
	line += i;

	/* If the message name is "mcp", it's the keyless handshake */
	if( strcasecmp( *name, "mcp" ) )
	{
		/* Skip whitespace */
		while( line[0] == ' ' )
			line++;

		/* Get the MCP key */
		for( i = 0; line[i] != '\n' && line[i] != ' '; i++ )
			;

		*key = malloc( i + 1 );
		memcpy( *key, line, i );
		( *key )[i] = '\0';
		line += i;
	}
	else
	{
		*key = strdup( "" );
	}

	for(;;)
		switch( factor_mcp_get_keyval( &line, &keystr, &valstr ) )
		{
		case 0:
			/* Success; add key and value */
			kv_ar = realloc( kv_ar, ++numkv * 2 *
					sizeof( char * ) );
			kv_ar[(numkv - 1 ) * 2] = keystr;
			kv_ar[(numkv - 1 ) * 2 + 1] = valstr;
			break;
		case 1:
			/* EOL, we're done */
			*keyvals = kv_ar;
			*num_keyvals = numkv;
			return 1;
			break;
		case 2:
			/* Parse error; clean up and return */
			for( i = numkv * 2 - 1; i >= 0; i-- )
				free( kv_ar[i] );

			free( kv_ar );
			free( *name );
			free( *key );
			return 0;
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
	if( line[0] == '\n' )
		return 1;

	/* Get the key */
	for( i = 0; line[i] != ' '; i++ )
		if( line[i] == '\n' )
			return 2;

	if( line[i - 1] != ':' )
		return 2;

	*key = malloc( i );
	memcpy( *key, line, i - 1 );
	( *key )[i - 1] = '\0';
	line += i;

	/* Skip whitespace */
	while( line[0] == ' ' )
		line++;

	/* Value, unquoted string */
	if( line[0] != '"' )
	{
		for( i = 0; line[i] != '\n' && line[i] != ' '; i++ )
			;

		*val = malloc( i + 1 );
		memcpy( *val, line, i );
		( *val )[i] = '\0';
		line += i;

		*linep = line;
		return 0;
	}

	/* Value, quoted string */
	line++;

	for( i = 0; line[i] != '\n'; i++ )
	{
		if( line[i] == '\\' )
			quote = 1 - quote;
		if( line[i] == '"' && quote == 0 )
			break;
		if( line[i] == '"' && quote == 1 )
			quote = 0;
	}

	/* Did we find a closing quote ? */
	if( line[i] == '\n' )
	{
		free( *key );
		return 2;
	}

	/* Save the key */
	*val = malloc( i );
	memcpy( *val, line, i - 1 );
	( *val )[i - 1] = '\0';
	line += i;

	*linep = line;
	return 0;
}
