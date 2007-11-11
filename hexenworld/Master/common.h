/*
	common.h
	misc utilities used in client and server

	$Id: common.h,v 1.5 2007-11-11 13:18:22 sezero Exp $
*/

#ifndef __HX2_COMMON_H
#define __HX2_COMMON_H

/* snprintf, vsnprintf : always use our versions. */
/* platform dependant (v)snprintf function names: */
#if defined(PLATFORM_WINDOWS)
#define	snprintf_func		_snprintf
#define	vsnprintf_func		_vsnprintf
#else
#define	snprintf_func		snprintf
#define	vsnprintf_func		vsnprintf
#endif

extern int q_snprintf (char *str, size_t size, const char *format, ...) __attribute__((format(printf,3,4)));
extern int q_vsnprintf(char *str, size_t size, const char *format, va_list args);


extern	char		com_token[1024];

const char *COM_Parse (const char *data);

extern	int		com_argc;
extern	char		**com_argv;

int COM_CheckParm (const char *parm);

#endif	/* __HX2_COMMON_H */

