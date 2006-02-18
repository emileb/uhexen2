// quakedef.h -- primary header for client

#define	QUAKE_GAME			// as opposed to utilities

//define	PARANOID			// speed sapping error checking

/* We keep the userdir (and host_parms.userdir) as ~/.hexen2 here. In
   COM_InitFilesystem, we'll first change com_userdir to ~/.hexen2/hw,
   then to ~/.hexen2/<game> according to the -game cmdline arg. */
#ifndef DEMOBUILD
#define AOT_USERDIR ".hexen2"
#else
#define AOT_USERDIR ".hexen2demo"
#endif

#include <sys/types.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

#include "bothdefs.h"

#include "common.h"
#include "bspfile.h"
#include "vid.h"
#include "sys.h"
#include "zone.h"
#include "mathlib.h"
#include "wad.h"
#include "draw.h"
#include "cvar.h"
#include "screen.h"
#include "net.h"
#include "protocol.h"
#include "cmd.h"
#ifdef PLATFORM_UNIX
#define WITH_SDL	/* for the mouse2/3 hack in keys.c */
			/* also enables some SDL-only things */
#endif
#include "../Server/pr_comp.h"			// defs shared with qcc
#include "../Server/progdefs.h"			// generated by program cdefs
#include "strings.h"
#include "sbar.h"
#include "sound.h"
#include "render.h"
#include "cl_effect.h"
#include "client.h"

#ifdef GLQUAKE
#include "gl_model.h"
#else
#include "model.h"
#include "d_iface.h"
#endif

#include "input.h"
#include "keys.h"
#include "console.h"
#include "view.h"
#include "menu.h"
#include "crc.h"
#include "cdaudio.h"
#include "pmove.h"
#include "mididef.h"

#ifdef GLQUAKE
#include "glquake.h"
#endif

//=============================================================================

// the host system specifies the base of the directory tree, the
// command line parms passed to the program, and the amount of memory
// available for the program to use

typedef struct
{
	char	*basedir;
        char    *userdir;
	char	*cachedir;		// for development over ISDN lines
	int		argc;
	char	**argv;
	void	*membase;
	int		memsize;
} quakeparms_t;


//=============================================================================

#define MAX_NUM_ARGVS	50


extern qboolean noclip_anglehack;


//
// host
//
extern	quakeparms_t host_parms;

extern	cvar_t		sys_ticrate;
extern	cvar_t		sys_nostdout;
extern	cvar_t		developer;

extern	cvar_t	password;
extern	cvar_t  talksounds;

extern	qboolean	host_initialized;		// true if into command execution
extern	double		host_frametime;
extern	byte		*host_basepal;
extern	byte		*host_colormap;
extern	int			host_framecount;	// incremented every frame, never reset
extern	double		realtime;			// not bounded in any way, changed at
										// start of every frame, never reset

void Host_InitCommands (void);
void Host_Init (quakeparms_t *parms);
void Host_Shutdown(void);
void Host_Error (char *error, ...);
void Host_EndGame (char *message, ...);
void Host_Frame (float time);
void Host_Quit_f (void);
void Host_ClientCommands (char *fmt, ...);
void Host_ShutdownServer (qboolean crash);
void Host_WriteConfiguration (char *fname);

extern qboolean		msg_suppress_1;		// suppresses resolution and cache size console output
						//  an fullscreen DIB focus gain/loss

extern	unsigned int		defLosses;	// Defenders losses in Siege
extern	unsigned int		attLosses;	// Attackers Losses in Siege

extern int			cl_keyholder;
extern int			cl_doc;

