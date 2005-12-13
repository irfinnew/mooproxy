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



#ifndef MOOPROXY__HEADER__GLOBAL
#define MOOPROXY__HEADER__GLOBAL



/* Mooproxy version */
#define VERSIONSTR "0.0.7.1"

/* Configuration dirnames */
#define CONFIGDIR ".mooproxy/"
#define WORLDSDIR ".mooproxy/worlds/"
#define LOGSDIR ".mooproxy/logs/"

/* Some default option values */
#define DEFAULT_AUTOLOGIN 0
#define DEFAULT_CMDSTRING "/"
#define DEFAULT_INFOSTRING "% "
#define DEFAULT_LOGENABLE 1
#define DEFAULT_CONTEXTLINES 50
#define DEFAULT_MAXBUFFERED 2048
#define DEFAULT_MAXHISTORY 256
#define DEFAULT_STRICTCMDS 1

/* Maximum number of authenticating connections */
#define NET_MAXAUTHCONN 8
/* Maximum number of characters accepted from an authenticating client.
 * This effectively also limits the authentication string length */
#define NET_MAXAUTHLEN 128
/* Size of blocks-to-lines buffer in bytes.
 * Note that this limits the maximum line length. */
#define NET_BBUFFER_LEN 16384
/* String to be appended to mooproxy messages */
#define MESSAGE_TERMINATOR "[0m"
/* The strftime() format for full date and time. */
#define FULL_TIME "%A %d %b %Y, %T"
/* The minimum number of seconds between two identical complaints about
 * the logfiles */
#define LOG_MSGINTERVAL 3600



#endif  /* ifndef MOOPROXY__HEADER__GLOBAL */
