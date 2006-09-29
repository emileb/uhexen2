/*
	snd_sys.h
	Platform specific macros and prototypes for sound

	$Id: snd_sys.h,v 1.9 2006-09-29 23:08:06 sezero Exp $
*/

#ifndef __HX2_SND_SYS__
#define __HX2_SND_SYS__

#undef HAVE_SDL_SOUND
#undef HAVE_OSS_SOUND
#undef HAVE_SUN_SOUND
#undef HAVE_ALSA_SOUND
#undef HAVE_WIN32_SOUND

// add more systems with OSS here
#if defined(__linux__) || defined(__DragonFly__) || defined(__FreeBSD__)
#define HAVE_OSS_SOUND	1
#else
#define HAVE_OSS_SOUND	0
#endif

// add more systems with SUN audio here
#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(SUNOS)
#define HAVE_SUN_SOUND	1
#else
#define HAVE_SUN_SOUND	0
#endif

#if defined(NO_ALSA)
#define HAVE_ALSA_SOUND	0
#elif defined(__linux__)
// add more systems with ALSA here
#define HAVE_ALSA_SOUND	1
#else
#define HAVE_ALSA_SOUND	0
#endif

#if defined(PLATFORM_UNIX)
#define HAVE_SDL_SOUND	1
#else
#define HAVE_SDL_SOUND	0
#endif

#if defined(_WIN32)
#define HAVE_WIN32_SOUND	1
#else
#define HAVE_WIN32_SOUND	0
#endif

// Sound system definitions
#define	S_SYS_NULL	0
#define	S_SYS_OSS	1
#define	S_SYS_SDL	2
#define	S_SYS_ALSA	3
#define	S_SYS_SUN	4
#define	S_SYS_WIN32	5
#define	S_SYS_MAX	6

// this prevents running S_Update_() with the sdl sound driver
// if the snd_sdl implementation already calls S_PaintChannels.
#define SDLSOUND_PAINTS_CHANNELS	1

#if defined(_WIN32)
// for the windows crap used in snd_dma.c
#include "winquake.h"
#endif	//  _WIN32


extern unsigned int	snd_system;

#ifndef _SND_SYS_MACROS_ONLY

// chooses functions to call depending on audio subsystem
extern void S_InitDrivers(void);

// initializes driver and cycling through a DMA buffer
extern qboolean (*SNDDMA_Init)(void);

// gets the current DMA position
extern int (*SNDDMA_GetDMAPos)(void);

// shutdown the DMA xfer and driver
extern void (*SNDDMA_Shutdown)(void);

// sends sound to the device
extern void (*SNDDMA_Submit)(void);


#ifdef _SND_LIST_DRIVERS

#if HAVE_WIN32_SOUND
// WIN32 versions of the above
extern qboolean S_WIN32_Init(void);
extern int S_WIN32_GetDMAPos(void);
extern void S_WIN32_Shutdown(void);
extern void S_WIN32_Submit(void);
#endif

#if HAVE_OSS_SOUND
// OSS versions of the above
extern qboolean S_OSS_Init(void);
extern int S_OSS_GetDMAPos(void);
extern void S_OSS_Shutdown(void);
extern void S_OSS_Submit(void);
#endif	// HAVE_OSS_SOUND

#if HAVE_SUN_SOUND
// SUN Audio versions of the above
extern qboolean S_SUN_Init(void);
extern int S_SUN_GetDMAPos(void);
extern void S_SUN_Shutdown(void);
extern void S_SUN_Submit(void);
#endif	// HAVE_SUN_SOUND

#if HAVE_ALSA_SOUND
// ALSA versions of the above
extern qboolean S_ALSA_Init(void);
extern int S_ALSA_GetDMAPos(void);
extern void S_ALSA_Submit(void);
extern void S_ALSA_Shutdown(void);
#endif	// HAVE_ALSA_SOUND

#if HAVE_SDL_SOUND
// SDL versions of the above
extern qboolean S_SDL_Init(void);
extern int S_SDL_GetDMAPos(void);
extern void S_SDL_Shutdown(void);
extern void S_SDL_Submit(void);
#endif	// HAVE_SDL_SOUND

#endif	// _SND_LIST_DRIVERS

#endif	// !(_SND_SYS_MACROS_ONLY)

#endif	// __HX2_SND_SYS__

