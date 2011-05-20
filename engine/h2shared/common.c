/*
	common.c
	misc utility functions used in client and server

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

#include "quakedef.h"
#include <ctype.h>


int		safemode;


/*
============================================================================

REPLACEMENT FUNCTIONS

============================================================================
*/

char *q_strlwr (char *str)
{
	char	*c;
	c = str;
	while (*c)
	{
		*c = tolower(*c);
		c++;
	}
	return str;
}

char *q_strupr (char *str)
{
	char	*c;
	c = str;
	while (*c)
	{
		*c = toupper(*c);
		c++;
	}
	return str;
}


int q_vsnprintf(char *str, size_t size, const char *format, va_list args)
{
	int		ret;

	ret = vsnprintf_func (str, size, format, args);

	if (ret < 0)
		ret = (int)size;

	if ((size_t)ret >= size)
		str[size - 1] = '\0';

	return ret;
}

int q_snprintf (char *str, size_t size, const char *format, ...)
{
	int		ret;
	va_list		argptr;

	va_start (argptr, format);
	ret = q_vsnprintf (str, size, format, argptr);
	va_end (argptr);

	return ret;
}

size_t qerr_strlcat (const char *caller, int linenum,
		     char *dst, const char *src, size_t size)
{
	size_t	ret = q_strlcat (dst, src, size);
	if (ret >= size)
		Sys_Error("%s: %d: string buffer overflow!", caller, linenum);
	return ret;
}

size_t qerr_strlcpy (const char *caller, int linenum,
		     char *dst, const char *src, size_t size)
{
	size_t	ret = q_strlcpy (dst, src, size);
	if (ret >= size)
		Sys_Error("%s: %d: string buffer overflow!", caller, linenum);
	return ret;
}

int qerr_snprintf (const char *caller, int linenum,
		   char *str, size_t size, const char *format, ...)
{
	int		ret;
	va_list		argptr;

	va_start (argptr, format);
	ret = q_vsnprintf (str, size, format, argptr);
	va_end (argptr);

	if ((size_t)ret >= size)
		Sys_Error("%s: %d: string buffer overflow!", caller, linenum);
	return ret;
}


/*
============================================================================

MISC UTILITY FUNCTIONS

============================================================================
*/

/*
============
va

does a varargs printf into a temp buffer. cycles between
4 different static buffers. the number of buffers cycled
is defined in VA_NUM_BUFFS.
============
*/
#define	VA_NUM_BUFFS	4
#define	VA_BUFFERLEN	1024

static char *get_va_buffer(void)
{
	static char va_buffers[VA_NUM_BUFFS][VA_BUFFERLEN];
	static int buffer_idx = 0;
	buffer_idx = (buffer_idx + 1) & (VA_NUM_BUFFS - 1);
	return va_buffers[buffer_idx];
}

char *va (const char *format, ...)
{
	va_list		argptr;
	char		*va_buf;

	va_buf = get_va_buffer ();
	va_start (argptr, format);
	if (q_vsnprintf(va_buf, VA_BUFFERLEN, format, argptr) >= VA_BUFFERLEN)
		Con_DPrintf("%s: overflow (string truncated)\n", __thisfunc__);
	va_end (argptr);

	return va_buf;
}


/*============================================================================
  quick'n'dirty string comparison function for use with qsort
  ============================================================================*/

int COM_StrCompare (const void *arg1, const void *arg2)
{
	return q_strcasecmp ( *(char **) arg1, *(char **) arg2);
}


/*============================================================================
  FileName Processing utilities
  ============================================================================*/

/*
============
COM_SkipPath
============
*/
const char *COM_SkipPath (const char *pathname)
{
	const char	*last;

	last = pathname;
	while (*pathname)
	{
		if (*pathname == '/')
			last = pathname + 1;
		pathname++;
	}
	return last;
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension (const char *in, char *out, size_t outsize)
{
	int		length;

	if (in != out)	/* copy when not in-place editing */
		q_strlcpy (out, in, outsize);
	length = (int)strlen(out) - 1;
	while (length > 0 && out[length] != '.')
	{
		--length;
		if (out[length] == '/' || out[length] == '\\')
			return;	/* no extension */
	}
	if (length > 0)
		out[length] = '\0';
}

/*
============
COM_FileGetExtension - doesn't return NULL
============
*/
const char *COM_FileGetExtension (const char *in)
{
	const char	*src;
	size_t		len;

	len = strlen(in);
	if (len < 2)	/* nothing meaningful */
		return "";

	src = in + len - 1;
	while (src != in && src[-1] != '.')
		src--;
	if (src == in || strchr(src, '/') != NULL || strchr(src, '\\') != NULL)
		return "";	/* no extension, at not least in the file itself. */

	return src;
}

/*
============
COM_ExtractExtension
============
*/
void COM_ExtractExtension (const char *in, char *out, size_t outsize)
{
	const char	*src;
	size_t		len;

	len = strlen(in);
	if (len < 2)	/* nothing meaningful */
	{
		*out = '\0';
		return;
	}

	src = in + len - 1;
	while (src != in && src[-1] != '.')
		src--;
	if (src == in || strchr(src, '/') != NULL || strchr(src, '\\') != NULL)
	{			/* no extension, at not least in the file itself. */
		*out = '\0';
		return;
	}

	q_strlcpy (out, src, outsize);
}

/*
============
COM_FileBase
take 'somedir/otherdir/filename.ext',
write only 'filename' to the output
============
*/
void COM_FileBase (const char *in, char *out, size_t outsize)
{
	const char	*dot, *slash, *s;

	s = in;
	slash = in;
	dot = NULL;
	while (*s)
	{
		if (*s == '/')
			slash = s + 1;
		if (*s == '.')
			dot = s;
		s++;
	}
	if (dot == NULL)
		dot = s;

	if (dot - slash < 2)
		q_strlcpy (out, "?model?", outsize);
	else
	{
		size_t	len = dot - slash;
		if (len >= outsize)
			len = outsize - 1;
		memcpy (out, slash, len);
		out[len] = '\0';
	}
}

/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension (char *path, const char *extension, size_t len)
{
	char	*src;
//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	if (!*path)
		return;
	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return;		// it has an extension
		src--;
	}

	qerr_strlcat(__thisfunc__, __LINE__, path, extension, len);
}


/*
============================================================================

STRING PARSING FUNCTIONS

============================================================================
*/

char		com_token[1024];

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
const char *COM_Parse (const char *data)
{
	int		c;
	int		len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;			// end of file;
		data++;
	}

// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c == '\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
/*	if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':')
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}
*/
// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;

//		if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':')
//			break;
	} while (c > 32);

	com_token[len] = 0;
	return data;
}


/*
============================================================================

COMMAND LINE PROCESSING FUNCTIONS

============================================================================
*/

/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int COM_CheckParm (const char *parm)
{
	int		i;

	for (i = 1; i < com_argc; i++)
	{
		if (!com_argv[i])
			continue;		// NEXTSTEP sometimes clears appkit vars.
		if (!strcmp (parm,com_argv[i]))
			return i;
	}

	return 0;
}

static void COM_Cmdline_f (void)
{
	int			i;

	Con_Printf ("cmdline was:");
	for (i = 0; (i < MAX_NUM_ARGVS) && (i < com_argc); i++)
	{
		if (com_argv[i])
			Con_Printf (" %s", com_argv[i]);
	}
	Con_Printf ("\n");
}

/*
============================================================================

INIT, ETC..

============================================================================
*/

void COM_ValidateByteorder (void)
{
	const char	*endianism[] = { "BE", "LE", "PDP", "Unknown" };
	const char	*tmp;

	ByteOrder_Init ();
	switch (host_byteorder)
	{
	case BIG_ENDIAN:
		tmp = endianism[0];
		break;
	case LITTLE_ENDIAN:
		tmp = endianism[1];
		break;
	case PDP_ENDIAN:
		tmp = endianism[2];
		host_byteorder = -1;	/* not supported */
		break;
	default:
		tmp = endianism[3];
		break;
	}
	if (host_byteorder < 0)
		Sys_Error ("%s: Unsupported byte order [%s]", __thisfunc__, tmp);
	Sys_Printf("Detected byte order: %s\n", tmp);
#if !ENDIAN_RUNTIME_DETECT
	if (host_byteorder != BYTE_ORDER)
	{
		const char	*tmp2;

		switch (BYTE_ORDER)
		{
		case BIG_ENDIAN:
			tmp2 = endianism[0];
			break;
		case LITTLE_ENDIAN:
			tmp2 = endianism[1];
			break;
		case PDP_ENDIAN:
			tmp2 = endianism[2];
			break;
		default:
			tmp2 = endianism[3];
			break;
		}
		Sys_Error ("Detected byte order %s doesn't match compiled %s order!", tmp, tmp2);
	}
#endif	/* ENDIAN_RUNTIME_DETECT */
}

/*
================
COM_Init
================
*/
void COM_Init (void)
{
	Cmd_AddCommand ("cmdline", COM_Cmdline_f);

	safemode = COM_CheckParm ("-safe");
}

