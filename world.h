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



#ifndef MOOPROXY__HEADER__WORLD
#define MOOPROXY__HEADER__WORLD



#include "global.h"
#include "misc.h"



#define WLD_FDCHANGE 0x01
#define WLD_QUIT 0x02
#define WLD_PASSTEXT 0x04
#define WLD_BLOCKSRV 0x08



typedef struct _World World;
struct _World
{
	/* Essentials */
	char *name;
	char *configfile;       
	long flags;

	/* Listening connection */
	long listenport;
	int listen_fd;

	/* Authentication related stuff */
	char *authstring;
	int auth_fd[NET_MAXAUTHCONN];
	int auth_read[NET_MAXAUTHCONN];
	char *auth_buf[NET_MAXAUTHCONN];

	/* Data related to the server connection */
	char *host;
	char *ipv4_address;
	long port;
	int server_fd;

	Linequeue *server_rlines;
	Linequeue *server_slines;
	char *server_rbuffer;
	long server_rbfull;
	char *server_sbuffer;
	long server_sbfull;

	/* Data related to the client connection */
	int client_fd;

	Linequeue *client_rlines;
	Linequeue *client_slines;
	Linequeue *buffered_text;
	char *client_rbuffer;
	long client_rbfull;
	char *client_sbuffer;
	long client_sbfull;

	/* Options */
	char *commandstring;
	char *infostring;
};



/* This function creates a world by allocating memory and initialising
 * some variables. */
extern World *world_create( char * );

/* This function de-initialises a world and frees the allocated memory.
 * The world must be disconnected before destroying it */
extern void world_destroy( World * );

/* This function takes a worldname as argument and returns the filename
 * The string is strdup()ped, so it can bee free()d. */
extern void world_configfile_from_name( World * );

/* Queue a line for transmission to the client, and prefix the line with
 * the infostring, to indicate it's a message from mooproxy.
 * The line will not be free()d. */
extern void world_message_to_client( World *, char * );

/* Handle all the queued lines from the client.
 * Handling includes commands, MCP, logging, requeueing, etc. */
extern void world_handle_client_queue( World * );

/* Handle all the queued lines from the server.
 * Handling includes commands, MCP, logging, requeueing, etc. */
extern void world_handle_server_queue( World * );

/* Pass the lines from buffered_text to the client send buffer.
 * The long is the number of lines to pass, -1 for all. */
extern void world_pass_buffered_text( World *, long );



#endif  /* ifndef MOOPROXY__HEADER__WORLD */
