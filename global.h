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



#ifndef MOOPROXY__HEADER__GLOBAL
#define MOOPROXY__HEADER__GLOBAL



/* Mooproxy version */
#define VERSIONSTR "0.0.5"

/* Configuration paths */
#define CONFIGDIR ".mooproxy/"
#define WORLDSDIR ".mooproxy/worlds/"
#define LOGSDIR ".mooproxy/logs/"
#define LOCKSDIR ".mooproxy/locks/"
#define LOG_EXTENSION ".log"

/* Exit codes */
#define EXIT_OK 0
#define EXIT_HELP 1
#define EXIT_UNKNOWNOPT 2
#define EXIT_NOWORLD 3
#define EXIT_CONFIGDIRS 4
#define EXIT_NOAUTH 5
#define EXIT_HOMEDIR 6
#define EXIT_NOSUCHWORLD 7
#define EXIT_CONFIGERR 8
#define EXIT_SOCKET 9
#define EXIT_BIND 10
#define EXIT_LISTEN 11
#define EXIT_NOHOST 12
#define EXIT_RESOLV 13
#define EXIT_CONNECT 15

/* Some default values */
#define DEFAULT_CMDSTRING "/"
#define DEFAULT_INFOSTRING "% "

/* Maximum number of authenticating connections */
#define NET_MAXAUTHCONN 4
/* Maximum number of characters accepted from an authenticating client.
 * This effectively also limits the authstring length */
#define NET_MAXAUTHLEN 1024
/* Various network messages */
#define NET_AUTHSTRING "Welcome, please authenticate."
#define NET_AUTHFAIL "Authentication failed, goodbye."
#define NET_AUTHGOOD "Authentication succesful."
#define NET_CONNTAKEOVER "Connection is taken over."
/* Size of blocks-to-lines buffer in bytes.
 * Note that this limits the maximum line length. */
#define NET_BBUFFER_LEN 102400
/* string to be appended to messages */
#define MESSAGE_TERMINATOR "[0m\n"



#endif  /* ifndef MOOPROXY__HEADER__GLOBAL */
