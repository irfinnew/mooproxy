/*
 *
 *  mooproxy - a buffering proxy for MOO connections
 *  Copyright (C) 2001-2011 Marcel L. Moreaux <marcelm@qvdr.net>
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
#define PANIC_STRNDUP 5
#define PANIC_VASPRINTF 6
#define PANIC_SELECT 7
#define PANIC_ACCEPT 8



/* Signal handler. Basically calls panic(). */
extern void sighandler_panic( int sig );

/* Panic. Try to write a helpful message to stderr, crash-file, and any
 * connected client. After that, terminate.
 * Reason holds the reason, (u)extra holds extra information specific to
 * the panic cause specified in reason. */
extern void panic( int reason, long extra, unsigned long uextra );



#endif  /* ifndef MOOPROXY__HEADER__PANIC */
