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



#ifndef MOOPROXY__HEADER__NETWORK
#define MOOPROXY__HEADER__NETWORK



#include "world.h"



/* Wait for data from the network or timeout.
 * Any data received from the network is parsed into lines and put in the
 * respective input queues, so they can be processed further. */
extern void wait_for_network( World *wld );

/* Open wld->listenport on the local machine, and start listening on it.
 * The returned BindResult does not need to be free'd. */
extern void world_bind_port( World *wld, long port );

/* Take the first address in wld->server_addresslist, start a non-blocking
 * connect to that address, and sets wld->server_status to ST_CONNECTING. */
extern void world_start_server_connect( World *wld );

/* If mooproxy is in the process of connecting to the server, abort all that
 * and clean up. Note that this does not abort _resolving_ attempts. */
extern void world_cancel_server_connect( World *wld );

/* Disconnect mooproxy from the server. */
extern void world_disconnect_server( World *wld );

/* Disconnect the client from mooproxy. */
extern void world_disconnect_client( World *wld );

/* Try to flush the queued lines into the send buffer, and the send buffer
 * to the client. If this fails, leave the contents of the buffer in the
 * queued lines, mark the fd as write-blocked, and signal an FD change. */
extern void world_flush_client_txbuf( World *wld );

/* Try to flush the queued lines into the send buffer, and the send buffer
 * to the server. If this fails, leave the contents of the buffer in the
 * queued lines, mark the fd as write-blocked, and signal an FD change.
 * If the socket is closed, discard contents, and announce
 * disconnectedness. */
extern void world_flush_server_txbuf( World *wld );

/* Add some tokens to the auth token bucket. */
extern void world_auth_add_bucket( World *wld );



#endif  /* ifndef MOOPROXY__HEADER__NETWORK */
