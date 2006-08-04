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



#ifndef MOOPROXY__HEADER__PANIC
#define MOOPROXY__HEADER__PANIC



#define PANIC_SIGNAL 1
#define PANIC_MALLOC 2 
#define PANIC_REALLOC 3
#define PANIC_STRDUP 4
#define PANIC_VASPRINTF 5
#define PANIC_SELECT 6
#define PANIC_ACCEPT 7



/* Signal handler. Basically calls panic(). */
extern void panic_sighandler( int sig );

/* Panic. Try to write a helpful message to stderr, crash-file, and any
 * connected client. After that, terminate.
 * Reason holds the reason, (u)extra holds extra information specific to
 * the panic cause specified in reason. */
extern void panic( int reason, long extra, unsigned long uextra );



#endif  /* ifndef MOOPROXY__HEADER__PANIC */
