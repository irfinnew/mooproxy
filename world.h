/*
 *
 *  mooproxy - a smart proxy for MUD/MOO connections
 *  Copyright 2001-2011 Marcel Moreaux
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



/* World flags */
#define WLD_ACTIVATED		0x00000001
#define WLD_NOTCONNECTED	0x00000002
#define WLD_CLIENTQUIT		0x00000004
#define WLD_SERVERQUIT		0x00000008
#define WLD_RECONNECT		0x00000010
#define WLD_SERVERRESOLVE	0x00000020
#define WLD_SERVERCONNECT	0x00000040
#define WLD_LOGLINKUPDATE	0x00000080
#define WLD_REBINDPORT		0x00000100
#define WLD_SHUTDOWN		0x00000200

/* Server/client statuses */
#define ST_DISCONNECTED		0x01
#define ST_RESOLVING		0x02
#define ST_CONNECTING		0x03
#define ST_CONNECTED		0x04
#define ST_RECONNECTWAIT	0x05

/* Authentication connection statuses */
#define AUTH_ST_WAITNET		0x01
#define AUTH_ST_VERIFY		0x02
#define AUTH_ST_CORRECT		0x03


/* BindResult struct. Contains the result of the attempt to bind on a port. */
typedef struct BindResult BindResult;
struct BindResult
{
	/* Contains error msg if a fatal init error occurred. Otherwise NULL. */
	char *fatal;

	/* Number of address families we tried to bind for */
	int af_count;
	/* Number of address families we successfully bound for */
	int af_success_count;
	/* Array of booleans indicating each AF's success or failure */
	int *af_success;
	/* Human-readable message for each AF */
	char **af_msg;

	/* Array of af_success_count + 1 filedescriptors. The last one is -1. */
	int *listen_fds;

	/* Human-readable conclusion */
	char *conclusion;
};



/* The World struct. Contains all configuration and state information for a
 * world. */
typedef struct World World;
struct World
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
	long requestedlistenport;
	int *listen_fds;
	BindResult *bindresult;

	/* Authentication related stuff */
	char *auth_hash;
	char *auth_literal;
	int auth_tokenbucket;
	int auth_connections;
	int auth_status[NET_MAXAUTHCONN];
	int auth_ispriv[NET_MAXAUTHCONN];
	int auth_fd[NET_MAXAUTHCONN];
	int auth_read[NET_MAXAUTHCONN];
	char *auth_buf[NET_MAXAUTHCONN];
	char *auth_address[NET_MAXAUTHCONN];
	Linequeue *auth_privaddrs;

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

	int reconnect_enabled;
	int reconnect_delay;
	time_t reconnect_at;

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
	time_t client_connected_since;
	time_t client_last_connected;
	long client_login_failures;
	char *client_last_failaddr;
	time_t client_last_failtime;
	time_t client_last_notconnmsg;

	Linequeue *client_rxqueue;
	Linequeue *client_toqueue;
	Linequeue *client_txqueue;
	char *client_rxbuffer;
	long client_rxfull;
	char *client_txbuffer;
	long client_txfull;

	/* Miscellaneous */
	Linequeue *buffered_lines;
	Linequeue *inactive_lines;
	Linequeue *history_lines;
	long dropped_inactive_lines;
	long dropped_buffered_lines;
	time_t easteregg_last;

	/* Timer stuff */
	int timer_prev_sec;
	int timer_prev_min;
	int timer_prev_hour;
	long timer_prev_day;
	int timer_prev_mon;
	int timer_prev_year;

	/* Logging */
	Linequeue *log_queue;
	Linequeue *log_current;
	long dropped_loggable_lines;
	long log_currentday;
	time_t log_currenttime;
	char *log_currenttimestr;
	int log_fd;
	char *log_buffer;
	long log_bfull;
	char *log_lasterror;
	time_t log_lasterrtime;

	/* MCP stuff */
	int mcp_negotiated;
	char *mcp_key;
	char *mcp_initmsg;

	/* Ansi Client Emulation (ACE) */
	int ace_enabled;
	int ace_cols;
	int ace_rows;
	char *ace_prestr;
	char *ace_poststr;

	/* Options */
	int autologin;
	int autoreconnect;
	char *commandstring;
	int strict_commands;
	char *infostring;
	char *infostring_parsed;
	char *newinfostring;
	char *newinfostring_parsed;
	long context_lines;
	long buffer_size;
	long logbuffer_size;
	int logging;
	int log_timestamps;
	int easteregg_version;
};



/* Create a world struct, and initialise it with default values. The world's
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

/* Like world_msg_client(), but uses newinfostring as a prefix. */
extern Line *world_newmsg_client( World *wld, const char *fmt, ... );

/* Schedule the next reconnect attempt. */
extern void world_schedule_reconnect( World *wld );

/* Attempt to reconnect to the server. */
extern void world_do_reconnect( World *wld );

/* Decrease the delay between reconnect attempts. */
extern void world_decrease_reconnect_delay( World *wld );

/* Measure the length of dynamic queues such as buffered and history,
 * and remove the oldest lines until they are small enough.
 * This puts a limit on the amount of memory these queues will occupy. */
extern void world_trim_dynamic_queues( World *wld );

/* Replicate the configured number of history lines to provide context for
 * the newly connected client, and then pass all buffered lines. */
extern void world_recall_and_pass( World *wld );

/* (Auto-)login to the server. If autologin is enabled, log in.
 * If autologin is disabled, only log in if override is non-zero. */
extern void world_login_server( World *wld, int override );

/* Appends the lines in the inactive queue to the history queue, effectively
 * remove the 'possibly new' status from these lines. */
extern void world_inactive_to_history( World *wld );

/* Recall (at most) count lines from wld->history_lines.
 * Return a newly created Linequeue object with copies of the recalled lines.
 * The lines have their flags set to LINE_RECALLED, and their strings
 * have ASCII BELLs stripped out. */
extern Linequeue *world_recall_history( World *wld, long count );

/* Attempt to bind to wld->requestedlistenport. If successful, close old
 * listen fds and install new ones. If unsuccessful, retain old fds. */
extern void world_rebind_port( World *wld );

/* Enable Ansi Client Emulation for this world.
 * Sends ansi sequences to initialize the screen, sets wld->ace_enabled to
 * true, and initalizes the other wld->ace_* variables.
 * Returns true if initialization was successful, false otherwise. */
extern int world_enable_ace( World *wld );

/* Disable Ansi Client Emulation for this world.
 * Sends ansi sequences to reset the screen and cleans up wld->ace_*  */
extern void world_disable_ace( World *wld );

/* Check for, and respond to, easter eggs. */
extern void world_easteregg_server( World *wld, Line *line );

/* Signal shutdown if there are no unlogged lines (or force = 1).
 * Returns 0 if shutdown is refused, 1 otherwise.
 * This function messages the client.
 * fromsig should be one when called from SIGTERM/QUIT, 0 otherwise. */
extern int world_start_shutdown( World *wld, int force, int fromsig );


#endif  /* ifndef MOOPROXY__HEADER__WORLD */
