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

/*
Todo:
 Check increasing precision in adjacent timespecs? (ie, barf on 01/02 01/02)

Keywords to add:
limit, count, context, before, after, timestamp, highlight, noansi


WHEN      -> ABSOLUTE [WHEN]
           | RELATIVE [WHEN]

ABSOLUTE  -> 'now'
           | 'today'
           | 'yesterday'
           | {last,next} 'mon' | 'monday' | 'tue' | 'tuesday' | ...
           | [YY/]MM/DD
           | HH:MM[:SS]

RELATIVE  -> '-/+' <number> MODIFIER

MODIFIER  -> 'lines'
           | 'seconds'
           | 'minutes'
           | 'hours'
           | 'days'
*/



#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>

#include "recall.h"
#include "world.h"
#include "misc.h"



typedef struct Params Params;
struct Params
{
	/* The argument string we're parsing. */
	char   *argstr;
	/* Error messages go here. */
	char   *error;

	/* A copy of the word in argstr we're examining now.
	 * Must be freed. */
	char   *word;
	/* Indexes to the current word in argstr. */
	long    word_start;
	long    word_end;

	/* Bitfield indicating which keywords we've seen so far. */
	long    keywords_seen;

	/* This time is manipulated by the time parsing functions. */
	time_t  when;

	/* The actual recall options. The parameters set (some of) these. */
	time_t  from;
	time_t  to;
	long    lines;
	char   *search_str;

	/* Statistics about the recalled lines. */
	long    lines_inperiod;
	long    lines_matched;
};



static int parse_arguments( World *wld, Params *params );
static int parse_keyword_index( char *keyword );
static void parse_nextword( Params *params );

static int parse_keyword_from( World *wld, Params *params );
static int parse_keyword_to( World *wld, Params *params );
static int parse_keyword_search( World *wld, Params *params );

static int parse_when( World *wld, Params *params, int lma );
static int parse_when_relative( World *wld, Params *params, int lma );
static int parse_when_absolute( World *wld, Params *params );
static int parse_when_absdate( World *wld, Params *params );
static int parse_when_abstime( World *wld, Params *params );
static int parse_when_rchk( Params *params, int v, int l, int u, char *name );

static void recall_search_and_recall( World *wld, Params *params );
static void recall_match_one_line( World *wld, Params *params, Line *line );
static int recall_match( char *line, char *re );


static const char *weekday[] =
{
	"sun", "sunday",
	"mon", "monday",
	"tue", "tuesday",
	"wed", "wednesday",
	"thu", "thursday",
	"fri", "friday",
	"sat", "saturday",
	NULL
};

static const struct
{
	char *keyword;
	int (*func)( World *, Params * );
} keyword_db[] = {
	{ "from", 	parse_keyword_from },
	{ "to",		parse_keyword_to },
	{ "search",	parse_keyword_search },

	{ NULL,		NULL }
};



extern void world_recall_command( World *wld, char *argstr )
{
	Params params;

	params.argstr = argstr;

	/* Default recall: from oldest line to now. */
	params.from = current_time();
	if( wld->history_lines->tail )
		params.from = wld->history_lines->head->time;
	params.to = current_time();
	params.lines = 0;
	params.search_str = NULL;

	/* Parse the command arguments. */
	if( parse_arguments( wld, &params ) )
		goto out;

	/* Make sure from < to. */
	if( params.from > params.to )
	{
		time_t t = params.from;
		params.from = params.to;
		params.to = t;
	}

	/* Print the recall header. */
	if( params.lines == 0 )
	{
		world_msg_client( wld, "Recalling from %s to %s.",
				time_string( params.from, "%a %Y/%m/%d %T" ),
				time_string( params.to, "%a %Y/%m/%d %T" ) );
	}
	else
	{
		world_msg_client( wld, "Recalling %li lines %s %s.",
				abs( params.lines ),
				( params.lines > 0 ) ? "after" : "before",
				( params.from == current_time() ) ? "now" :
				time_string( params.from, "%a %Y/%m/%d %T" ) );
	}

	/* Recall matching lines. */
	recall_search_and_recall( wld, &params );

	/* Print the recall footer. */
	world_msg_client( wld, "Recall end (%i / %i / %i).",
			wld->history_lines->count, params.lines_inperiod,\
			params.lines_matched );

out:
	free( params.word );
	free( params.error );
	free( params.search_str );
}



/* Loop over all argument words, trying to make sense of them.
 * All parameters encountered are altered in params.
 * On error, return true. On successful parse, return false. */
static int parse_arguments( World *wld, Params *params )
{
	int idx = 0;


	/* Initialization... */
	params->error = NULL;
	params->word = NULL;
	params->word_start = 0;
	params->word_end = 0;
	params->keywords_seen = 0;

	parse_nextword( params );
	while( params->word[0] != '\0' )
	{
		/* Ok, which keyword are we dealing with? */
		idx = parse_keyword_index( params->word );

		/* Invalid keyword? */
		if( idx == -1 )
		{
			world_msg_client( wld, "Unrecognized keyword `%s'.",
					params->word );
			return 1;
		}

		/* Keyword we've already seen? */
		if( params->keywords_seen & ( 1 << idx ) )
		{
			world_msg_client( wld, "Keyword `%s' may appear "
				"only once.", keyword_db[idx].keyword );
			return 1;
		}
		params->keywords_seen |= 1 << idx;

		/* Parse the keywords options, if any. Throw error if the 
		 * keyword option parse function complains. */
		if( (*keyword_db[idx].func)( wld, params ) )
		{
			world_msg_client( wld, "%s", params->error );
			return 1;
		}
	}

	/* The entire argument string has been parsed! */
	return 0;
}



/* Loop up a keyword, return its index or -1 if unknown.
 * Search is case insensitive. */
static int parse_keyword_index( char *keyword )
{
	int idx;

	for( idx = 0; keyword_db[idx].keyword; idx++ )
		if( !strcasecmp( keyword, keyword_db[idx].keyword ) )
			return idx;

	return -1;
}



/* Scan for the next word in the argument string, copy it into the word
 * variable, and update some pointers. */
static void parse_nextword( Params *params )
{
	int wlen = 0;
	char *str;

	free( params->word );
	params->word = NULL;

	/* Find the start of the next word. */
	while( isspace( params->argstr[params->word_end] ) )
		params->word_end++;

	/* Mark the start of the word. */
	str = params->argstr + params->word_end;
	params->word_start = params->word_end;

	/* Find the end of the word. */
	if( *str != '\0' )
		while( *str && !isspace( *str ) )
		{
			str++;
			wlen++;
		}

	/* Copy the word. */
	params->word_end = params->word_start + wlen;
	params->word = xmalloc( wlen + 1 );
	memcpy( params->word, params->argstr + params->word_start, wlen );
	params->word[wlen] = '\0';
}



/* Parse the options to the 'from' keyword. */
static int parse_keyword_from( World *wld, Params *params )
{
	int i;

	/* From may not come after to! */
	if( params->keywords_seen & ( 1 << parse_keyword_index( "to" ) ) )
	{
		xasprintf( &params->error, "The `from' keyword may not "
				"appear after the `to' keyword." );
		return 1;
	}

	parse_nextword( params );
	/* When defaults to now, so that relative times are relative to now
	 * instead of to the oldest line. */
	params->when = current_time();

	/* We're really expecting a timespec now. */
	if( params->word[0] == '\0' )
	{
		xasprintf( &params->error, "Missing timespec after `from' "
				"keyword." );
		return 1;
	}

	/* Scan next words for timespecs. */
	for( i = 0; ; i++ )
	{
		if( parse_when( wld, params, 0 ) )
		{
			/* Parsing failed. If it's the first option, it's an
			 * invalid timespec. If it's not the first option, we
			 * claim success and leave the unparseable word to be
			 * parsed higher layers. */

			/* If it's the first option, it's invalid. Create
			 * an error message if there isn't one already. */
			if( i == 0 && !params->error )
				xasprintf( &params->error, "Invalid timespec: "
						"%s.", params->word );

			/* Only fail if we actually have an error. */
			return params->error != NULL;
		}

		params->from = params->when;
	}
}



/* Parse the options to the 'to' keyword. */
static int parse_keyword_to( World *wld, Params *params )
{
	int i;

	/* If from has been used, default when to from, so that relative times
	 * are relative to the previously specified from. */
	if( params->keywords_seen & ( 1 << parse_keyword_index( "from" ) ) )
		params->when = params->from;
	else
		params->when = current_time();

	parse_nextword( params );

	/* We're really expecting a timespec now. */
	if( params->word[0] == '\0' )
	{
		xasprintf( &params->error, "Missing timespec after `to' "
				"keyword." );
		return 1;
	}

	/* Scan next words for timespecs. */
	for( i = 0; ; i++ )
	{
		/* lma = i == 0; 'lines' is only allowed if it's the first
		 * option to this keyword. */
		if( parse_when( wld, params, i == 0 ) )
		{
			/* Parsing failed. If it's the first option, it's an
			 * invalid timespec. If it's not the first option, we
			 * claim success and leave the unparseable word to be
			 * parsed higher layers. */

			/* If it's the first option, it's invalid. Create
			 * an error message if there isn't one already. */
			if( i == 0 && !params->error )
				xasprintf( &params->error, "Invalid timespec: "
						"%s.", params->word );

			/* Only fail if we actually have an error. */
			return params->error != NULL;
		}

		/* If the lines parameter has been set, stop parsing. */
		if( params->lines != 0 )
			return 0;

		params->to = params->when;
	}
}



/* Parse the options to the 'search' keyword. */
static int parse_keyword_search( World *wld, Params *params )
{
	parse_nextword( params );
	if( params->word[0] == '\0' )
	{
		xasprintf( &params->error, "Missing search string after "
				"`search' keyword." );
		return 1;
	}

	/* The entire rest of the argument string becomes the search string. */
	params->search_str = xstrdup( params->argstr + params->word_start );

	/* Eat any remaining words. */
	while( params->word[0] != '\0' )
		parse_nextword( params );
	return 0;
}



/* Parse a timespec. Returns true on error, false on success.
 * On error, params->error may or may not be set.
 * On success, params->when or params->lines will be modified. */
static int parse_when( World *wld, Params *params, int lma )
{
	if( params->word[0] == '-' || params->word[0] == '+' )
		return parse_when_relative( wld, params, lma );
	else
		return parse_when_absolute( wld, params );
} 



/* Parse a relative timespec. Returns true on error, false on success.
 * On error, params->error may or may not be set.
 * On success, params->when or params->lines will be modified.
 * lma idicates whether or not the 'lines' modifier is allowed now. */
static int parse_when_relative( World *wld, Params *params, int lma )
{
	char *str = params->word;
	long n = 0;
	int dir = 0;

	/* A relative timespec should start with - or +. */
	if( *str != '-' && *str != '+' )
		return 1;

	/* Get the direction, as determined by the sign. */
	dir = ( *str == '+' ) ? 1 : -1;

	/* Eliminate the sign character. */
	str++;

	/* If there's nothing left, continue parsing at the next word. */
	if( *str == '\0' )
	{
		parse_nextword( params );
		str = params->word;
	}

	/* Need a digit now. */
	if( !isdigit( *str ) )
	{
		xasprintf( &params->error, "Invalid relative timespec: %s.",
				params->word );
		return 1;
	}

	/* Construct the number. */
	while( isdigit( *str ) )
		n = n * 10 + ( *str++ - '0' );

	/* Zero not allowed. */
	if( n == 0 )
	{
		xasprintf( &params->error, "Number should be non-zero: %s.",
				params->word );
		return 1;
	}

	/* If there's nothing left, continue parsing at the next word. */
	if( *str == '\0' )
	{
		parse_nextword( params );
		str = params->word;
	}

	/* We do want a modifier now. */
	if( *str == '\0' )
	{
		xasprintf( &params->error, "Missing modifier to relative "
				"timespec." );
		return 1;
	}

	/* FIXME: go through localtime() / mktime() ?
	 * This might be better (or worse) for daylight saving. */

	/* Parse the modifier word, and adjust when/lines accordingly. */
	if( !strncasecmp( str, "seconds", strlen( str ) ) )
		params->when += dir * n;
	else if( !strncasecmp( str, "secs", strlen( str ) ) )
		params->when += dir * n;
	else if( !strncasecmp( str, "minutes", strlen( str ) ) )
		params->when += dir * n * 60;
	else if( !strncasecmp( str, "mins", strlen( str ) ) )
		params->when += dir * n * 60;
	else if( !strncasecmp( str, "hours", strlen( str ) ) )
		params->when += dir * n * 60 * 60;
	else if( !strncasecmp( str, "hrs", strlen( str ) ) )
		params->when += dir * n * 60 * 60;
	else if( !strncasecmp( str, "days", strlen( str ) ) )
		params->when += dir * n * 60 * 60 * 24;
	else if( !strncasecmp( str, "lines", strlen( str ) ) )
		params->lines = dir * n;
	else
	{
		xasprintf( &params->error, "Invalid modifier to relative "
				"timespec: %s.", str );
		return 1;
	}

	/* Was lines set while this was not allowed? */
	if( params->lines != 0 && !lma )
	{
		xasprintf( &params->error, "The `lines' modifier may "
				"only be used alone with the `to' keyword." );
		return 1;
	}

	parse_nextword( params );
	return 0;
}



/* Parse an absolute timespec. Returns true on error, false on success.
 * On error, params->error may or may not be set.
 * On success, params->when or params->lines will be modified. */
static int parse_when_absolute( World *wld, Params *params )
{
	struct tm *tm;
	time_t t;
	int i, prevnext = 0;

	/* Now. */
	if( !strcasecmp( params->word, "now" ) )
	{
		params->when = current_time();
		parse_nextword( params );
		return 0;
	}

	/* Today. */
	if( !strcasecmp( params->word, "today" ) )
	{
		t = current_time();
		tm = localtime( &t );
		tm->tm_sec = 0;
		tm->tm_min = 0;
		tm->tm_hour = 0;
		params->when = mktime( tm );
		parse_nextword( params );
		return 0;
	}

	/* Yesterday. */
	if( !strcasecmp( params->word, "yesterday" ) )
	{
		t = current_time();
		tm = localtime( &t );
		tm->tm_sec = 0;
		tm->tm_min = 0;
		tm->tm_hour = 0;
		tm->tm_mday -= 1;
		params->when = mktime( tm );
		parse_nextword( params );
		return 0;
	}

	/* If we encounter 'next' or 'prev', note that in prevnext.
	 * We'll use it in parsing weekdays.
	 * 1 means next, -1 means last. */
	if( !strcasecmp( params->word, "next" ) )
		prevnext = 1;
	if( !strcasecmp( params->word, "last" ) )
		prevnext = -1;

	if( prevnext != 0 )
	{
		parse_nextword( params );

		/* Try if we got a weekday next. */
		for( i = 0; weekday[i]; i++ )
		{
			if( strcasecmp( params->word, weekday[i] ) )
				continue;

			tm = localtime( &params->when );
			tm->tm_sec = 0;
			tm->tm_min = 0;
			tm->tm_hour = 0;

			/* Seek forward if we encountered a 'last'.
			 * Otherwise, seek backward. */
			if( prevnext == 1 )
				tm->tm_mday += ( i / 2 - tm->tm_wday + 6 ) % 7
						+ 1;
			else
				tm->tm_mday -= ( tm->tm_wday - i / 2 + 6 ) % 7
						+ 1;

			params->when = mktime( tm );
			parse_nextword( params );
			return 0;
		}

		/* If we got here, we didn't get a valid weekday. */
		xasprintf( &params->error, "Expecting a week day after `%s'.",
				prevnext == -1 ? "last" : "next" );
		return 1;
	}

	/* Try parsing for [YY/]MM/DD. */
	if( !parse_when_absdate( wld, params ) )
		return 0;

	/* Try parsing for HH:MM[:SS]. */
	if( !parse_when_abstime( wld, params ) )
		return 0;

	return 1;
}



/* Parse an absolute date, [YY/]MM/DD.
 * Return true on error, false on success.
 * On success, params->when is modified. */
static int parse_when_absdate( World *wld, Params *params )
{
	struct tm *tm;
	int r, l, n1 = 0, n2 = 0, n3 = 0;

	/* Break down the current when. */
	tm = localtime( &params->when );

	/* Scan the timespec string. */
	r = sscanf( params->word, "%d/%d%n/%d%n", &n1, &n2, &l, &n3, &l );

	/* If sscanf() didn't read 2 or 3 variables, or didn't consume the
	 * entire string, something was wrong. */
	if( ( r != 2 && r != 3 ) || l != strlen( params->word ) )
		return 1;

	/* Is it MM/DD? */
	if( r == 2 )
	{
		tm->tm_mday = n2;
		tm->tm_mon = n1 - 1;
	}
	/* Or YY/MM/DD? */
	if( r == 3 )
	{
		tm->tm_mday = n3;
		tm->tm_mon = n2 - 1;
		/* Years are interpreted as a 0-99 number meaning a year
		 * in the 1970 - 2069 range. */
		tm->tm_year = ( n1 + 30 ) % 100 + 70;
	}

	/* Months are 1-12, days are 1-31. */
	if( parse_when_rchk( params, tm->tm_mon + 1, 1, 12, "Months" ) )
		return 1;
	if( parse_when_rchk( params, tm->tm_mday, 1, 31, "Days" ) )
		return 1;

	/* Set the time to 00:00:00, and flag unknown daylight saving time. */
	tm->tm_sec = 0;
	tm->tm_min = 0;
	tm->tm_hour = 0;
	tm->tm_isdst = -1;
	params->when = mktime( tm );
	parse_nextword( params );
	return 0;
}



/* Parse an absolute time, HH:MM[:SS].
 * Return true on error, false on success.
 * On success, params->when is modified. */
static int parse_when_abstime( World *wld, Params *params )
{
	struct tm *tm;
	int r, l, n1 = 0, n2 = 0, n3 = 0;

	/* Break down the current when. */
	tm = localtime( &params->when );

	/* Scan the timespec string. */
	r = sscanf( params->word, "%d:%d%n:%d%n", &n1, &n2, &l, &n3, &l );

	/* If sscanf() didn't read 2 or 3 variables, or didn't consume the
	 * entire string, something was wrong. */
	if( ( r != 2 && r != 3 ) || l != strlen( params->word ) )
		return 1;

	tm->tm_hour = n1;
	tm->tm_min = n2;
	/* If HH:MM:SS, set the seconds. Otherwise, set seconds to 00. */
	if( r == 3 )
		tm->tm_sec = n3;
	else
		tm->tm_sec = 0;

	/* Hours are 00-23, minutes and seconds are 0-59. */
	if( parse_when_rchk( params, tm->tm_hour, 0, 23, "Hours" ) )
		return 1;
	if( parse_when_rchk( params, tm->tm_min, 0, 59, "Minutes" ) )
		return 1;
	if( parse_when_rchk( params, tm->tm_sec, 0, 59, "Seconds" ) )
		return 1;

	tm->tm_isdst = -1;
	params->when = mktime( tm );
	parse_nextword( params );
	return 0;
}



/* Check if l <= v <= u.
 * If the check succeeds, return false.
 * If the check fails, return true and set params->error. */
static int parse_when_rchk( Params *params, int v, int l, int u, char *name )
{
	if( v >= l && v <= u )
		return 0;

	xasprintf( &params->error, "%s should be in the range %i to %i.",
			name, l, u );
	return 1;
}



/* Search the history for any lines that match our search criteria,
 * and recall those lines. */
static void recall_search_and_recall( World *wld, Params *params )
{
	Line *line;
	char *str;
	int count = 0, lines = 0;

	/* Initialize statistics. */
	params->lines_inperiod = 0;
	params->lines_matched = 0;

	/* Transform the search-string, in-place, into a \0 separated list
	 * of individually searchable strings so our "RE engine" can use it.
	 * Yes, this is utterly horrible. */
	if( params->search_str != NULL )
	{
		params->search_str = xrealloc( params->search_str,
				strlen( params->search_str ) + 2 );
		str = params->search_str;
		while( *str != '\0' )
			*str = tolower( *str ), str++;
		str = params->search_str;
		while( *str != '\0' )
		{
			if( *str++ != '.' )
				continue;
			if( *str != '*' )
				continue;
			*(str - 1) = '\0';
			memmove( str, str + 1, strlen( str ) );
		}
		str++;
		*str = '\0';
	}

	/* Search all lines that satisfy from <= time <= to. */
	if( params->lines == 0 )
	{
		/* Just loop from oldest line to newest, and only do further
		 * matching on those that satisfy the time requirements. */
		for( line = wld->history_lines->head; line; line = line->next )
		{
			if( line->time < params->from )
				continue;
			if( line->time > params->to )
				continue;

			recall_match_one_line( wld, params, line );
		}
	}

	/* Search params->lines after from. */
	if( params->lines > 0 )
	{
		/* Just loop from oldest line to newest, and only do further
		 * matching on the first X lines that are new enough. */
		for( line = wld->history_lines->head; line; line = line->next )
		{
			if( line->time < params->from )
				continue;
			if( ++count > params->lines )
				break;

			recall_match_one_line( wld, params, line );
		}
	}

	/* Search -params->lines before from. */
	if( params->lines < 0 )
	{
		/* First, loop from newest line to oldest, stopping when we've
		 * encountered X lines that are old enough. */
		for( line = wld->history_lines->tail; line; line = line->prev )
		{
			 if( line->time > params->from )
				 continue;
			 if( ++lines >= -params->lines )
				 break;
		}

		/* Don't run off the head of the queue. */
		if( !line )
			line = wld->history_lines->head;

		/* Now, loop from the last line found back forward in time,
		 * inspecting X lines. */
		for( ; line; line = line->next )
		{
			if( line->time > params->from )
				continue;
			if( ++count > lines )
				break;

			recall_match_one_line( wld, params, line );
		}
	}
}



/* Inspect a line that already matches the time criteria further.
 * If it matches the string criteria as well, recall it. */
static void recall_match_one_line( World *wld, Params *params, Line *line )
{
	Line *recalled;
	char *str;

	/* It got here, so it matched the time criteria. */
	params->lines_inperiod++;

	/* Get the string without ANSI stuff. */
	str = xmalloc( line->len + 1 );
	strcpy_noansi( str, line->str );

	/* If we have a search string, and it doesn't match, dump the line. */
	if( params->search_str != NULL &&
			!recall_match( str, params->search_str ) )
	{
		free( str );
		return;
	}

	/* We're good, recall it! */
	recalled = line_create( str, -1 );
	recalled->flags = LINE_MESSAGE;
	recalled->time = line->time;
	linequeue_append( wld->client_toqueue, recalled );
	params->lines_matched++;
}



/* Match the (prepared) re against the string.
 * Return true on successful match, false otherwise. */
static int recall_match( char *line, char *re )
{
	char *l, *r;

	/* A continuation of the horrible re stuff.
	 * Please just pretend it doesn't exist. */
	while( *re != '\0' && *line != '\0' )
	{
		l = line;
		r = re;

		while( ( *r == '.' || tolower( *l ) == *r ) && *l != '\0' )
			l++, r++;

		if( *r != '\0' )
			line++;
		else
		{
			re = r + 1;
			line = l;
		}
	}

	return *re == '\0';
}
