/*
	vid_sdl.c
	SDL video driver
	Select window size and mode and init SDL in SOFTWARE mode.

	$Id: vid_sdl.c,v 1.71 2007-03-16 22:44:44 sezero Exp $

	Changed by S.A. 7/11/04, 27/12/04
	Options are now: -fullscreen | -window, -height , -width
	Currently bpp is 8 bit , and it seems fairly hardwired at this depth

	Changed by O.S 7/01/06
	- Added video modes enumeration via SDL
	- Added video mode changing on the fly.
*/

#include "quakedef.h"
#include "d_local.h"
#include "cfgfile.h"
#include "sdl_inc.h"

#define MIN_WIDTH		320
//#define MIN_HEIGHT		200
#define MIN_HEIGHT		240
#define MAX_DESC		13

qboolean	msg_suppress_1 = false;

unsigned char	vid_curpal[256*3];
unsigned short	d_8to16table[256];
unsigned	d_8to24table[256];

byte globalcolormap[VID_GRADES*256], lastglobalcolor = 0;
byte *lastsourcecolormap = NULL;

//intermission screen cache reference (to flush on video mode switch)
extern cache_user_t	*intermissionScreen;

static qboolean	vid_initialized = false;
static int	lockcount;
qboolean	in_mode_set;
static int	enable_mouse;
static qboolean	palette_changed;

static int	num_fmodes;
static int	num_wmodes;
static int	*nummodes;
//static int	bpp = 8;
static const SDL_VideoInfo	*vid_info;
static SDL_Surface	*screen;
static qboolean	vid_menu_fs;
static qboolean	fs_toggle_works = true;

viddef_t	vid;		// global video state
// cvar vid_mode must be set before calling VID_SetMode, VID_ChangeVideoMode or VID_Restart_f
static cvar_t	vid_mode = {"vid_mode", "0", CVAR_NONE};
static cvar_t	vid_config_glx = {"vid_config_glx", "640", CVAR_ARCHIVE};
static cvar_t	vid_config_gly = {"vid_config_gly", "480", CVAR_ARCHIVE};
static cvar_t	vid_config_swx = {"vid_config_swx", "320", CVAR_ARCHIVE};
static cvar_t	vid_config_swy = {"vid_config_swy", "240", CVAR_ARCHIVE};
static cvar_t	vid_config_fscr= {"vid_config_fscr", "0", CVAR_ARCHIVE};

static cvar_t	vid_showload = {"vid_showload", "1", CVAR_NONE};

cvar_t		_enable_mouse = {"_enable_mouse", "1", CVAR_ARCHIVE};

static int	vid_default = -1;	// modenum of 320x240 as a safe default
static int	vid_modenum = -1;	// current video mode, set after mode setting succeeds
static int	vid_maxwidth = 640, vid_maxheight = 480;
modestate_t	modestate = MS_UNINIT;

static byte		*vid_surfcache;
static int		vid_surfcachesize;
static int		VID_highhunkmark;

typedef struct {
	modestate_t	type;
	int			width;
	int			height;
	int			modenum;
	int			fullscreen;
	int			bpp;
	int			halfscreen;
	char		modedesc[MAX_DESC];
} vmode_t;

typedef struct {
	int	width;
	int	height;
} stdmode_t;

#define RES_640X480	3
static const stdmode_t	std_modes[] = {
// NOTE: keep this list in order
	{320, 240},	// 0
	{400, 300},	// 1
	{512, 384},	// 2
	{640, 480},	// 3 == RES_640X480, this is our default, below
			//		this is the lowresmodes region.
			//		either do not change its order,
			//		or change the above define, too
	{800,  600},	// 4, RES_640X480 + 1
	{1024, 768}	// 5, RES_640X480 + 2
};

#define MAX_MODE_LIST	30
#define MAX_STDMODES	(sizeof(std_modes) / sizeof(std_modes[0]))
#define NUM_LOWRESMODES	(RES_640X480)
static vmode_t	fmodelist[MAX_MODE_LIST+1];	// list of enumerated fullscreen modes
static vmode_t	wmodelist[MAX_STDMODES +1];	// list of standart 4:3 windowed modes
static vmode_t	*modelist;	// modelist in use, points to one of the above lists

static int VID_SetMode (int modenum, unsigned char *palette);

void VID_MenuDraw (void);
void VID_MenuKey (int key);

// window manager stuff
#if defined(H2W)
#	define WM_TITLEBAR_TEXT	"HexenWorld"
#	define WM_ICON_TEXT	"HexenWorld"
//#elif defined(H2MP)
//#	define WM_TITLEBAR_TEXT	"Hexen II+"
//#	define WM_ICON_TEXT	"HEXEN2MP"
#else
#	define WM_TITLEBAR_TEXT	"Hexen II"
#	define WM_ICON_TEXT	"HEXEN2"
#endif

//====================================

/*
================
ClearAllStates
================
*/
static void ClearAllStates (void)
{
	int		i;
	
// send an up event for each key, to make sure the server clears them all
	for (i = 0; i < 256; i++)
	{
		Key_Event (i, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}


/*
================
VID_AllocBuffers
================
*/
static qboolean VID_AllocBuffers (int width, int height)
{
	int		tsize, tbuffersize;

	tbuffersize = width * height * sizeof (*d_pzbuffer);

	tsize = D_SurfaceCacheForRes (width, height);

	tbuffersize += tsize;

// see if there's enough memory, allowing for the normal mode 0x13 pixel,
// z, and surface buffers
	//if ((host_parms->memsize - tbuffersize + SURFCACHE_SIZE_AT_320X200 +
	//	 0x10000 * 3) < MINIMUM_MEMORY)
	// Pa3PyX: using hopefully better estimation now
	// if total memory < needed surface cache + (minimum operational memory
	// less surface cache for 320x200 and typical hunk state after init)
	if (host_parms->memsize < tbuffersize + 0x180000 + 0xC00000)
	{
		Con_SafePrintf ("Not enough memory for video mode\n");
		return false;		// not enough memory for mode
	}

	vid_surfcachesize = tsize;

	if (d_pzbuffer)
	{
		D_FlushCaches ();
		Hunk_FreeToHighMark (VID_highhunkmark);
		d_pzbuffer = NULL;
	}

	VID_highhunkmark = Hunk_HighMark ();

	d_pzbuffer = Hunk_HighAllocName (tbuffersize, "video");

	vid_surfcache = (byte *)d_pzbuffer +
			width * height * sizeof (*d_pzbuffer);
	
	return true;
}

/*
================
VID_CheckAdequateMem
================
*/
static qboolean VID_CheckAdequateMem (int width, int height)
{
	int		tbuffersize;

	tbuffersize = width * height * sizeof (*d_pzbuffer);

	tbuffersize += D_SurfaceCacheForRes (width, height);

// see if there's enough memory, allowing for the normal mode 0x13 pixel,
// z, and surface buffers
	//if ((host_parms->memsize - tbuffersize + SURFCACHE_SIZE_AT_320X200 +
	//	 0x10000 * 3) < MINIMUM_MEMORY)
	// Pa3PyX: using hopefully better estimation now
	// Experimentation: the heap should have at least 12.0 megs
	// remaining (after init) after setting video mode, otherwise
	// it's Hunk_Alloc failures and cache thrashes upon level load
	if (host_parms->memsize < tbuffersize + 0x180000 + 0xC00000)
	{
		return false;		// not enough memory for mode
	}

	return true;
}

static void VID_SetIcon (void)
{
	SDL_Surface *icon;
	SDL_Color color;
	Uint8 *ptr;
	int i, mask;
#	include "xbm_icon.h"

	icon = SDL_CreateRGBSurface(SDL_SWSURFACE, HOT_ICON_WIDTH, HOT_ICON_HEIGHT, 8, 0, 0, 0, 0);
	if (icon == NULL)
		return;	/* oh well... */
	SDL_SetColorKey(icon, SDL_SRCCOLORKEY, 0);

	color.r = 255;
	color.g = 255;
	color.b = 255;
	SDL_SetColors(icon, &color, 0, 1);	/* just in case */
	color.r = 128;
	color.g = 0;
	color.b = 0;
	SDL_SetColors(icon, &color, 1, 1);

	ptr = (Uint8 *)icon->pixels;
	for (i = 0; i < sizeof(HOT_ICON_bits); i++)
	{
		for (mask = 1; mask != 0x100; mask <<= 1)
		{
			*ptr = (HOT_ICON_bits[i] & mask) ? 1 : 0;
			ptr++;
		}		
	}

	SDL_WM_SetIcon(icon, NULL);
	SDL_FreeSurface(icon);
}

static int sort_modes (const void *arg1, const void *arg2)
{
	const SDL_Rect *a1, *a2;
	a1 = *(SDL_Rect **) arg1;
	a2 = *(SDL_Rect **) arg2;

	if (a1->w == a2->w)
		return a1->h - a2->h;	// lowres-to-highres
	//	return a2->h - a1->h;	// highres-to-lowres
	else
		return a1->w - a2->w;	// lowres-to-highres
	//	return a2->w - a1->w;	// highres-to-lowres
}

static void VID_PrepareModes (SDL_Rect **sdl_modes)
{
	int	i, j;
	qboolean	have_mem, not_multiple;

	num_fmodes = 0;
	num_wmodes = 0;

	// Add the standart 4:3 modes to the windowed modes list
	// In an unlikely case that we receive no fullscreen modes,
	// this will be our modes list (kind of...)
	for (i = 0; i < MAX_STDMODES; i++)
	{
		have_mem = VID_CheckAdequateMem(std_modes[i].width, std_modes[i].height);
		if (!have_mem)
			break;
		wmodelist[num_wmodes].width = std_modes[i].width;
		wmodelist[num_wmodes].height = std_modes[i].height;
		wmodelist[num_wmodes].halfscreen = 0;
		wmodelist[num_wmodes].fullscreen = 0;
		wmodelist[num_wmodes].bpp = 8;
		snprintf (wmodelist[num_wmodes].modedesc, MAX_DESC, "%d x %d", std_modes[i].width, std_modes[i].height);
		num_wmodes++;
	}

	// disaster scenario #1: no fullscreen modes. bind to the
	// windowed modes list. limit it to 640x480 max. because
	// we don't know the desktop dimensions
	if (sdl_modes == (SDL_Rect **)0)
	{
no_fmodes:
		Con_Printf ("No fullscreen video modes available\n");
		if (num_wmodes > RES_640X480)
			num_wmodes = RES_640X480 + 1;
		modelist = wmodelist;
		nummodes = &num_wmodes;
		vid_default = 0;
		Cvar_SetValue ("vid_config_swx", modelist[vid_default].width);
		Cvar_SetValue ("vid_config_swy", modelist[vid_default].height);
		return;
	}

	// another disaster scenario (#2)
	if (sdl_modes == (SDL_Rect **)-1)
	{	// Really should NOT HAVE happened! this return value is
		// for windowed modes!  Since this means all resolutions
		// are supported, use our standart modes as modes list.
		Con_Printf ("Unexpectedly received -1 from SDL_ListModes\n");
		vid_maxwidth = MAXWIDTH;
		vid_maxheight = MAXHEIGHT;
	//	num_fmodes = -1;
		num_fmodes = num_wmodes;
		nummodes = &num_wmodes;
		modelist = wmodelist;
		vid_default = 0;
		Cvar_SetValue ("vid_config_swx", modelist[vid_default].width);
		Cvar_SetValue ("vid_config_swy", modelist[vid_default].height);
		return;
	}

#if 0
	// print the un-processed modelist as reported by SDL
	for (j = 0; sdl_modes[j]; ++j)
	{
		Con_Printf ("%d x %d\n", sdl_modes[j]->w, sdl_modes[j]->h);
	}
	Con_Printf ("Total %d entries\n", j);
#endif

	// count the entries
	j = 0;
	while ( sdl_modes[j] )
		j++;

	// sort the original list from low-res to high-res
	// so that the low resolutions take priority
	qsort(sdl_modes, j, sizeof *sdl_modes, sort_modes);

	for (i = 0; sdl_modes[i] && num_fmodes < MAX_MODE_LIST; ++i)
	{
		// avoid multiple listings of the same dimension
		not_multiple = true;
		for (j = 0; j < num_fmodes; ++j)
		{
			if (fmodelist[j].width == sdl_modes[i]->w && fmodelist[j].height == sdl_modes[i]->h)
			{
				not_multiple = false;
				break;
			}
		}

		// automatically strip-off resolutions that we
		// don't have enough memory for
		have_mem = VID_CheckAdequateMem(sdl_modes[i]->w, sdl_modes[i]->h);

		// avoid resolutions < 320x240
		if (not_multiple && have_mem && sdl_modes[i]->w >= MIN_WIDTH && sdl_modes[i]->h >= MIN_HEIGHT)
		{
			fmodelist[num_fmodes].width = sdl_modes[i]->w;
			fmodelist[num_fmodes].height = sdl_modes[i]->h;
			// FIXME: look at vid_win.c and learn how to
			// really functionalize the halfscreen field.
			fmodelist[num_fmodes].halfscreen = 0;
			fmodelist[num_fmodes].fullscreen = 1;
			fmodelist[num_fmodes].bpp = 8;
			snprintf (fmodelist[num_fmodes].modedesc, MAX_DESC, "%d x %d", sdl_modes[i]->w, sdl_modes[i]->h);
			num_fmodes++;
		}
	}

	if (!num_fmodes)
		goto no_fmodes;

	// At his point, we have a list of valid fullscreen modes:
	// Let's bind to it and use it for windowed modes, as well.
	// The only downside is that if SDL doesn't report any low
	// resolutions to us, we shall not have any for windowed
	// rendering where they would be perfectly legitimate...
	// Since our fullscreen/windowed toggling is instant and
	// doesn't require a vid_restart, switching lists won't be
	// feasible, either. The -width/-height commandline args
	// remain as the user's trusty old friends here.
	nummodes = &num_fmodes;
	modelist = fmodelist;

	vid_maxwidth = fmodelist[num_fmodes-1].width;
	vid_maxheight = fmodelist[num_fmodes-1].height;

	// see if we have 320x240 among the available modes
	for (i = 0; i < num_fmodes; i++)
	{
		if (fmodelist[i].width == 320 && fmodelist[i].height == 240)
		{
			vid_default = i;
			break;
		}
	}

	if (vid_default < 0)
	{
		// 320x240 not found among the supported dimensions
		// set default to the lowest resolution reported
		vid_default = 0;
	}

	// limit the windowed (standart) modes list to desktop dimensions
	for (i = 0; i < num_wmodes; i++)
	{
		if (wmodelist[i].width > vid_maxwidth || wmodelist[i].height > vid_maxheight)
			break;
	}
	if (i < num_wmodes)
		num_wmodes = i;

	Cvar_SetValue ("vid_config_swx", modelist[vid_default].width);
	Cvar_SetValue ("vid_config_swy", modelist[vid_default].height);
}

static void VID_ListModes_f (void)
{
	int	i;

	Con_Printf ("Maximum allowed mode: %d x %d\n", vid_maxwidth, vid_maxheight);
	Con_Printf ("Windowed modes enabled:\n");
	for (i = 0; i < num_wmodes; i++)
		Con_Printf ("%2d:  %u x %u\n", i, wmodelist[i].width, wmodelist[i].height);
	Con_Printf ("Fullscreen modes enumerated:");
	if (num_fmodes)
	{
		Con_Printf ("\n");
		for (i = 0; i < num_fmodes; i++)
			Con_Printf ("%2d:  %u x %u\n", i, fmodelist[i].width, fmodelist[i].height);
	}
	else
	{
		Con_Printf (" None\n");
	}
}

static void VID_NumModes_f (void)
{
	Con_Printf ("%d video modes in current list\n", *nummodes);
}

static void VID_ShowInfo_f (void)
{
	Con_Printf ("Video info:\n"
			"BitsPerPixel: %d,\n"
			"Rmask : %u, Gmask : %u, Bmask : %u\n"
			"Rshift: %u, Gshift: %u, Bshift: %u\n"
			"Rloss : %u, Gloss : %u, Bloss : %u\n"
			"alpha : %u, colorkey: %u\n",
			vid_info->vfmt->BitsPerPixel,
			vid_info->vfmt->Rmask, vid_info->vfmt->Gmask, vid_info->vfmt->Bmask,
			vid_info->vfmt->Rshift, vid_info->vfmt->Gshift, vid_info->vfmt->Bshift,
			vid_info->vfmt->Rloss, vid_info->vfmt->Gloss, vid_info->vfmt->Bloss,
			vid_info->vfmt->alpha, vid_info->vfmt->colorkey	);
}

static int VID_SetMode (int modenum, unsigned char *palette)
{
	Uint32 flags;
	int	is_fullscreen;

	in_mode_set = true;

	//flush the intermission screen if it's cached (Pa3PyX)
	if (intermissionScreen && intermissionScreen->data)
		Cache_Free(intermissionScreen);

	if (screen)
		SDL_FreeSurface(screen);

	flags = (SDL_SWSURFACE|SDL_HWPALETTE);
	if ((int)vid_config_fscr.value)
		flags |= SDL_FULLSCREEN;

	// Set the mode
	screen = SDL_SetVideoMode(modelist[modenum].width, modelist[modenum].height, modelist[modenum].bpp, flags);
	if (!screen)
		return false;

	// initial success. adjust vid vars.
	vid.height = vid.conheight = modelist[modenum].height;
	vid.width = vid.conwidth = modelist[modenum].width;
	vid.buffer = vid.conbuffer = vid.direct = screen->pixels;
	vid.rowbytes = vid.conrowbytes = screen->pitch;
	vid.numpages = 1;
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);

	if (!VID_AllocBuffers (vid.width, vid.height))
		return false;

	D_InitCaches (vid_surfcache, vid_surfcachesize);

	// real success. set vid_modenum properly.
	vid_modenum = modenum;
	is_fullscreen = (screen->flags & SDL_FULLSCREEN) ? 1 : 0;
	modestate = (is_fullscreen) ? MS_FULLDIB : MS_WINDOWED;
	Cvar_SetValue ("vid_config_swx", modelist[vid_modenum].width);
	Cvar_SetValue ("vid_config_swy", modelist[vid_modenum].height);
	Cvar_SetValue ("vid_config_fscr", is_fullscreen);

	IN_HideMouse ();

	ClearAllStates();

	VID_SetPalette (palette);

	// setup the window manager stuff
	VID_SetIcon();
	SDL_WM_SetCaption(WM_TITLEBAR_TEXT, WM_ICON_TEXT);

	Con_SafePrintf ("Video Mode: %dx%dx%d\n", vid.width, vid.height, modelist[modenum].bpp);

	in_mode_set = false;
	vid.recalc_refdef = 1;

	return true;
}

//
// VID_ChangeVideoMode
// intended only as a callback for VID_Restart_f
//
static void VID_ChangeVideoMode(int newmode)
{
	int		stat, temp;

	if (!screen)
		return;

	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;
	CDAudio_Pause ();
	MIDI_Pause (MIDI_ALWAYS_PAUSE);
	S_ClearBuffer ();

	stat = VID_SetMode (newmode, vid_curpal);
	if (!stat)
	{
		if (vid_modenum == newmode)
			Sys_Error ("Couldn't set video mode: %s", SDL_GetError());

		// failed setting mode, probably due to insufficient
		// memory. go back to previous mode.
		Cvar_SetValue ("vid_mode", vid_modenum);
		stat = VID_SetMode (vid_modenum, vid_curpal);
		if (!stat)
			Sys_Error ("Couldn't set video mode: %s", SDL_GetError());
	}

	CDAudio_Resume (); 
	MIDI_Pause (MIDI_ALWAYS_RESUME);
	scr_disabled_for_loading = temp;
}

static void VID_Restart_f (void)
{
	if ((int)vid_mode.value < 0 || (int)vid_mode.value >= *nummodes)
	{
		Con_Printf ("Bad video mode %d\n", (int)vid_mode.value);
		Cvar_SetValue ("vid_mode", vid_modenum);
		return;
	}

	Con_Printf ("Re-initializing video:\n");
	VID_ChangeVideoMode ((int)vid_mode.value);
}

void VID_LockBuffer (void)
{
	lockcount++;

	if (lockcount > 1)
		return;

	SDL_LockSurface(screen);

	// Update surface pointer for linear access modes
	vid.buffer = vid.conbuffer = vid.direct = screen->pixels;
	vid.rowbytes = vid.conrowbytes = screen->pitch;

	if (r_dowarp)
		d_viewbuffer = r_warpbuffer;
	else
		d_viewbuffer = (void *)(byte *)vid.buffer;

	if (r_dowarp)
		screenwidth = WARP_WIDTH;
	else
		screenwidth = vid.rowbytes;
}

void VID_UnlockBuffer (void)
{
	lockcount--;

	if (lockcount > 0)
		return;

	if (lockcount < 0)
		Sys_Error ("Unbalanced unlock");

	SDL_UnlockSurface (screen);

// to turn up any unlocked accesses
	//vid.buffer = vid.conbuffer = vid.direct = d_viewbuffer = NULL;
}


void VID_SetPalette (unsigned char *palette)
{
	int		i;
	SDL_Color colors[256];

	palette_changed = true;

	memcpy (vid_curpal, palette, sizeof(vid_curpal));

	for (i = 0; i < 256; ++i)
	{
		colors[i].r = *palette++;
		colors[i].g = *palette++;
		colors[i].b = *palette++;
	}

	SDL_SetColors(screen, colors, 0, 256);
}


void VID_ShiftPalette (unsigned char *palette)
{
	VID_SetPalette (palette);
}


/*
===================
VID_Init
===================
*/
void VID_Init (unsigned char *palette)
{
	int		width, height, i, temp;
	SDL_Rect	**enumlist;
	const char	*read_vars[] = {
				"vid_config_fscr",
				"vid_config_swx",
				"vid_config_swy" };
#define num_readvars	( sizeof(read_vars)/sizeof(read_vars[0]) )

	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	Cvar_RegisterVariable (&vid_config_fscr);
	Cvar_RegisterVariable (&vid_config_swy);
	Cvar_RegisterVariable (&vid_config_swx);
	Cvar_RegisterVariable (&vid_config_gly);
	Cvar_RegisterVariable (&vid_config_glx);
	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&_enable_mouse);
	Cvar_RegisterVariable (&vid_showload);

	Cmd_AddCommand ("vid_showinfo", VID_ShowInfo_f);
	Cmd_AddCommand ("vid_listmodes", VID_ListModes_f);
	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f);
	Cmd_AddCommand ("vid_restart", VID_Restart_f);

	// init sdl
	// the first check is actually unnecessary
	if ( (SDL_WasInit(SDL_INIT_VIDEO)) == 0 )
	{
		if (SDL_Init(SDL_INIT_VIDEO) < 0)
			Sys_Error("VID: Couldn't load SDL: %s", SDL_GetError());
	}

	// this will contain the "best bpp" for the current display
	// make sure to re-retrieve it if you ever re-init sdl_video
	vid_info = SDL_GetVideoInfo();

	// retrieve the list of fullscreen modes
	enumlist = SDL_ListModes(NULL, SDL_SWSURFACE|SDL_HWPALETTE|SDL_FULLSCREEN);
	// prepare the modelists, find the actual modenum for vid_default
	VID_PrepareModes(enumlist);

	// set vid_mode to our safe default first
	Cvar_SetValue ("vid_mode", vid_default);

	// perform an early read of config.cfg
	CFG_ReadCvars (read_vars, num_readvars);

	// windowed mode is default
	// see if the user wants fullscreen
	if (COM_CheckParm("-fullscreen") || COM_CheckParm("-f"))
	{
		Cvar_SetValue("vid_config_fscr", 1);
	}
	else if (COM_CheckParm("-window") || COM_CheckParm("-w"))
	{
		Cvar_SetValue("vid_config_fscr", 0);
	}

	if ((int)vid_config_fscr.value && !num_fmodes) // FIXME: see below, as well
		Sys_Error ("No fullscreen modes available at this color depth");

	width = (int)vid_config_swx.value;
	height = (int)vid_config_swy.value;

	// user is always right ...
	i = COM_CheckParm("-width");
	if (i && i < com_argc-1)
	{	// FIXME: this part doesn't know about a disaster case
		// like we aren't reported any fullscreen modes.
		width = atoi(com_argv[i+1]);

		i = COM_CheckParm("-height");
		if (i && i < com_argc-1)
			height = atoi(com_argv[i+1]);
		else	// proceed with 4/3 ratio
			height = 3 * width / 4;
	}

	// user requested a mode either from the config or from the
	// command line
	// scan existing modes to see if this is already available
	// if not, add this as the last "valid" video mode and set
	// vid_mode to it only if it doesn't go beyond vid_maxwidth
	i = 0;
	while (i < *nummodes)
	{
		if (modelist[i].width == width && modelist[i].height == height)
			break;
		i++;
	}
	if (i < *nummodes)
	{
		Cvar_SetValue ("vid_mode", i);
	}
	else if ( (width <= vid_maxwidth && width >= MIN_WIDTH &&
		   height <= vid_maxheight && height >= MIN_HEIGHT) ||
		  COM_CheckParm("-force") )
	{
		modelist[*nummodes].width = width;
		modelist[*nummodes].height = height;
		modelist[*nummodes].halfscreen = 0;
		modelist[*nummodes].fullscreen = 1;
		modelist[*nummodes].bpp = 8;
		snprintf (modelist[*nummodes].modedesc, MAX_DESC, "%d x %d (user mode)", width, height);
		Cvar_SetValue ("vid_mode", *nummodes);
		(*nummodes)++;
	}
	else
	{
		Con_Printf ("ignoring invalid -width and/or -height arguments\n");
	}

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	i = VID_SetMode((int)vid_mode.value, palette);
	if ( !i )
	{
		if ((int)vid_mode.value == vid_default)
			Sys_Error ("Couldn't set video mode: %s", SDL_GetError());

		// just one more try before dying
		Con_Printf ("Couldn't set video mode %d\n"
			    "Trying the default mode\n", (int)vid_mode.value);
		//Cvar_SetValue("vid_config_fscr", 0);
		Cvar_SetValue ("vid_mode", vid_default);
		i = VID_SetMode(vid_default, palette);
		if ( !i )
			Sys_Error ("Couldn't set video mode: %s", SDL_GetError());
	}

	// lock the early-read cvars until Host_Init is finished
	Cvar_LockVars (read_vars, num_readvars);

	scr_disabled_for_loading = temp;
	vid_initialized = true;

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;
}


void VID_Shutdown (void)
{
	if (vid_initialized)
	{
		if (screen != NULL && lockcount > 0)
			SDL_UnlockSurface (screen);

		vid_initialized = 0;
		SDL_Quit();
	}
}


/*
================
FlipScreen
================
*/
static void FlipScreen(vrect_t *rects)
{
	while (rects)
	{
		SDL_UpdateRect (screen, rects->x, rects->y, rects->width,
				rects->height);
		rects = rects->pnext;
	}
}

void VID_Update (vrect_t *rects)
{
	vrect_t	rect;

	if (palette_changed)
	{
		palette_changed = false;
		rect.x = 0;
		rect.y = 0;
		rect.width = vid.width;
		rect.height = vid.height;
		rect.pnext = NULL;
		rects = &rect;
	}

	// We've drawn the frame; copy it to the screen
	FlipScreen (rects);

	// handle the mouse state when windowed if that's changed
#if 0	// change to 1 if dont want to disable mouse in fullscreen
	if (modestate == MS_WINDOWED)
#endif
		if ((int)_enable_mouse.value != enable_mouse)
		{
			if (_enable_mouse.value)
				IN_ActivateMouse ();
			else
				IN_DeactivateMouse ();

			enable_mouse = (int)_enable_mouse.value;
		}
}


/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
//	these bits from quakeforge
	Uint8      *offset;

	if (!screen)
		return;
	if (x < 0)
		x = screen->w + x - 1;
	offset = (Uint8 *) screen->pixels + y * screen->pitch + x;
	while (height--)
	{
		memcpy (offset, pbitmap, width);
		offset += screen->pitch;
		pbitmap += width;
	}
}

/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
//	these bits from quakeforge
	if (!screen)
		return;
	if (x < 0)
		x = screen->w + x - 1;
	SDL_UpdateRect (screen, x, y, width, height);
}


#ifndef H2W
// unused in hexenworld
void D_ShowLoadingSize (void)
{
	vrect_t		rect;
	viddef_t	save_vid;	// global video state

	if (!vid_showload.value)
		return; 

	if (!vid_initialized)
		return;

	save_vid = vid;
	if (vid.numpages == 1)
	{
		VID_LockBuffer ();

		if (!vid.direct)
			Sys_Error ("NULL vid.direct pointer");

		vid.buffer = vid.direct;

		SCR_DrawLoading();

		VID_UnlockBuffer ();

		rect.x = 0;
		rect.y = 0;
		rect.width = vid.width;
		rect.height = 112;
		rect.pnext = NULL;

		FlipScreen (&rect);
	}
	else
	{

		vid.buffer = (byte *)screen->pixels;
		vid.rowbytes = screen->pitch;

		SCR_DrawLoading();

	}

	vid = save_vid;
}
#endif


/*
============================
Gamma functions for UNIX/SDL
============================
*/
#if 0
static void VID_SetGamma(void)
{
	float value;

	if ((v_gamma.value != 0)&&(v_gamma.value > (1/GAMMA_MAX)))
		value = 1.0/v_gamma.value;
	else
		value = GAMMA_MAX;

	SDL_SetGamma(value,value,value);
}
#endif


//==========================================================================

/*
================
VID_HandlePause
================
*/
void VID_HandlePause (qboolean paused)
{
#if 0	// change to 1 if dont want to disable mouse in fullscreen
	if ((modestate == MS_WINDOWED) && _enable_mouse.value)
#else
	if (_enable_mouse.value)
#endif
	{
		// for consistency , don't show pointer S.A
		if (paused)
		{
			IN_DeactivateMouse ();
			// IN_ShowMouse ();
		}
		else
		{
			IN_ActivateMouse ();
			// IN_HideMouse ();
		}
	}
}


/*
================
VID_ToggleFullscreen
Handles switching between fullscreen/windowed modes
and brings the mouse to a proper state afterwards
================
*/
extern qboolean mousestate_sa;
void VID_ToggleFullscreen (void)
{
	int	is_fullscreen;

	if (!fs_toggle_works)
		return;
	if (!num_fmodes)
		return;
	if (!screen)
		return;

	S_ClearBuffer ();

	if ( SDL_WM_ToggleFullScreen(screen) > 0 )
	{
		is_fullscreen = (screen->flags & SDL_FULLSCREEN) ? 1 : 0;
		Cvar_SetValue("vid_config_fscr", is_fullscreen);
		modestate = (is_fullscreen) ? MS_FULLDIB : MS_WINDOWED;
		if (is_fullscreen)
		{
			// activate mouse in fullscreen mode
			// in_sdl.c handles other non-moused cases
			if (mousestate_sa)
				IN_ActivateMouse();
		}
		else
		{	// windowed mode:
			// deactivate mouse if we are in menus
			if (mousestate_sa)
				IN_DeactivateMouse();
		}
		// update the video menu option
		vid_menu_fs = (modestate != MS_WINDOWED);
	}
	else
	{
		fs_toggle_works = false;
		Con_Printf ("SDL_WM_ToggleFullScreen failed\n");
	}
}


//========================================================
// Video menu stuff
//========================================================

static int	vid_menunum;
static int	vid_cursor;
static qboolean	want_fstoggle, need_apply;
static qboolean	vid_menu_firsttime = true;

enum {
	VID_FULLSCREEN,	// make sure the fullscreen entry (0)
	VID_RESOLUTION,	// is lower than resolution entry (1)
	VID_BLANKLINE,	// spacer line
	VID_RESET,
	VID_APPLY,
	VID_ITEMS
};

static void M_DrawYesNo (int x, int y, int on, int white)
{
	if (on)
	{
		if (white)
			M_PrintWhite (x, y, "yes");
		else
			M_Print (x, y, "yes");
	}
	else
	{
		if (white)
			M_PrintWhite (x, y, "no");
		else
			M_Print (x, y, "no");
	}
}

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	ScrollTitle("gfx/menu/title7.lmp");

	if (vid_menu_firsttime)
	{	// settings for entering the menu first time
		vid_menunum = vid_modenum;
		vid_menu_fs = (modestate != MS_WINDOWED);
		vid_cursor = (num_fmodes) ? 0 : VID_RESOLUTION;
		vid_menu_firsttime = false;
	}

	want_fstoggle = ( ((modestate == MS_WINDOWED) && vid_menu_fs) || ((modestate != MS_WINDOWED) && !vid_menu_fs) );

	need_apply = (vid_menunum != vid_modenum) || want_fstoggle;

	M_Print (76, 92 + 8*VID_FULLSCREEN, "Fullscreen: ");
	M_DrawYesNo (76+12*8, 92 + 8*VID_FULLSCREEN, vid_menu_fs, !want_fstoggle);

	M_Print (76, 92 + 8*VID_RESOLUTION, "Resolution: ");
	if (vid_menunum == vid_modenum)
		M_PrintWhite (76+12*8, 92 + 8*VID_RESOLUTION, modelist[vid_menunum].modedesc);
	else
		M_Print (76+12*8, 92 + 8*VID_RESOLUTION, modelist[vid_menunum].modedesc);

	if (need_apply)
	{
		M_Print (76, 92 + 8*VID_RESET, "RESET CHANGES");
		M_Print (76, 92 + 8*VID_APPLY, "APPLY CHANGES");
	}

	M_DrawCharacter (64, 92 + vid_cursor*8, 12+((int)(realtime*4)&1));
}

/*
================
VID_MenuKey
================
*/
void VID_MenuKey (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		vid_cursor = (num_fmodes) ? 0 : VID_RESOLUTION;
		M_Menu_Options_f ();
		return;

	case K_ENTER:
		switch (vid_cursor)
		{
		case VID_RESET:
			vid_menu_fs = (modestate != MS_WINDOWED);
			vid_menunum = vid_modenum;
			vid_cursor = (num_fmodes) ? 0 : VID_RESOLUTION;
			break;
		case VID_APPLY:
			if (need_apply)
			{
				Cvar_SetValue("vid_mode", vid_menunum);
				Cvar_SetValue("vid_config_fscr", vid_menu_fs);
				VID_Restart_f();
			}
			vid_cursor = (num_fmodes) ? 0 : VID_RESOLUTION;
			break;
		}
		return;

	case K_LEFTARROW:
		switch (vid_cursor)
		{
		case VID_FULLSCREEN:
			vid_menu_fs = !vid_menu_fs;
			if (fs_toggle_works)
				VID_ToggleFullscreen();
			break;
		case VID_RESOLUTION:
			S_LocalSound ("raven/menu1.wav");
			vid_menunum--;
			if (vid_menunum < 0)
				vid_menunum = 0;
			break;
		}
		return;

	case K_RIGHTARROW:
		switch (vid_cursor)
		{
		case VID_FULLSCREEN:
			vid_menu_fs = !vid_menu_fs;
			if (fs_toggle_works)
				VID_ToggleFullscreen();
			break;
		case VID_RESOLUTION:
			S_LocalSound ("raven/menu1.wav");
			vid_menunum++;
			if (vid_menunum >= *nummodes)
				vid_menunum = *nummodes - 1;
			break;
		}
		return;

	case K_UPARROW:
		S_LocalSound ("raven/menu1.wav");
		vid_cursor--;
		if (vid_cursor < 0)
		{
			vid_cursor = (need_apply) ? VID_ITEMS-1 : VID_BLANKLINE-1;
		}
		else if (vid_cursor == VID_BLANKLINE)
		{
			vid_cursor--;
		}
		break;

	case K_DOWNARROW:
		S_LocalSound ("raven/menu1.wav");
		vid_cursor++;
		if (vid_cursor >= VID_ITEMS)
		{
			vid_cursor = (num_fmodes) ? 0 : VID_RESOLUTION;
			break;
		}
		if (vid_cursor >= VID_BLANKLINE)
		{
			if (need_apply)
			{
				if (vid_cursor == VID_BLANKLINE)
					vid_cursor++;
			}
			else
			{
				vid_cursor = (num_fmodes) ? 0 : VID_RESOLUTION;
			}
		}
		break;

	default:
		return;
	}
}

