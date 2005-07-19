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



#ifndef MOOPROXY__HEADER__NETWORK
#define MOOPROXY__HEADER__NETWORK



#include "world.h"



/* This function opens the port on the local machine and listens to it.
 * On failure, it returns non-zero and places the error in the last arg. */
extern int world_bind_port( World *, char ** );

/* This function resolves the remote host to a useful address.
 * On failure, it returns non-zero and places the error in the last arg. */
extern int world_resolve_server( World *, char ** );

/* Return 1 if we're connected to the client, 0 if not. */
extern int world_connected_to_client( World * );

/* Return 1 if we're connected to the server, 0 if not. */
extern int world_connected_to_server( World * );

/* This function connects to the resolved address.
 * On failure, it returns non-zero and places the error in the last arg. */
extern int world_connect_server( World *, char ** );

/* This function disconnects from the server. */
extern void world_disconnect_server( World * );

/* This function disconnects the client. */
extern void world_disconnect_client( World * );

/* Wait for data from the network or timeout.
 * Parse data into lines, return lines to main loop. */
extern void wait_for_network( World * );

/* Try to flush the queued lines into the send buffer, and the send buffer
 * to the client. If this fails, leave the contents of the buffer in the
 * queued lines, mark the fd as write-blocked, and signal an FD change. */
extern void world_flush_client_txbuf( World * );

/* Try to flush the queued lines into the send buffer, and the send buffer
 * to the server. If this fails, leave the contents of the buffer in the
 * queued lines, mark the fd as write-blocked, and signal an FD change.
 * If the socket is closed, discard contents, and announce
 * disconnectedness. */
extern void world_flush_server_txbuf( World * );



#endif  /* ifndef MOOPROXY__HEADER_NETWORK */
