/*
	printsys.h
	console printing

	$Id: printsys.h,v 1.3 2007-04-19 14:06:57 sezero Exp $
*/

#ifndef __PRINTSYS_H
#define __PRINTSYS_H

/*
 * CON_Printf: Prints to the in-game console and manages other jobs, such
 * as echoing to the terminal and log.  In case of HexenWorld this may be
 * redirected. In that case, the message is printed to the relevant client.
 *
 * Location:	Graphical client : console.c
 *		HexenWorld server: sv_send.c
 *	Hexen II dedicated server: host.c
 */
void CON_Printf (unsigned int flags, const char *fmt, ...) __attribute__((format(printf,2,3)));

/* common print flags */
#define	_PRINT_NORMAL			0	/* print to both terminal and to the in-game console */
#define	_PRINT_TERMONLY			1	/* print to the terminal only: formerly Sys_Printf */
#define	_PRINT_DEVEL			2	/* print only if the developer cvar is set */
#define	_PRINT_SAFE			4	/* okay to call even when the screen can't be updated */

/* macros for compatibility with quake api */
#define Con_Printf(fmt, args...)	CON_Printf(_PRINT_NORMAL, fmt, ##args)
#define Con_DPrintf(fmt, args...)	CON_Printf(_PRINT_DEVEL, fmt, ##args)
#define Con_SafePrintf(fmt, args...)	CON_Printf(_PRINT_SAFE, fmt, ##args)

/* these macros print to the terminal only */
#define Sys_Printf(fmt, args...)	CON_Printf(_PRINT_TERMONLY, fmt, ##args)
#define Sys_DPrintf(fmt, args...)	CON_Printf(_PRINT_TERMONLY|_PRINT_DEVEL, fmt, ##args)

#ifdef DEBUG_BUILD
#define DEBUG_Printf Sys_DPrintf
#else
#define DEBUG_Printf(fmt, args...)
#endif

#endif	/* __PRINTSYS_H */

