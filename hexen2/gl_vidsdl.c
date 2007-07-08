/*
	gl_vidsdl.c -- SDL GL vid component
	Select window size and mode and init SDL in GL mode.

	$Id: gl_vidsdl.c,v 1.165 2007-07-08 11:55:19 sezero Exp $

	Changed 7/11/04 by S.A.
	- Fixed fullscreen opengl mode, window sizes
	- Options are now: -fullscreen, -height, -width, -bpp
	- The "-mode" option has been removed

	Changed 7/01/06 by O.S
	- Added video modes enumeration via SDL
	- Added video mode changing on the fly.
*/

#define	__GL_FUNC_EXTERN

#include "quakedef.h"
#include "cfgfile.h"
#include "sdl_inc.h"
#include <unistd.h>


#define WARP_WIDTH		320
#define WARP_HEIGHT		200
#define MAXWIDTH		10000
#define MAXHEIGHT		10000
#define MIN_WIDTH		320
//#define MIN_HEIGHT		200
#define MIN_HEIGHT		240
#define MAX_DESC		33

typedef struct {
	modestate_t	type;
	int			width;
	int			height;
	int			modenum;
	int			dib;
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
	{1024, 768},	// 5, RES_640X480 + 2
	{1280, 1024},	// 6
	{1600, 1200}	// 7
};

#define MAX_MODE_LIST	40
#define MAX_STDMODES	(sizeof(std_modes) / sizeof(std_modes[0]))
#define NUM_LOWRESMODES	(RES_640X480)
static vmode_t	fmodelist[MAX_MODE_LIST+1];	// list of enumerated fullscreen modes
static vmode_t	wmodelist[MAX_STDMODES +1];	// list of standart 4:3 windowed modes
static vmode_t	*modelist;	// modelist in use, points to one of the above lists

static int	num_fmodes;
static int	num_wmodes;
static int	*nummodes;
static int	bpp = 16;

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

typedef struct {
	int	red,
		green,
		blue,
		alpha,
		depth,
		stencil;
} attributes_t;

static attributes_t	vid_attribs;
static const SDL_VideoInfo	*vid_info;
static SDL_Surface	*screen;
static qboolean	vid_menu_fs;
static qboolean	fs_toggle_works = true;

// vars for vid state
viddef_t	vid;			// global video state
modestate_t	modestate = MS_UNINIT;
static int	vid_default = -1;	// modenum of 640x480 as a safe default
static int	vid_modenum = -1;	// current video mode, set after mode setting succeeds
static int	vid_maxwidth = 640, vid_maxheight = 480;
static qboolean	vid_conscale = false;
static char	vid_consize[MAX_DESC];
static int	WRHeight, WRWidth;

extern qboolean	scr_skipupdate;
extern qboolean	draw_reinit;
static qboolean	vid_initialized = false;
qboolean	in_mode_set = false;

// cvar vid_mode must be set before calling
// VID_SetMode, VID_ChangeVideoMode or VID_Restart_f
static cvar_t	vid_mode = {"vid_mode", "0", CVAR_NONE};
static cvar_t	vid_config_consize = {"vid_config_consize", "640", CVAR_ARCHIVE};
static cvar_t	vid_config_glx = {"vid_config_glx", "640", CVAR_ARCHIVE};
static cvar_t	vid_config_gly = {"vid_config_gly", "480", CVAR_ARCHIVE};
static cvar_t	vid_config_swx = {"vid_config_swx", "320", CVAR_ARCHIVE};
static cvar_t	vid_config_fscr= {"vid_config_fscr", "0", CVAR_ARCHIVE};
// cvars for compatibility with the software version
static cvar_t	vid_config_swy = {"vid_config_swy", "240", CVAR_ARCHIVE};

byte		globalcolormap[VID_GRADES*256];
float		RTint[256], GTint[256], BTint[256];
unsigned short	d_8to16table[256];
unsigned	d_8to24table[256];
unsigned	d_8to24TranslucentTable[256];
#if USE_HEXEN2_PALTEX_CODE
unsigned char	inverse_pal[(1<<INVERSE_PAL_TOTAL_BITS)+1]; // +1: FS_LoadStackFile puts a 0 at the end of the data
#else
unsigned char	d_15to8table[65536];
#endif

// gl stuff
static void GL_Init (void);

#ifdef GL_DLSYM
static const char	*gl_library;
#endif

static const char	*gl_vendor;
static const char	*gl_renderer;
static const char	*gl_version;
static const char	*gl_extensions;
qboolean	is_3dfx = false;

GLint		gl_max_size = 256;
float		gldepthmin, gldepthmax;
GLuint		texture_extension_number = 1U;

// palettized textures
typedef void	(APIENTRY *glColorTableEXT_f)(int, int, int, int, int, const void*);
static	glColorTableEXT_f  glColorTableEXT_fp;
static qboolean	have8bit = false;
qboolean	is8bit = false;
static cvar_t	vid_config_gl8bit = {"vid_config_gl8bit", "0", CVAR_ARCHIVE};

// Gamma stuff
#define	USE_GAMMA_RAMPS			0
#if USE_GAMMA_RAMPS
extern unsigned short	ramps[3][256];
static unsigned short	orig_ramps[3][256];
#endif

// 3dfx gamma hacks: stuff are in fx_gamma.c
// Note: gamma ramps crashes voodoo graphics
#if defined(USE_3DFXGAMMA)
#include "fx_gamma.h"
#endif	/* USE_3DFXGAMMA */

static qboolean	fx_gamma   = false;	// 3dfx-specific gamma control
static qboolean	gammaworks = false;	// whether hw-gamma works
qboolean	gl_dogamma = false;	// none of the above two, use gl tricks

// multitexturing
qboolean	gl_mtexable = false;
static GLint	num_tmus = 1;

// multisampling
static int	multisample = 0; // never set this to non-zero if SDL isn't multisampling-capable
static qboolean	sdl_has_multisample = false;
static cvar_t	vid_config_fsaa = {"vid_config_fsaa", "0", CVAR_ARCHIVE};

// stencil buffer
qboolean	have_stencil = false;

// misc gl tweaks
static qboolean	fullsbardraw = false;

// misc external data and functions
extern void	Mod_ReloadTextures (void);

// menu drawing
static void VID_MenuDraw (void);
static void VID_MenuKey (int key);

// input stuff
static void ClearAllStates (void);
static int	enable_mouse;
cvar_t		_enable_mouse = {"_enable_mouse", "1", CVAR_ARCHIVE};


//====================================

void VID_HandlePause (qboolean paused)
{
#if 0	// change to 1 if dont want to disable mouse in fullscreen
	if ((modestate == MS_WINDOWED) && _enable_mouse.integer)
#else
	if (_enable_mouse.integer)
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


//====================================

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

static void VID_ConWidth (int modenum)
{
	int	w, h;

	if (!vid_conscale)
	{
		Cvar_SetValue ("vid_config_consize", modelist[modenum].width);
		goto finish;
	}

	w = vid_config_consize.integer;
// disabling this: we may have non-multiple-of-eight resolutions
//	w &= 0xfff8; // make it a multiple of eight

	if (w < MIN_WIDTH)
		w = MIN_WIDTH;

// pick a conheight that matches with correct aspect
//	h = w * 3 / 4;
	h = w * modelist[modenum].height / modelist[modenum].width;
//	if (h < MIN_HEIGHT
	if (h < 200 || h > modelist[modenum].height || w > modelist[modenum].width)
	{
		Cvar_SetValue ("vid_config_consize", modelist[modenum].width);
		vid_conscale = false;
		goto finish;
	}
	vid.width = vid.conwidth = w;
	vid.height = vid.conheight = h;
	if (w != modelist[modenum].width)
		vid_conscale = true;
	else
		vid_conscale = false;
finish:
	snprintf (vid_consize, sizeof(vid_consize), "x%.2f (at %ux%u)",
			(float)modelist[vid_modenum].width/vid.conwidth, vid.conwidth, vid.conheight);
}

void VID_ChangeConsize(int key)
{
	int	i, w, h;

	switch (key)
	{
	case K_LEFTARROW:	// smaller text, bigger res nums
		for (i = 0; i <= RES_640X480+1; i++)
		{
			w = std_modes[i].width;
			if (w > vid.conwidth && w <= modelist[vid_modenum].width)
				goto set_size;
		}
		w = modelist[vid_modenum].width;
		goto set_size;

	case K_RIGHTARROW:	// bigger text, smaller res nums
		for (i = RES_640X480+1; i >= 0; i--)
		{
			w = std_modes[i].width;
			if (w < vid.conwidth && w <= modelist[vid_modenum].width)
				goto set_size;
		}
		w = std_modes[0].width;
		goto set_size;

	default:	// bad key
		return;
	}

set_size:
	// preserve the same aspect ratio as the resolution
	h = w * modelist[vid_modenum].height / modelist[vid_modenum].width;
//	if (h < MIN_HEIGHT)
	if (h < 200)
		return;
	vid.width = vid.conwidth = w;
	vid.height = vid.conheight = h;
	Cvar_SetValue ("vid_config_consize", vid.conwidth);
	vid.recalc_refdef = 1;
	if (vid.conwidth != modelist[vid_modenum].width)
		vid_conscale = true;
	else
		vid_conscale = false;

	snprintf (vid_consize, sizeof(vid_consize), "x%.2f (at %ux%u)",
			(float)modelist[vid_modenum].width/vid.conwidth, vid.conwidth, vid.conheight);
}

char *VID_ReportConsize(void)
{
	return vid_consize;
}


static int VID_SetMode (int modenum)
{
	Uint32	flags;
	int	i, is_fullscreen;

	in_mode_set = true;

	//flags = (SDL_OPENGL|SDL_NOFRAME);
	flags = (SDL_OPENGL);
	if (vid_config_fscr.integer)
		flags |= SDL_FULLSCREEN;

	// setup the attributes
	if (bpp >= 32)
	{
		SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
		SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
		SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 8 );
	}
	else
	{
		SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 5 );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 5 );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 5 );
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
	}
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

	if (multisample && !sdl_has_multisample)
	{
		multisample = 0;
		Con_Printf ("SDL ver < %d, multisampling disabled\n", SDL_VER_WITH_MULTISAMPLING);
	}
	if (multisample)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, multisample);
	}

	Con_Printf ("Requested mode %d: %dx%dx%d\n", modenum, modelist[modenum].width, modelist[modenum].height, bpp);
	screen = SDL_SetVideoMode (modelist[modenum].width, modelist[modenum].height, bpp, flags);
	if (!screen)
	{
		if (!multisample)
		{
			Sys_Error ("Couldn't set video mode: %s", SDL_GetError());
		}
		else
		{
			Con_Printf ("multisample window failed\n");
			multisample = 0;
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, multisample);
			screen = SDL_SetVideoMode (modelist[modenum].width, modelist[modenum].height, bpp, flags);
			if (!screen)
				Sys_Error ("Couldn't set video mode: %s", SDL_GetError());
		}
	}

	// success. set vid_modenum properly and adjust other vars.
	vid_modenum = modenum;
	is_fullscreen = (screen->flags & SDL_FULLSCREEN) ? 1 : 0;
	modestate = (is_fullscreen) ? MS_FULLDIB : MS_WINDOWED;
	Cvar_SetValue ("vid_config_glx", modelist[vid_modenum].width);
	Cvar_SetValue ("vid_config_gly", modelist[vid_modenum].height);
	Cvar_SetValue ("vid_config_fscr", is_fullscreen);
	WRWidth = vid.width = vid.conwidth = modelist[modenum].width;
	WRHeight = vid.height = vid.conheight = modelist[modenum].height;

	// setup the effective console width
	VID_ConWidth(modenum);

	SDL_GL_GetAttribute(SDL_GL_BUFFER_SIZE, &i);
	Con_Printf ("Video Mode Set : %ux%ux%d\n", vid.width, vid.height, i);
	if (multisample)
	{
		SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &multisample);
		Con_Printf ("multisample buffer with %i samples\n", multisample);
	}
	Cvar_SetValue ("vid_config_fsaa", multisample);

	// collect the actual attributes
	memset (&vid_attribs, 0, sizeof(attributes_t));
	SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &vid_attribs.red);
	SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &vid_attribs.green);
	SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &vid_attribs.blue);
	SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &vid_attribs.alpha);
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &vid_attribs.depth);
	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &vid_attribs.stencil);
	Con_Printf ("vid_info: red: %d, green: %d, blue: %d, alpha: %d, depth: %d\n",
			vid_attribs.red, vid_attribs.green, vid_attribs.blue, vid_attribs.alpha, vid_attribs.depth);

	// setup the window manager stuff
	VID_SetIcon();
	SDL_WM_SetCaption(WM_TITLEBAR_TEXT, WM_ICON_TEXT);

	IN_HideMouse ();

	in_mode_set = false;

	return true;
}


//====================================

static void VID_Init8bitPalette (void)
{
	// Check for 8bit Extensions and initialize them.
	int			i;
	char	thePalette[256*3];
	char	*oldPalette, *newPalette;

	have8bit = false;
	is8bit = false;
	glColorTableEXT_fp = NULL;

	if (strstr(gl_extensions, "GL_EXT_shared_texture_palette"))
	{
		glColorTableEXT_fp = (glColorTableEXT_f)SDL_GL_GetProcAddress("glColorTableEXT");
		if (glColorTableEXT_fp == NULL)
		{
			return;
		}
		have8bit = true;

		if (!vid_config_gl8bit.integer)
			return;

		oldPalette = (char *) d_8to24table;
		newPalette = thePalette;
		for (i = 0; i < 256; i++)
		{
			*newPalette++ = *oldPalette++;
			*newPalette++ = *oldPalette++;
			*newPalette++ = *oldPalette++;
			oldPalette++;
		}

		glEnable_fp (GL_SHARED_TEXTURE_PALETTE_EXT);
		glColorTableEXT_fp (GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, 256,
					GL_RGB, GL_UNSIGNED_BYTE, (void *) thePalette);
		is8bit = true;
		Con_Printf("8-bit palettized textures enabled\n");
	}
}


#if defined (USE_3DFXGAMMA)
static qboolean VID_Check3dfxGamma (void)
{
	int		ret;

	if ( ! COM_CheckParm("-3dfxgamma") )
		return false;

#if USE_GAMMA_RAMPS
// not recommended for Voodoo1, currently crashes
	ret = glGetDeviceGammaRamp3DFX(orig_ramps);
	if (ret != 0)
	{
		Con_Printf ("Using 3dfx glide3 specific gamma ramps\n");
		return true;
	}
#else	/* not using gamma ramps */
	ret = Init_3dfxGammaCtrl();
	if (ret > 0)
	{
		Con_Printf ("Using 3dfx glide%d gamma controls\n", ret);
		return true;
	}
#endif

	return false;
}
#endif	/* USE_3DFXGAMMA */

static void VID_InitGamma (void)
{
	if (is_3dfx)
	{
	// we don't have WGL_3DFX_gamma_control or an equivalent
	// in unix. If we have it one day, I'll put stuff checking
	// for and linking to it here.
	// Otherwise, assuming is_3dfx means Voodoo1 or Voodoo2,
	// this means we dont have hw-gamma, just use gl_dogamma
#if defined (USE_3DFXGAMMA)
	// Here is an evil hack to abuse the Glide symbols exposed on us
		fx_gamma = VID_Check3dfxGamma();
#endif	/* USE_3DFXGAMMA */
		if (!fx_gamma)
			gl_dogamma = true;
	}

	if (!fx_gamma && !gl_dogamma)
	{
#if USE_GAMMA_RAMPS
		if (SDL_GetGammaRamp(orig_ramps[0], orig_ramps[1], orig_ramps[2]) == 0)
#else
		if (SDL_SetGamma(v_gamma.value,v_gamma.value,v_gamma.value) == 0)
#endif
			gammaworks = true;
		else
			gl_dogamma = true;
	}

	if (gl_dogamma)
		Con_Printf("gamma not available, using gl tricks\n");
}

static void VID_ShutdownGamma (void)
{
#if USE_GAMMA_RAMPS	/* restore hardware gamma */
#if defined (USE_3DFXGAMMA)
	if (fx_gamma)
		glSetDeviceGammaRamp3DFX(orig_ramps);
	else
#endif	/* USE_3DFXGAMMA */
	if (!fx_gamma && !gl_dogamma && gammaworks)
		SDL_SetGammaRamp(orig_ramps[0], orig_ramps[1], orig_ramps[2]);
#endif	/* USE_GAMMA_RAMPS */
#if defined (USE_3DFXGAMMA)
	if (fx_gamma)
		Shutdown_3dfxGamma();
#endif	/* USE_3DFXGAMMA */
}

#if USE_GAMMA_RAMPS
static void VID_SetGammaRamp (void)
{
#if defined (USE_3DFXGAMMA)
	if (fx_gamma)
		glSetDeviceGammaRamp3DFX(ramps);
	else
#endif	/* USE_3DFXGAMMA */
	if (!gl_dogamma && gammaworks)
		SDL_SetGammaRamp(ramps[0], ramps[1], ramps[2]);
}
#else /* ! USE_GAMMA_RAMPS */
static void VID_SetGamma (void)
{
	float value;

	if (v_gamma.value > (1.0 / GAMMA_MAX))
		value = 1.0 / v_gamma.value;
	else
		value = GAMMA_MAX;

#if defined (USE_3DFXGAMMA)
	if (fx_gamma)
		do3dfxGammaCtrl(value);
	else
#endif	/* USE_3DFXGAMMA */
	if (!gl_dogamma && gammaworks)
		SDL_SetGamma(value,value,value);
}
#endif	/* USE_GAMMA_RAMPS */

void VID_ShiftPalette (unsigned char *palette)
{
#if USE_GAMMA_RAMPS
	VID_SetGammaRamp();
#else
	VID_SetGamma();
#endif
}


static void CheckMultiTextureExtensions(void)
{
	gl_mtexable = false;

	if (COM_CheckParm("-nomtex"))
	{
		Con_Printf("Multitexture extensions disabled\n");
	}
	else if (strstr(gl_extensions, "GL_ARB_multitexture"))
	{
		Con_Printf("ARB Multitexture extensions found\n");

		glGetIntegerv_fp(GL_MAX_TEXTURE_UNITS_ARB, &num_tmus);
		if (num_tmus < 2)
		{
			Con_Printf("not enough TMUs, ignoring multitexture\n");
			return;
		}

		glMultiTexCoord2fARB_fp = (glMultiTexCoord2fARB_f) SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
		glActiveTextureARB_fp = (glActiveTextureARB_f) SDL_GL_GetProcAddress("glActiveTextureARB");
		if ((glMultiTexCoord2fARB_fp == NULL) ||
		    (glActiveTextureARB_fp == NULL))
		{
			Con_Printf ("Couldn't link to multitexture functions\n");
			return;
		}

		Con_Printf("Found %i TMUs support\n", num_tmus);
		gl_mtexable = true;

		// start up with the correct texture selected!
		glDisable_fp(GL_TEXTURE_2D);
		glActiveTextureARB_fp(GL_TEXTURE0_ARB);
	}
	else
	{
		Con_Printf("GL_ARB_multitexture not found\n");
	}
}

static void CheckStencilBuffer(void)
{
	have_stencil = false;

	if (vid_attribs.stencil)
	{
		Con_Printf("Stencil buffer created with %d bits\n", vid_attribs.stencil);
		have_stencil = true;
	}
}


#ifdef GL_DLSYM
static qboolean GL_OpenLibrary(const char *name)
{
	int	ret;
	char	gl_liblocal[MAX_OSPATH];

	ret = SDL_GL_LoadLibrary(name);

	if (ret < 0)
	{
		// In case of user-specified gl library, look for it under the
		// installation directory, too: the user may forget providing
		// a valid path information. In that case, make sure it doesnt
		// contain any path information and exists in our own basedir,
		// then try loading it
		if ( name && !strchr(name, '/') )
		{
			snprintf (gl_liblocal, MAX_OSPATH, "%s/%s", fs_basedir, name);
			if (access(gl_liblocal, R_OK) == -1)
				return false;

			Con_Printf ("Failed loading gl library %s\n"
				    "Trying to load %s\n", name, gl_liblocal);

			ret = SDL_GL_LoadLibrary(gl_liblocal);
			if (ret < 0)
				return false;

			Con_Printf("Using GL library: %s\n", gl_liblocal);
			return true;
		}

		return false;
	}

	if (name)
		Con_Printf("Using GL library: %s\n", name);
	else
		Con_Printf("Using system GL library\n");

	return true;
}
#endif	// GL_DLSYM


#ifdef GL_DLSYM
static void GL_Init_Functions(void)
{
#define GL_FUNCTION(ret, func, params) \
	func##_fp = (func##_f) SDL_GL_GetProcAddress(#func); \
	if (func##_fp == 0) \
		Sys_Error("%s not found in GL library", #func);
#define GL_FUNCTION_OPT(ret, func, params)
#include "gl_func.h"
#undef	GL_FUNCTION_OPT
#undef	GL_FUNCTION
}
#endif	// GL_DLSYM

static void GL_ResetFunctions(void)
{
#ifdef	GL_DLSYM
#define GL_FUNCTION(ret, func, params) \
	func##_fp = NULL;
#define GL_FUNCTION_OPT(ret, func, params)
#include "gl_func.h"
#undef	GL_FUNCTION_OPT
#undef	GL_FUNCTION
#endif	// GL_DLSYM

	have_stencil = false;

	gl_mtexable = false;
	glActiveTextureARB_fp = NULL;
	glMultiTexCoord2fARB_fp = NULL;

	have8bit = false;
	is8bit = false;
	glColorTableEXT_fp = NULL;
}

/*
===============
GL_Init
===============
*/
static void GL_Init (void)
{
#ifdef GL_DLSYM
	// initialize gl function pointers
	GL_Init_Functions();
#endif
	gl_vendor = (const char *)glGetString_fp (GL_VENDOR);
	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = (const char *)glGetString_fp (GL_RENDERER);
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);

	gl_version = (const char *)glGetString_fp (GL_VERSION);
	Con_Printf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = (const char *)glGetString_fp (GL_EXTENSIONS);
	Con_DPrintf ("GL_EXTENSIONS: %s\n", gl_extensions);

	glGetIntegerv_fp(GL_MAX_TEXTURE_SIZE, &gl_max_size);
	if (gl_max_size < 256)	// Refuse to work when less than 256
		Sys_Error ("hardware capable of min. 256k opengl texture size needed");
	if (gl_max_size > 1024)	// We're cool with 1024, write a cmdline override if necessary
		gl_max_size = 1024;
	Con_Printf("OpenGL max.texture size: %i\n", gl_max_size);

	is_3dfx = false;
	if (!Q_strncasecmp(gl_renderer, "3dfx", 4)	  ||
	    !Q_strncasecmp(gl_renderer, "SAGE Glide", 10) ||
	    !Q_strncasecmp(gl_renderer, "Mesa Glide", 10))
	{
	// This should hopefully detect Voodoo1 and Voodoo2
	// hardware and possibly Voodoo Rush.
	// Voodoo Banshee, Voodoo3 and later are hw-accelerated
	// by DRI in XFree86-4.x and should be: is_3dfx = false.
		Con_Printf("3dfx Voodoo found\n");
		is_3dfx = true;
	}

	if (!Q_strncasecmp(gl_renderer, "PowerVR", 7))
		fullsbardraw = true;	// this actually seems useless, things aren't like those in quake

	CheckMultiTextureExtensions();
	CheckStencilBuffer();

	glClearColor_fp (1,0,0,0);
	glCullFace_fp(GL_FRONT);
	glEnable_fp(GL_TEXTURE_2D);

	glEnable_fp(GL_ALPHA_TEST);
	glAlphaFunc_fp(GL_GREATER, 0.632); // 1 - e^-1 : replaced 0.666 to avoid clipping of smaller fonts/graphics

	glPolygonMode_fp (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel_fp (GL_FLAT);

	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

//	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	/* S.A  -  Replacing the GL_REPEAT parameter to GL_CLAMP 'fixes'
	   the extra lines drawn in rendering fires of Succubus and Praevus.
	   Also see gl_rmain.c in func: R_DrawSpriteModel().  The fix was
	   suggested by Pa3PyX.	 This is actually a workaround only..	*/
	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//	glTexEnvf_fp(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexEnvf_fp(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	if (multisample)
	{
		glEnable_fp (GL_MULTISAMPLE_ARB);
		Con_Printf ("enabled %i sample fsaa\n", multisample);
	}
}

/*
=================
GL_BeginRendering
=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = WRWidth;
	*height = WRHeight;

//	glViewport_fp (*x, *y, *width, *height);
}


void GL_EndRendering (void)
{
	if (!scr_skipupdate)
		SDL_GL_SwapBuffers();

// handle the mouse state when windowed if that's changed
#if 0	// change to 1 if dont want to disable mouse in fullscreen
	if (modestate == MS_WINDOWED)
#endif
		if (_enable_mouse.integer != enable_mouse)
		{
			if (_enable_mouse.integer)
				IN_ActivateMouse ();
			else
				IN_DeactivateMouse ();

			enable_mouse = _enable_mouse.integer;
		}

	if (fullsbardraw)
		Sbar_Changed();
}


int ColorIndex[16] =
{
	0, 31, 47, 63, 79, 95, 111, 127, 143, 159, 175, 191, 199, 207, 223, 231
};

unsigned ColorPercent[16] =
{
	25, 51, 76, 102, 114, 127, 140, 153, 165, 178, 191, 204, 216, 229, 237, 247
};

#if USE_HEXEN2_PALTEX_CODE
// these two procedures should have been used by Raven to
// generate the gfx/invpal.lmp file which resides in pak0

#define	INVERSE_PALNAME	"gfx/invpal.lmp"
static int ConvertTrueColorToPal (unsigned char *true_color, unsigned char *palette)
{
	int	i;
	long	min_dist;
	int	min_index;
	long	r, g, b;

	min_dist = 256 * 256 + 256 * 256 + 256 * 256;
	min_index = -1;
	r = ( long )true_color[0];
	g = ( long )true_color[1];
	b = ( long )true_color[2];

	for (i = 0; i < 256; i++)
	{
		long palr, palg, palb, dist;
		long dr, dg, db;

		palr = palette[3*i];
		palg = palette[3*i+1];
		palb = palette[3*i+2];
		dr = palr - r;
		dg = palg - g;
		db = palb - b;
		dist = dr * dr + dg * dg + db * db;
		if (dist < min_dist)
		{
			min_dist = dist;
			min_index = i;
		}
	}
	return min_index;
}

static void VID_CreateInversePalette (unsigned char *palette)
{
	long	r, g, b;
	long	idx = 0;
	unsigned char	true_color[3];

	Con_Printf ("Creating inverse palette\n");

	for (r = 0; r < ( 1 << INVERSE_PAL_R_BITS ); r++)
	{
		for (g = 0; g < ( 1 << INVERSE_PAL_G_BITS ); g++)
		{
			for (b = 0; b < ( 1 << INVERSE_PAL_B_BITS ); b++)
			{
				true_color[0] = ( unsigned char )( r << ( 8 - INVERSE_PAL_R_BITS ) );
				true_color[1] = ( unsigned char )( g << ( 8 - INVERSE_PAL_G_BITS ) );
				true_color[2] = ( unsigned char )( b << ( 8 - INVERSE_PAL_B_BITS ) );
				inverse_pal[idx] = ConvertTrueColorToPal( true_color, palette );
				idx++;
			}
		}
	}

	FS_CreatePath(va("%s/%s", fs_userdir, INVERSE_PALNAME));
	FS_WriteFile (INVERSE_PALNAME, inverse_pal, sizeof(inverse_pal)-1);
}
#endif	/* USE_HEXEN2_PALTEX_CODE */


void VID_SetPalette (unsigned char *palette)
{
	byte	*pal;
	unsigned short	r, g, b;
	int		v;
	unsigned short	i, p, c;
	unsigned	*table;
#if !USE_HEXEN2_PALTEX_CODE
	int		r1, g1, b1;
	int		j, k, l, m;
	FILE	*f;
	char	s[MAX_OSPATH];
#endif
	static qboolean	been_here = false;

//
// 8 8 8 encoding
//
	pal = palette;
	table = d_8to24table;
	for (i = 0; i < 256; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

#if BYTE_ORDER == BIG_ENDIAN
		v = (255<<0) + (r<<24) + (g<<16) + (b<<8);
#else
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
#endif
		*table++ = v;
	}

#if BYTE_ORDER == BIG_ENDIAN
	d_8to24table[255] &= 0xffffff00;	// 255 is transparent
#else
	d_8to24table[255] &= 0x00ffffff;	// 255 is transparent
#endif

	pal = palette;
	table = d_8to24TranslucentTable;

	for (i = 0; i < 16; i++)
	{
		c = ColorIndex[i] * 3;

		r = pal[c];
		g = pal[c+1];
		b = pal[c+2];

		for (p = 0; p < 16; p++)
		{
#if BYTE_ORDER == BIG_ENDIAN
			v = (ColorPercent[15-p]) + (r<<24) + (g<<16) + (b<<8);
#else
			v = (ColorPercent[15-p]<<24) + (r<<0) + (g<<8) + (b<<16);
#endif
			*table++ = v;

			RTint[i*16+p] = ((float)r) / ((float)ColorPercent[15-p]);
			GTint[i*16+p] = ((float)g) / ((float)ColorPercent[15-p]);
			BTint[i*16+p] = ((float)b) / ((float)ColorPercent[15-p]);
		}
	}

	// Initialize the palettized textures data
	if (been_here)
		return;

#if USE_HEXEN2_PALTEX_CODE
	// This is original hexen2 code for palettized textures
	// Hexenworld replaced it with quake's newer code below
	pal = (byte *) FS_LoadStackFile (INVERSE_PALNAME, inverse_pal, sizeof(inverse_pal));
	if (pal == NULL || pal != inverse_pal)
		VID_CreateInversePalette (palette);

#else // end of HEXEN2_PALTEX_CODE
	FS_OpenFile("glhexen/15to8.pal", &f, true);
	if (f)
	{
		fread(d_15to8table, 1<<15, 1, f);
		fclose(f);
	}
	else
	{	// JACK: 3D distance calcs:
		// k is last closest, l is the distance
		Con_Printf ("Creating 15to8.pal ..");

		// FIXME: Endianness ???
		for (i = 0, m = 0; i < (1<<15); i++, m++)
		{
			/* Maps
			000000000000000
			000000000011111 = Red  = 0x1F
			000001111100000 = Blue = 0x03E0
			111110000000000 = Grn  = 0x7C00
			*/
			r = ((i & 0x1F) << 3) + 4;
			g = ((i & 0x03E0) >> 2) + 4;
			b = ((i & 0x7C00) >> 7) + 4;
#   if 0
			r = (i << 11);
			g = (i << 6);
			b = (i << 1);
			r >>= 11;
			g >>= 11;
			b >>= 11;
#   endif
			pal = (unsigned char *)d_8to24table;
			for (v = 0, k = 0, l = 10000; v < 256; v++, pal += 4)
			{
				r1 = r - pal[0];
				g1 = g - pal[1];
				b1 = b - pal[2];
				j = sqrt( (r1*r1) + (g1*g1) + (b1*b1) );
				if (j < l)
				{
					k = v;
					l = j;
				}
			}
			d_15to8table[i] = k;
			if (m >= 1000)
				m = 0;
		}
		snprintf(s, sizeof(s), "%s/glhexen", fs_userdir);
		Sys_mkdir (s);
		snprintf(s, sizeof(s), "%s/glhexen/15to8.pal", fs_userdir);
		f = fopen(s, "wb");
		if (f)
		{
			fwrite(d_15to8table, 1<<15, 1, f);
			fclose(f);
		}
		Con_Printf(". done\n");
	}
#endif	// end of hexenworld 8_BIT_PALETTE_CODE
	been_here = true;
}


/*
===================================================================

MAIN WINDOW

===================================================================
*/

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
=================
VID_ChangeVideoMode
intended only as a callback for VID_Restart_f
=================
*/
static void VID_ChangeVideoMode(int newmode)
{
	unsigned int	j;
	int	temp;

	if (!screen)
		return;

	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	// restore gamma (just in case), reset gamma function pointers
	VID_ShutdownGamma();
	CDAudio_Pause ();
	MIDI_Pause (MIDI_ALWAYS_PAUSE);
	S_ClearBuffer ();

	// Unload all textures and reset texture counts
	D_ClearOpenGLTextures(0);
	texture_extension_number = 1U;
	lightmap_textures = 0U;
	for (j = 0; j < MAX_LIGHTMAPS; j++)
		lightmap_modified[j] = true;

	// reset all opengl function pointers (just in case)
	GL_ResetFunctions();

	// Avoid re-registering commands and re-allocating memory
	draw_reinit = true;

	// temporarily disable input devices
	IN_DeactivateMouse ();
	IN_ShowMouse ();

	// Kill device and rendering contexts
	SDL_FreeSurface(screen);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);	// also unloads the opengl driver

	// re-init sdl_video, set the mode and re-init opengl
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		Sys_Error ("Couldn't init video: %s", SDL_GetError());
#ifdef GL_DLSYM
	if (!GL_OpenLibrary(gl_library))
		Sys_Error ("Unable to load GL library %s", (gl_library != NULL) ? gl_library : SDL_GetError());
#endif
	VID_SetMode (newmode);
	// re-get the video info since we re-inited sdl_video
	vid_info = SDL_GetVideoInfo();
	GL_Init();
	VID_InitGamma();
	VID_Init8bitPalette();

	// re-init input devices
	IN_Init ();
	ClearAllStates ();
	CDAudio_Resume ();
	MIDI_Pause (MIDI_ALWAYS_RESUME);

	// Reload graphics wad file (Draw_PicFromWad writes glpic_t data (sizes,
	// texnums) right on top of the original pic data, so the pic data will
	// be dirty after gl textures are loaded the first time; we need to load
	// a clean version)
	W_LoadWadFile ("gfx.wad");

	// Reload pre-map pics, fonts, console, etc
	Draw_Init();
	SCR_Init();
	Sbar_Init();
	// Reload the particle texture
	R_InitParticleTexture();
#if defined(H2W)
	R_InitNetgraphTexture();
#endif	/* H2W */

	vid.recalc_refdef = 1;

	// Reload model textures and player skins
	Mod_ReloadTextures();
	// rebuild the lightmaps
	GL_BuildLightmaps();

	// finished reloading all images
	draw_reinit = false;
	scr_disabled_for_loading = temp;

	// apply our gamma
	VID_ShiftPalette(NULL);
}

static void VID_Restart_f (void)
{
	if (vid_mode.integer < 0 || vid_mode.integer >= *nummodes)
	{
		Con_Printf ("Bad video mode %d\n", vid_mode.integer);
		Cvar_SetValue ("vid_mode", vid_modenum);
		return;
	}

	Con_Printf ("Re-initializing video:\n");
	VID_ChangeVideoMode (vid_mode.integer);
}

static int sort_modes (const void *arg1, const void *arg2)
{
	const vmode_t *a1, *a2;
	a1 = (vmode_t *) arg1;
	a2 = (vmode_t *) arg2;

	if (a1->width == a2->width)
		return a1->height - a2->height;	// lowres-to-highres
	//	return a2->height - a1->height;	// highres-to-lowres
	else
		return a1->width - a2->width;	// lowres-to-highres
	//	return a2->width - a1->width;	// highres-to-lowres
}

static void VID_PrepareModes (SDL_Rect **sdl_modes)
{
	int	i, j;
	qboolean	not_multiple;

	num_fmodes = 0;
	num_wmodes = 0;

	// Add the standart 4:3 modes to the windowed modes list
	// In an unlikely case that we receive no fullscreen modes,
	// this will be our modes list (kind of...)
	for (i = 0; i < MAX_STDMODES; i++)
	{
		wmodelist[num_wmodes].width = std_modes[i].width;
		wmodelist[num_wmodes].height = std_modes[i].height;
		wmodelist[num_wmodes].halfscreen = 0;
		wmodelist[num_wmodes].fullscreen = 0;
		wmodelist[num_wmodes].bpp = 16;
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
		num_wmodes = RES_640X480 + 1;
		modelist = wmodelist;
		nummodes = &num_wmodes;
		vid_default = RES_640X480;
		Cvar_SetValue ("vid_config_glx", modelist[vid_default].width);
		Cvar_SetValue ("vid_config_gly", modelist[vid_default].height);
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
		vid_default = RES_640X480;
		Cvar_SetValue ("vid_config_glx", modelist[vid_default].width);
		Cvar_SetValue ("vid_config_gly", modelist[vid_default].height);
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

		// avoid resolutions < 320x240
		if (not_multiple && sdl_modes[i]->w >= MIN_WIDTH && sdl_modes[i]->h >= MIN_HEIGHT)
		{
			fmodelist[num_fmodes].width = sdl_modes[i]->w;
			fmodelist[num_fmodes].height = sdl_modes[i]->h;
			// FIXME: look at gl_vidnt.c and learn how to
			// really functionalize the halfscreen field.
			fmodelist[num_fmodes].halfscreen = 0;
			fmodelist[num_fmodes].fullscreen = 1;
			fmodelist[num_fmodes].bpp = 16;
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

	// SDL versions older than 1.2.8 have sorting problems
	qsort(fmodelist, num_fmodes, sizeof fmodelist[0], sort_modes);

	vid_maxwidth = fmodelist[num_fmodes-1].width;
	vid_maxheight = fmodelist[num_fmodes-1].height;

	// find the 640x480 default resolution. this shouldn't fail
	// at all (for any adapter suporting the VGA/XGA legacy).
	for (i = 0; i < num_fmodes; i++)
	{
		if (fmodelist[i].width == 640 && fmodelist[i].height == 480)
		{
			vid_default = i;
			break;
		}
	}

	if (vid_default < 0)
	{
		// No 640x480? Unexpected, at least today..
		// Easiest thing is to set the default mode
		// as the highest reported one.
		Con_Printf("WARNING: 640x480 not found in fullscreen modes\n"
			   "Using the largest reported dimension as default\n");
		vid_default = num_fmodes;
	}

	// limit the windowed (standart) modes list to desktop dimensions
	for (i = 0; i < num_wmodes; i++)
	{
		if (wmodelist[i].width > vid_maxwidth || wmodelist[i].height > vid_maxheight)
			break;
	}
	if (i < num_wmodes)
		num_wmodes = i;

	Cvar_SetValue ("vid_config_glx", modelist[vid_default].width);
	Cvar_SetValue ("vid_config_gly", modelist[vid_default].height);
}

static void VID_ListModes_f (void)
{
	int	i;

	Con_Printf ("Maximum allowed mode: %d x %d\n", vid_maxwidth, vid_maxheight);
	Con_Printf ("Windowed modes enabled:\n");
	for (i = 0; i < num_wmodes; i++)
		Con_Printf ("%2d:  %d x %d\n", i, wmodelist[i].width, wmodelist[i].height);
	Con_Printf ("Fullscreen modes enumerated:");
	if (num_fmodes)
	{
		Con_Printf ("\n");
		for (i = 0; i < num_fmodes; i++)
			Con_Printf ("%2d:  %d x %d\n", i, fmodelist[i].width, fmodelist[i].height);
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

/*
===================
VID_Init
===================
*/
void	VID_Init (unsigned char *palette)
{
	int	i, temp, width, height;
#if DO_MESH_CACHE
	char	gldir[MAX_OSPATH];
#endif
	SDL_Rect	**enumlist;
	const SDL_version	*sdl_version;
	const char	*read_vars[] = {
				"vid_config_fscr",
				"vid_config_gl8bit",
				"vid_config_fsaa",
				"vid_config_glx",
				"vid_config_gly",
				"vid_config_consize",
				"gl_lightmapfmt" };
#define num_readvars	( sizeof(read_vars)/sizeof(read_vars[0]) )

	Cvar_RegisterVariable (&vid_config_gl8bit);
	Cvar_RegisterVariable (&vid_config_fsaa);
	Cvar_RegisterVariable (&vid_config_fscr);
	Cvar_RegisterVariable (&vid_config_swy);
	Cvar_RegisterVariable (&vid_config_swx);
	Cvar_RegisterVariable (&vid_config_gly);
	Cvar_RegisterVariable (&vid_config_glx);
	Cvar_RegisterVariable (&vid_config_consize);
	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&_enable_mouse);
	Cvar_RegisterVariable (&gl_lightmapfmt);

	Cmd_AddCommand ("vid_showinfo", VID_ShowInfo_f);
	Cmd_AddCommand ("vid_listmodes", VID_ListModes_f);
	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f);
	Cmd_AddCommand ("vid_restart", VID_Restart_f);

	vid.numpages = 2;

#if DO_MESH_CACHE
	// prepare directories for caching mesh files
	snprintf (gldir, sizeof(gldir), "%s/glhexen", fs_userdir);
	Sys_mkdir (gldir);
	snprintf (gldir, sizeof(gldir), "%s/glhexen/boss", fs_userdir);
	Sys_mkdir (gldir);
	snprintf (gldir, sizeof(gldir), "%s/glhexen/puzzle", fs_userdir);
	Sys_mkdir (gldir);
#endif

	// see if the SDL version we linked to is multisampling-capable
	sdl_version = SDL_Linked_Version();
	if (SDL_VERSIONNUM(sdl_version->major,sdl_version->minor,sdl_version->patch) >= SDL_VER_WITH_MULTISAMPLING)
		sdl_has_multisample = true;

	// enable vsync for nvidia geforce or newer - S.A
	if (COM_CheckParm("-sync") || COM_CheckParm("-vsync"))
	{
		setenv("__GL_SYNC_TO_VBLANK", "1", 1);
		Con_Printf ("Nvidia GL vsync enabled\n");
	}

	// set fxMesa mode to fullscreen, don't let it it cheat multitexturing
	setenv ("MESA_GLX_FX","f",1);
	setenv ("FX_DONT_FAKE_MULTITEX","1",1);

	// init sdl
	// the first check is actually unnecessary
	if ( (SDL_WasInit(SDL_INIT_VIDEO)) == 0 )
	{
		if (SDL_Init(SDL_INIT_VIDEO) < 0)
			Sys_Error ("Couldn't init video: %s", SDL_GetError());
	}

#ifdef GL_DLSYM
	i = COM_CheckParm("--gllibrary");
	if (i == 0)
		i = COM_CheckParm ("-gllibrary");
	if (i == 0)
		i = COM_CheckParm ("-g");
	if (i && i < com_argc - 1)
		gl_library = com_argv[i+1];
	else
		gl_library = NULL;	// trust SDL's wisdom here

	// load the opengl library
	if (!GL_OpenLibrary(gl_library))
		Sys_Error ("Unable to load GL library %s", (gl_library != NULL) ? gl_library : SDL_GetError());
#endif

	// this will contain the "best bpp" for the current display
	// make sure to re-retrieve it if you ever re-init sdl_video
	vid_info = SDL_GetVideoInfo();

	// retrieve the list of fullscreen modes
	enumlist = SDL_ListModes(NULL, SDL_OPENGL|SDL_FULLSCREEN);

	i = COM_CheckParm("-bpp");
	if (i && i < com_argc-1)
	{
		bpp = atoi(com_argv[i+1]);
	}

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

	if (vid_config_fscr.integer && !num_fmodes) // FIXME: see below, as well
		Sys_Error ("No fullscreen modes available at this color depth");

	width = vid_config_glx.integer;
	height = vid_config_gly.integer;

	if (vid_config_consize.integer != width)
		vid_conscale = true;

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
		modelist[*nummodes].bpp = 16;
		snprintf (modelist[*nummodes].modedesc, MAX_DESC, "%d x %d (user mode)", width, height);
		Cvar_SetValue ("vid_mode", *nummodes);
		(*nummodes)++;
	}
	else
	{
		Con_Printf ("ignoring invalid -width and/or -height arguments\n");
	}

	if (!vid_conscale)
		Cvar_SetValue ("vid_config_consize", width);

	// This will display a bigger hud and readable fonts at high
	// resolutions. The fonts will be somewhat distorted, though
	i = COM_CheckParm("-conwidth");
	if (i != 0 && i < com_argc-1)
	{
		Cvar_SetValue("vid_config_consize", atoi(com_argv[i+1]));
		if (vid_config_consize.integer != width)
			vid_conscale = true;
	}

	multisample = vid_config_fsaa.integer;
	i = COM_CheckParm ("-fsaa");
	if (i && i < com_argc-1)
		multisample = atoi(com_argv[i+1]);

	if (COM_CheckParm("-paltex"))
		Cvar_SetValue ("vid_config_gl8bit", 1);

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;
	//set the mode
	VID_SetMode (vid_mode.integer);
	ClearAllStates ();

	GL_SetupLightmapFmt(true);
	GL_Init ();
	VID_InitGamma();

	// avoid the 3dfx splash screen on resolution changes
	setenv ("FX_GLIDE_NO_SPLASH","0",1);

	// set our palette
	VID_SetPalette (palette);

	// enable paletted textures
	VID_Init8bitPalette();

	// lock the early-read cvars until Host_Init is finished
	for (i = 0; i < (int)num_readvars; i++)
		Cvar_LockVar (read_vars[i]);

	vid_initialized = true;
	scr_disabled_for_loading = temp;
	vid.recalc_refdef = 1;

	Con_SafePrintf ("Video initialized.\n");

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	if (COM_CheckParm("-fullsbar"))
		fullsbardraw = true;
}


void	VID_Shutdown (void)
{
	VID_ShutdownGamma();
	SDL_Quit();
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

	// This doesn't seem to cause any trouble even
	// with is_3dfx == true and FX_GLX_MESA == f
	if ( SDL_WM_ToggleFullScreen(screen) > 0 )
	{
		is_fullscreen = (screen->flags & SDL_FULLSCREEN) ? 1 : 0;
		Cvar_SetValue("vid_config_fscr", is_fullscreen);
		modestate = (is_fullscreen) ? MS_FULLDIB : MS_WINDOWED;
		if (is_fullscreen)
		{
#if 0	// change to 1 if dont want to disable mouse in fullscreen
			if (!_enable_mouse.integer)
				Cvar_SetValue ("_enable_mouse", 1);
#endif
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


#ifndef H2W
//unused in hexenworld
void D_ShowLoadingSize(void)
{
	if (!vid_initialized)
		return;

	glDrawBuffer_fp (GL_FRONT);

	SCR_DrawLoading();

	glFlush_fp();

	glDrawBuffer_fp (GL_BACK);
}
#endif


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
	VID_MULTISAMPLE,
	VID_PALTEX,
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
static void VID_MenuDraw (void)
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

	need_apply = (vid_menunum != vid_modenum) || want_fstoggle ||
			(have8bit && (is8bit != !!vid_config_gl8bit.integer)) ||
			(multisample != vid_config_fsaa.integer);

	M_Print (76, 92 + 8*VID_FULLSCREEN, "Fullscreen: ");
	M_DrawYesNo (76+12*8, 92 + 8*VID_FULLSCREEN, vid_menu_fs, !want_fstoggle);

	M_Print (76, 92 + 8*VID_RESOLUTION, "Resolution: ");
	if (vid_menunum == vid_modenum)
		M_PrintWhite (76+12*8, 92 + 8*VID_RESOLUTION, modelist[vid_menunum].modedesc);
	else
		M_Print (76+12*8, 92 + 8*VID_RESOLUTION, modelist[vid_menunum].modedesc);

	M_Print (76, 92 + 8*VID_MULTISAMPLE, "Antialiasing  :");
	if (sdl_has_multisample)
	{
		if (multisample == vid_config_fsaa.integer)
			M_PrintWhite (76+16*8, 92 + 8*VID_MULTISAMPLE, va("%d",multisample));
		else
			M_Print (76+16*8, 92 + 8*VID_MULTISAMPLE, va("%d",multisample));
	}
	else
		M_PrintWhite (76+16*8, 92 + 8*VID_MULTISAMPLE, "Not found");

	M_Print (76, 92 + 8*VID_PALTEX, "8 bit textures:");
	if (have8bit)
		M_DrawYesNo (76+16*8, 92 + 8*VID_PALTEX, vid_config_gl8bit.integer, (is8bit == !!vid_config_gl8bit.integer));
	else
		M_PrintWhite (76+16*8, 92 + 8*VID_PALTEX, "Not found");

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
static void VID_MenuKey (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		vid_cursor = (num_fmodes) ? 0 : VID_RESOLUTION;
		M_Menu_Options_f ();
		break;

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

	case K_ENTER:
		switch (vid_cursor)
		{
		case VID_RESET:
			vid_menu_fs = (modestate != MS_WINDOWED);
			vid_menunum = vid_modenum;
			multisample = vid_config_fsaa.integer;
			Cvar_SetValue ("vid_config_gl8bit", is8bit);
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
		case VID_MULTISAMPLE:
			if (!sdl_has_multisample)
				break;
			if (multisample <= 2)
				multisample = 0;
			else if (multisample <= 4)
				multisample = 2;
			else
				multisample = 0;
			break;
		case VID_PALTEX:
			if (have8bit)
				Cvar_SetValue ("vid_config_gl8bit", !vid_config_gl8bit.integer);
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
		case VID_MULTISAMPLE:
			if (!sdl_has_multisample)
				break;
			if (multisample >= 2)
				multisample = 4;
			else if (multisample >= 0)
				multisample = 2;
			else
				multisample = 0;
			break;
		case VID_PALTEX:
			if (have8bit)
				Cvar_SetValue ("vid_config_gl8bit", !vid_config_gl8bit.integer);
			break;
		}
		return;

	default:
		break;
	}
}

