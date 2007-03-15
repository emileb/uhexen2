/*
	quakeinc.h
	primary header for client and server

	FIXME:	kill this in the future and make each C
		file include only the necessary headers.

	$Id: quakeinc.h,v 1.13 2007-03-15 13:36:47 sezero Exp $
*/

#ifndef __QUAKEINC_H
#define __QUAKEINC_H

/* make sure to include our compile time options first	*/
#include "h2option.h"

/* include the system headers: make sure to use q_types.h
   instead of <sys/types.h>				*/
#include "q_types.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#if !defined(_WIN32)
#include <strings.h>	/* strcasecmp and strncasecmp	*/
#endif	/* !_WIN32 */
#include <stdarg.h>
#include <stdlib.h>
#include <setjmp.h>

/* include the quake headers				*/

#include "q_endian.h"
#include "link_ops.h"
#include "sizebuf.h"
#include "msg_io.h"
#include "printsys.h"
#include "common.h"
#include "quakeio.h"
#include "quakefs.h"
#include "bspfile.h"
#include "sys.h"
#include "zone.h"
#include "mathlib.h"
#include "cvar.h"

#include "protocol.h"
#include "net.h"

#include "cmd.h"
#include "crc.h"

#include "host.h"
#include "entstate.h"

#if !defined(SERVERONLY)
#include "console.h"
#include "vid.h"
#include "wad.h"
#include "draw.h"
#include "render.h"
#include "view.h"
#include "screen.h"
#include "sbar.h"
#include "sound.h"
#include "cdaudio.h"
#include "mididef.h"
#endif	/* !SERVERONLY */

#include "progs.h"
#include "pr_strng.h"
#include "cl_effect.h"

#if defined(SERVERONLY)
#include "h2ded.h"
#else
#include "client.h"
#endif	/* SERVERONLY */
#include "server.h"

#if defined(GLQUAKE)
#include "glheader.h"
#include "gl_model.h"
#include "glquake.h"
#elif defined(SERVERONLY)
#include "server/model.h"
#else
#include "model.h"
#include "d_iface.h"
#endif

#include "world.h"

#if !defined(SERVERONLY)
#include "input.h"
#include "keys.h"
#include "menu.h"
#endif	/* !SERVERONLY */

#endif	/* __QUAKEINC_H */

