/*
 *
 *  mooproxy - a buffering proxy for moo-connections
 *  Copyright (C) 2001-2007 Marcel L. Moreaux <marcelm@luon.net>
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
#define VERSIONSTR "0.1.4-pre1"

/* Configuration dirnames */
#define CONFIGDIR ".mooproxy"
#define WORLDSDIR "worlds"
#define LOGSDIR "logs"
#define LOCKSDIR "locks"

/* Some default option values */
#define DEFAULT_AUTOLOGIN 0
#define DEFAULT_AUTORECONNECT 0
#define DEFAULT_CMDSTRING "/"
#define DEFAULT_INFOSTRING "%c%% "
#define DEFAULT_NEWINFOSTRING ""
#define DEFAULT_LOGENABLE 1
#define DEFAULT_CONTEXTLINES 100
#define DEFAULT_MAXBUFFERSIZE 4096
#define DEFAULT_MAXLOGBUFFERSIZE 4096
#define DEFAULT_STRICTCMDS 1
#define DEFAULT_LOGTIMESTAMPS 1

/* Maximum number of authenticating connections */
#define NET_MAXAUTHCONN 8
/* Maximum number of characters accepted from an authenticating client.
 * This effectively also limits the authentication string length */
#define NET_MAXAUTHLEN 128
/* Size of blocks-to-lines buffer in bytes.
 * Note that this limits the maximum line length. */
#define NET_BBUFFER_LEN 65536

/* The minimum number of seconds between two identical complaints about
 * the logfiles */
#define LOG_MSGINTERVAL 600
/* The minimum number of seconds between two "not connected" messages.*/
#define NOTCONN_MSGINTERVAL 3

/* String to be prepended to mooproxy messages */
#define MESSAGE_HEADER "[0m"
/* String to be appended to mooproxy messages */
#define MESSAGE_TERMINATOR "[0m"
/* The strftime() format for full date and time. */
#define FULL_TIME "%A %d %b %Y, %T"
/* The strftime() format, and string length of the log timestamp. */
#define LOG_TIMESTAMP_FORMAT "[%H:%M:%S] "
#define LOG_TIMESTAMP_LENGTH 11

/* When malloc() fails, mooproxy will sleep for a bit and then try again.
 * This setting determines how often mooproxy will try before giving up. */
#define XMALLOC_OOM_RETRIES 4
/* The name of the panic file, which will be placed in ~. */
#define PANIC_FILE "mooproxy.crashed"
/* The maximum allowed length of the config file, in KB. */
#define CONFIG_MAXLENGTH 128UL



#endif  /* ifndef MOOPROXY__HEADER__GLOBAL */
