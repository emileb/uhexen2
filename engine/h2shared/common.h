/*
	common.h
	misc utilities used in client and server

	$Id$

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2008-2010  O.Sezer <sezero@users.sourceforge.net>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		51 Franklin Street, Fifth Floor,
		Boston, MA  02110-1301  USA
*/

#ifndef __HX2_COMMON_H
#define __HX2_COMMON_H


#undef	min
#undef	max
#define	q_min(a, b)	(((a) < (b)) ? (a) : (b))
#define	q_max(a, b)	(((a) > (b)) ? (a) : (b))

#if defined(PLATFORM_WINDOWS) && !defined(F_OK)
/* constants for access() mode argument. MS does not define them.
 * Note that X_OK (0x01) must not be used in windows code.  */
#define	R_OK	4		/* Test for read permission.  */
#define	W_OK	2		/* Test for write permission.  */
#define	X_OK	1		/* Test for execute permission.  */
#define	F_OK	0		/* Test for existence.  */
#endif

#if defined(PLATFORM_WINDOWS)
#define q_strncasecmp	_strnicmp
#define q_strcasecmp	_stricmp
#elif defined(PLATFORM_DOS)
#define q_strncasecmp	strnicmp
#define q_strcasecmp	stricmp
#else
#define q_strncasecmp	strncasecmp
#define q_strcasecmp	strcasecmp
#endif

#ifdef _MSC_VER	/* MS Visual C */
/* disable some silent conversion warnings */
#  pragma warning(disable:4244)
	/* 'argument'	: conversion from 'type1' to 'type2',
			  possible loss of data */
#  pragma warning(disable:4305)
	/* 'identifier'	: truncation from 'type1' to 'type2' */
	/*  in our case, truncation from 'double' to 'float' */
#  pragma warning(disable:4267)
	/* 'var'	: conversion from 'size_t' to 'type',
			  possible loss of data (/Wp64 warning) */
#endif	/* _MSC_VER */

/* strlcpy and strlcat : */
#include "strl_fn.h"

#if HAVE_STRLCAT && HAVE_STRLCPY
/* use native library functions */
#define q_strlcpy	strlcpy
#define q_strlcat	strlcat
#else
/* use our own copies of strlcpy and strlcat taken from OpenBSD */
extern size_t q_strlcpy (char *dst, const char *src, size_t size);
extern size_t q_strlcat (char *dst, const char *src, size_t size);
#endif

/* these qerr_ versions of functions error out if they detect, well, an error.
 * their first two arguments must the name of the caller function (see compiler.h
 * for the __thisfunc__ macro) and the line number, which should be __LINE__ .
 */
extern size_t qerr_strlcat (const char *caller, int linenum, char *dst, const char *src, size_t size);
extern size_t qerr_strlcpy (const char *caller, int linenum, char *dst, const char *src, size_t size);
extern int qerr_snprintf (const char *caller, int linenum, char *str, size_t size, const char *format, ...)
									__attribute__((__format__(__printf__,5,6)));

extern char *q_strlwr (char *str);
extern char *q_strupr (char *str);

/*============================================================================*/

extern	char		com_token[1024];
extern	qboolean	com_eof;

const char *COM_Parse (const char *data);

extern	int		safemode;
/* safe mode: if true, the engine will behave as if one of these
   arguments were actually on the command line:
   -nosound, -nocdaudio, -nomidi, -stdvid, -dibonly, -nomouse, -nojoy, -nolan
 */

void COM_Init (void);
int COM_CheckParm (const char *parm);

/* macros for compatibility with quake api */
#define	com_argc	host_parms->argc
#define	com_argv	host_parms->argv

void COM_ValidateByteorder (void);

const char *COM_SkipPath (const char *pathname);
void COM_StripExtension (const char *in, char *out, size_t outsize);
const char *COM_FileGetExtension (const char *in); /* doesn't return NULL */
void COM_ExtractExtension (const char *in, char *out, size_t outsize);
void COM_FileBase (const char *in, char *out, size_t outsize);
void COM_DefaultExtension (char *path, const char *extension, size_t len);

char	*va (const char *format, ...) __attribute__((__format__(__printf__,1,2)));
/* does a varargs printf into a temp buffer. cycles between
 * 4 different static buffers. the number of buffers cycled
 * is defined in VA_NUM_BUFFS. */

int COM_StrCompare (const void *arg1, const void *arg2);
/* quick'n'dirty string comparison function for use with qsort */


#endif	/* __HX2_COMMON_H */

