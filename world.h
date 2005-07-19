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



#include <time.h>

#include "global.h"
#include "misc.h"



#define WLD_SHUTDOWN 0x01
#define WLD_CLIENTQUIT 0x02
#define WLD_SERVERQUIT 0x04



typedef struct _World World;
struct _World
{
	/* Essentials */
	char *name;
	char *configfile;       
	long flags;

	/* Destination */
	char *dest_host;
	char *dest_ipv4_address;
	long dest_port;

	/* Listening connection */
	long listenport;
	int listen_fd;

	/* Authentication related stuff */
	char *authstring;
	int auth_connections;
	int auth_correct[NET_MAXAUTHCONN];
	int auth_fd[NET_MAXAUTHCONN];
	int auth_read[NET_MAXAUTHCONN];
	char *auth_buf[NET_MAXAUTHCONN];

	/* Data related to the server connection */
	int server_fd;

	Linequeue *server_rxqueue;
	Linequeue *server_txqueue;
	char *server_rxbuffer;
	long server_rxfull;
	char *server_txbuffer;
	long server_txfull;

	/* Data related to the client connection */
	int client_fd;

	Linequeue *client_rxqueue;
	Linequeue *client_txqueue;
	Linequeue *client_logqueue;
	Linequeue *client_buffered;
	Linequeue *client_history;
	char *client_rxbuffer;
	long client_rxfull;
	char *client_txbuffer;
	long client_txfull;

	/* Miscellaneous */
	long buffer_dropped_lines;

	/* Timer stuff */
	int timer_prev_sec;
	int timer_prev_min;
	int timer_prev_hour;
	int timer_prev_day;
	int timer_prev_mon;
	int timer_prev_year;

	/* Logging */
	int log_fd;
	int logging_enabled;

	/* MCP stuff */
	int mcp_negotiated;
	char *mcp_key;
	
	/* Options */
	char *commandstring;
	char *infostring;
	long context_on_connect;
	long max_buffered_size;
	long max_history_size;
	int strict_commands;
};



/* This function creates a world by allocating memory and initialising
 * some variables. */
extern World *world_create( char * );

/* This function de-initialises a world and frees the allocated memory.
 * The world must be disconnected before destroying it */
extern void world_destroy( World * );

/* This function takes a worldname as argument and returns the filename
 * The string is strdup()ped, so it can be free()d. */
extern void world_configfile_from_name( World * );

/* Queue a line for transmission to the client, and prefix the line with
 * the infostring, to indicate it's a message from mooproxy.
 * The line will not be free()d. */
extern void world_message_to_client( World *, char * );

/* Queue a line for transmission to the client, and prefix the line with
 * the infostring, to indicate it's a message from mooproxy.
 * The line will not be free()d. */
extern void world_checkpoint_to_client( World *, char * );

/* Store the line in the history buffer. */
extern void world_store_history_line( World *, Line * );

/* Measure the length of dynamic queues such as buffered and history,
 * and remove the oldest lines until they are small enought. */
extern void world_trim_dynamic_queues( World * );

/* Replicate the set number of history lines to provide context for the newly
 * connected client, and then pass all buffered lines. */
extern void world_recall_and_pass( World * );

/* Recall a number of history lines. The second parameter is the
 * number of lines to recall. */
extern void world_recall_history_lines( World *, int );



#endif  /* ifndef MOOPROXY__HEADER__WORLD */
