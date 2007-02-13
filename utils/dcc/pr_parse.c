/*
	parseerr.c
	parse error and parse warning routines for the progs compilation

	$Id: pr_parse.c,v 1.2 2007-02-13 13:34:43 sezero Exp $
*/


// HEADER FILES ------------------------------------------------------------

#include "util_inc.h"
#include "qcc.h"

// CODE --------------------------------------------------------------------

/*
==============
PR_ParseError

==============
*/
void PR_ParseError (const char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr, error);
	vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);

	printf ("%s(%i) : %s\n", strings+s_file, pr_source_line, string);

	longjmp (pr_parse_abort, 1);
}

