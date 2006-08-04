/*
 *
 *  mooproxy - a buffering proxy for moo-connections
 *  Copyright (C) 2001-2006 Marcel L. Moreaux <marcelm@luon.net>
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



#ifndef MOOPROXY__HEADER__WORLD
#define MOOPROXY__HEADER__WORLD



#include <sys/types.h>
#include <time.h>

#include "global.h"
#include "line.h"



/* Action flags */
#define WLD_SHUTDOWN		0x00000001
#define WLD_CLIENTQUIT		0x00000002
#define WLD_SERVERQUIT		0x00000004
#define WLD_SERVERRESOLVE	0x00000008
#define WLD_SERVERCONNECT	0x00000010

/* Status flags */
#define WLD_NOTCONNECTED	0x00010000

/* Server statuses */
#define ST_DISCONNECTED 1
#define ST_RESOLVING 2
#define ST_CONNECTING 3
#define ST_CONNECTED 4



/* BindResult struct. Contains the result of the attempt to bind on a port. */
typedef struct _BindResult BindResult;
struct _BindResult
{
	/* Contains error msg if a fatal init error occurred. Otherwise NULL. */
	char *fatal;

	/* Number of address families we tried to bind for */
	int af_count;
	/* Number of address families we succesfully bound for */
	int af_success_count;
	/* Array of booleans indicating each AF's succes or failure */
	int *af_success;
	/* Human-readable message for each AF */
	char **af_msg;

	/* Human-readable conclusion */
	char *conclusion;
};



/* The World struct. Contains all configuration and state information for a
 * world. */
typedef struct _World World;
struct _World
{
	/* Essentials */
	char *name;
	char *configfile;       
	unsigned long flags;
	char *lockfile;
	int lockfile_fd;

	/* Destination */
	char *dest_host;
	long dest_port;

	/* Listening connection */
	long listenport;
	int *listen_fds;
	BindResult *bindresult;

	/* Authentication related stuff */
	char *auth_md5hash;
	char *auth_literal;
	int auth_connections;
	int auth_correct[NET_MAXAUTHCONN];
	int auth_fd[NET_MAXAUTHCONN];
	int auth_read[NET_MAXAUTHCONN];
	char *auth_buf[NET_MAXAUTHCONN];
	char *auth_address[NET_MAXAUTHCONN];

	/* Data related to the server connection */
	int server_status;
	int server_fd;
	char *server_port;
	char *server_host;
	char *server_address;

	pid_t server_resolver_pid;
	int server_resolver_fd;
	char *server_addresslist;
	int server_connecting_fd;

	Linequeue *server_rxqueue;
	Linequeue *server_toqueue;
	Linequeue *server_txqueue;
	char *server_rxbuffer;
	long server_rxfull;
	char *server_txbuffer;
	long server_txfull;

	/* Data related to the client connection */
	int client_status;
	int client_fd;
	char *client_address;
	char *client_prev_address;
	time_t client_last_connected;

	Linequeue *client_rxqueue;
	Linequeue *client_toqueue;
	Linequeue *client_txqueue;
	char *client_rxbuffer;
	long client_rxfull;
	char *client_txbuffer;
	long client_txfull;

	/* Miscellaneous */
	Linequeue *buffered_lines;
	Linequeue *history_lines;
	long buffer_dropped_lines;

	/* Timer stuff */
	int timer_prev_sec;
	int timer_prev_min;
	int timer_prev_hour;
	int timer_prev_day;
	int timer_prev_mon;
	int timer_prev_year;

	/* Logging */
	Linequeue *log_queue;
	Linequeue *log_current;
	long log_currentday;
	int log_fd;
	char *log_buffer;
	long log_bfull;
	char *log_lasterror;
	time_t log_lasterrtime;

	/* MCP stuff */
	int mcp_negotiated;
	char *mcp_key;
	char *mcp_initmsg;

	/* Options */
	int logging_enabled;
	int autologin;
	char *commandstring;
	char *infostring;
	long context_on_connect;
	long max_buffered_size;
	long max_history_size;
	int strict_commands;
};



/* Create a world struct, and initialise it with default values. The worlds
 * name will be set to wldname */
extern World *world_create( char *wldname );

/* Free all memory allocated for this world, close all FD's, and then free
 * the world itself. */
extern void world_destroy( World *wld );

/* Get a list of all World objects. The number of worlds will be placed in
 * count, and an array of pointers to Worlds in wldlist.
 * Neither the array nor the World objects should be freed. */
extern void world_get_list( int *count, World ***wldlist );

/* Initialize a BindResult object to empty/zeroes. */
extern void world_bindresult_init( BindResult *bindresult );

/* Free all allocated resources in a BindResult object. */
extern void world_bindresult_free( BindResult *bindresult );

/* Construct the path of the configuration file, based on wld->name, and put
 * it in wld->configfile. */
extern void world_configfile_from_name( World *wld );

/* Queue a line for transmission to the client, and prefix the line with
 * the infostring, to indicate it's a message from mooproxy.
 * The fmt and ... arguments are like printf(). Return the line object,
 * so the caller may frobble with line->flags or some such. */
extern Line *world_msg_client( World *wld, const char *fmt, ... );

/* Measure the length of dynamic queues such as buffered and history,
 * and remove the oldest lines until they are small enough.
 * This puts a limit on the amount of memory these queues will occupy. */
extern void world_trim_dynamic_queues( World *wld );

/* Replicate the configured number of history lines to provide context for
 * the newly connected client, and then pass all buffered lines. */
extern void world_recall_and_pass( World *wld );

/* Duplicate the last <lines> lines from the history queue to the client. */
extern void world_recall_history_lines( World *wld, int lines );

/* (Auto-)login to the server. If autologin is enabled, log in.
 * If autologin is disabled, only log in if override is non-zero. */
extern void world_login_server( World *wld, int override );



#endif  /* ifndef MOOPROXY__HEADER__WORLD */
