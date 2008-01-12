/*
	gl_vidnt.c -- NT GL vid component
	$Id: gl_vidnt.c,v 1.121 2008-01-12 09:46:17 sezero Exp $
*/

#define	__GL_FUNC_EXTERN

#include "quakedef.h"
#include "winquake.h"
#include "cfgfile.h"
#include "resource.h"
#include "wgl_func.h"
#if !USE_HEXEN2_PALTEX_CODE && !defined(NO_SPLASHES)
#include <commctrl.h>
#endif


#define WARP_WIDTH		320
#define WARP_HEIGHT		200
#define MAXWIDTH		10000
#define MAXHEIGHT		10000
#define MIN_WIDTH		320
//#define MIN_HEIGHT		200
#define MIN_HEIGHT		240
#define MAX_DESC		33
#define MAX_NUMBPP		8

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
static int	bpplist[MAX_NUMBPP][2];

static int	num_fmodes;
static int	num_wmodes;
static int	*nummodes;
static vmode_t	badmode;

#if defined(H2W)
#define WM_CLASSNAME	"HexenWorld"
#define WM_WINDOWNAME	"HexenWorld"
#else
#define WM_CLASSNAME	"HexenII"
#define WM_WINDOWNAME	"HexenII"
#endif

static HGLRC	baseRC;
static HDC	maindc;
static DEVMODE	gdevmode;
static qboolean	classregistered;
HWND		mainwindow;
static HICON	hIcon;
static int	DIBWidth, DIBHeight;
static RECT	WindowRect;
int		window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;
static LONG	WindowStyle, ExWindowStyle;
qboolean	DDActive;

static PIXELFORMATDESCRIPTOR pfd =
{
	sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
	1,				// version number
	PFD_DRAW_TO_WINDOW		// support window
		| PFD_SUPPORT_OPENGL	// support OpenGL
		| PFD_DOUBLEBUFFER ,	// double buffered
	PFD_TYPE_RGBA,			// RGBA type
	24,				// 24-bit color depth
	0, 0, 0, 0, 0, 0,		// color bits ignored
	0,				// no alpha buffer
	0,				// shift bit ignored
	0,				// no accumulation buffer
	0, 0, 0, 0,			// accum bits ignored
	24,				// 24-bit z-buffer
	8,				// 8-bit stencil buffer
	0,				// no auxiliary buffer
	PFD_MAIN_PLANE,			// main layer
	0,				// reserved
	0, 0, 0				// layer masks ignored
};

// main vid functions
static LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void AppActivate(BOOL fActive, BOOL minimize);
static void VID_UpdateWindowStatus (void);
static const char *VID_GetModeDescription (int mode);

// vars for vid state
viddef_t	vid;			// global video state
modestate_t	modestate = MS_UNINIT;
static int	vid_default = MODE_WINDOWED;
static int	vid_modenum = NO_MODE;	// current video mode, set after mode setting succeeds
static int	vid_deskwidth, vid_deskheight, vid_deskbpp, vid_deskmode;
static qboolean	vid_conscale = false;
static char	vid_consize[MAX_DESC];

static qboolean	vid_initialized = false;
static qboolean	vid_canalttab = false;
static qboolean	vid_wassuspended = false;

// cvar vid_mode must be set before calling
// VID_SetMode, VID_ChangeVideoMode or VID_Restart_f
static cvar_t	vid_mode = {"vid_mode", "0", CVAR_NONE};
static cvar_t	vid_config_consize = {"vid_config_consize", "640", CVAR_ARCHIVE};
static cvar_t	vid_config_glx = {"vid_config_glx", "640", CVAR_ARCHIVE};
static cvar_t	vid_config_gly = {"vid_config_gly", "480", CVAR_ARCHIVE};
static cvar_t	vid_config_bpp = {"vid_config_bpp", "16", CVAR_ARCHIVE};
static cvar_t	vid_config_fscr= {"vid_config_fscr", "1", CVAR_ARCHIVE};
// cvars for compatibility with the software version
static cvar_t	vid_wait = {"vid_wait", "-1", CVAR_ARCHIVE};
static cvar_t	vid_maxpages = {"vid_maxpages", "3", CVAR_ARCHIVE};
static cvar_t	vid_nopageflip = {"vid_nopageflip", "1", CVAR_ARCHIVE};
// Note that 0 is MODE_WINDOWED
// Note that 3 is MODE_FULLSCREEN_DEFAULT
static cvar_t	_vid_default_mode = {"_vid_default_mode", "0", CVAR_ARCHIVE};
static cvar_t	_vid_default_mode_win = {"_vid_default_mode_win", "3", CVAR_ARCHIVE};
static cvar_t	vid_stretch_by_2 = {"vid_stretch_by_2", "1", CVAR_ARCHIVE};
static cvar_t	vid_config_x = {"vid_config_x", "800", CVAR_ARCHIVE};
static cvar_t	vid_config_y = {"vid_config_y", "600", CVAR_ARCHIVE};

byte		globalcolormap[VID_GRADES*256];
float		RTint[256], GTint[256], BTint[256];
unsigned short	d_8to16table[256];
unsigned int	d_8to24table[256];
unsigned int	d_8to24TranslucentTable[256];
#if USE_HEXEN2_PALTEX_CODE
unsigned char	*inverse_pal;
#else
unsigned char	d_15to8table[65536];
#endif

// gl stuff
static void GL_Init (void);

#ifdef GL_DLSYM
HINSTANCE	hInstGL;
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
typedef BOOL	(APIENTRY *GAMMA_RAMP_FN)(HDC, LPVOID);
static GAMMA_RAMP_FN	GetDeviceGammaRamp_f;
static GAMMA_RAMP_FN	SetDeviceGammaRamp_f;
extern unsigned short	ramps[3][256];	// for hw- or 3dfx-gamma
static unsigned short	orig_ramps[3][256];	// for hw- or 3dfx-gamma
static qboolean	gammaworks = false;	// whether hw-gamma works
qboolean	gl_dogamma = false;

// multitexturing
qboolean	gl_mtexable = false;
static GLint	num_tmus = 1;

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
cvar_t		_enable_mouse = {"_enable_mouse", "0", CVAR_ARCHIVE};


//====================================

void VID_HandlePause (qboolean paused)
{

	if ((modestate == MS_WINDOWED) && _enable_mouse.integer)
	{
		if (paused)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
		else
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
	}
}


//====================================

static void CenterWindow (HWND hWndCenter, int width, int height, BOOL lefttopjustify)
{
	int	CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	if (CenterX > CenterY*2)
		CenterX >>= 1;	// dual screens
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;
	SetWindowPos (hWndCenter, NULL, CenterX, CenterY, 0, 0,
			SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
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
	q_snprintf (vid_consize, sizeof(vid_consize), "x%.2f (at %ux%u)",
			(float)modelist[vid_modenum].width/vid.conwidth, vid.conwidth, vid.conheight);
}

void VID_ChangeConsize (int key)
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

	q_snprintf (vid_consize, sizeof(vid_consize), "x%.2f (at %ux%u)",
			(float)modelist[vid_modenum].width/vid.conwidth, vid.conwidth, vid.conheight);
}

char *VID_ReportConsize (void)
{
	return vid_consize;
}


static qboolean VID_SetWindowedMode (int modenum)
{
	int	width, height;
	RECT	rect;

	// Pa3PyX: set the original fullscreen mode if
	// we are switching to window from fullscreen.
	if (modestate == MS_FULLDIB)
		ChangeDisplaySettings(NULL, 0);

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	mainwindow = CreateWindowEx (ExWindowStyle, WM_CLASSNAME, WM_WINDOWNAME, WindowStyle,
					rect.left, rect.top, width, height, NULL, NULL, global_hInstance, NULL);
	if (!mainwindow)
		Sys_Error ("Couldn't create DIB window");

	// center the DIB window
	CenterWindow(mainwindow, WindowRect.right - WindowRect.left,
				 WindowRect.bottom - WindowRect.top, false);

	modestate = MS_WINDOWED;
	Cvar_SetValue ("vid_config_fscr", 0);

	return true;
}


static qboolean VID_SetFullDIBMode (int modenum)
{
	int	width, height;
	RECT	rect;

	pfd.cColorBits = modelist[modenum].bpp;

	gdevmode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
	if (!Win95old)
	{
		gdevmode.dmFields |= DM_BITSPERPEL;
		gdevmode.dmBitsPerPel = modelist[modenum].bpp;
	}
	gdevmode.dmPelsWidth = modelist[modenum].width << modelist[modenum].halfscreen;
	gdevmode.dmPelsHeight = modelist[modenum].height;
	gdevmode.dmSize = sizeof (gdevmode);

	if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
		Sys_Error ("Couldn't set fullscreen DIB mode");

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	WindowStyle = WS_POPUP | WS_SYSMENU | WS_VISIBLE;
	ExWindowStyle = WS_EX_TOPMOST;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	mainwindow = CreateWindowEx (ExWindowStyle, WM_CLASSNAME, WM_WINDOWNAME, WindowStyle,
					rect.left, rect.top, width, height, NULL, NULL, global_hInstance, NULL);
	if (!mainwindow)
		Sys_Error ("Couldn't create DIB window");

	modestate = MS_FULLDIB;
	Cvar_SetValue ("vid_config_fscr", 1);

// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = 0;
	window_y = 0;

	return true;
}


static int VID_SetMode (int modenum, unsigned char *palette)
{
	qboolean	stat = false;
	MSG		msg;
	HDC		hdc;

	if (modenum < 0 || modenum >= *nummodes)
		Sys_Error ("Bad video mode\n");

	CDAudio_Pause ();

	// Set either the fullscreen or windowed mode
	if (modelist[modenum].type == MS_WINDOWED)
	{
		if (_enable_mouse.integer)
		{
			stat = VID_SetWindowedMode(modenum);
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
		else
		{
			stat = VID_SetWindowedMode(modenum);
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
	}
	else if (modelist[modenum].type == MS_FULLDIB)
	{
		stat = VID_SetFullDIBMode(modenum);
		IN_ActivateMouse ();
		IN_HideMouse ();
	}
	else
	{
		Sys_Error ("%s: Bad mode type in modelist", __thisfunc__);
	}

	if (!stat)
	{
		Sys_Error ("Couldn't set video mode");
	}

	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	// Because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop),
	// we clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC(mainwindow);
	PatBlt(hdc, 0, 0, WindowRect.right, WindowRect.bottom, BLACKNESS);
	ReleaseDC(mainwindow, hdc);

	vid.numpages = 2;
	vid.width  = vid.conwidth  = modelist[modenum].width;
	vid.height = vid.conheight = modelist[modenum].height;

	// setup the effective console width
	VID_ConWidth(modenum);

	window_width = DIBWidth;
	window_height = DIBHeight;
	VID_UpdateWindowStatus ();

// now we try to make sure we get the focus on the mode switch, because
// sometimes in some systems we don't.  We grab the foreground, then
// finish setting up, pump all our messages, and sleep for a little while
// to let messages finish bouncing around the system, then we put
// ourselves at the top of the z order, then grab the foreground again,
// Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);
	//VID_SetPalette (palette);
	vid_modenum = modenum;
	Cvar_SetValue ("vid_config_glx", modelist[vid_modenum].width);
	Cvar_SetValue ("vid_config_gly", modelist[vid_modenum].height);
	if (modestate != MS_WINDOWED)
		Cvar_SetValue ("vid_config_bpp", modelist[vid_modenum].bpp);

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	Sleep (100);

	SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0,
				  SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
				  SWP_NOCOPYBITS);

	SetForegroundWindow (mainwindow);

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	//VID_SetPalette (palette);
	//vid.recalc_refdef = 1;

	CDAudio_Resume ();

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
		glColorTableEXT_fp = (glColorTableEXT_f)wglGetProcAddress_fp("glColorTableEXT");
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
		Con_SafePrintf("8-bit palettized textures enabled\n");
	}
}

static void VID_Check3dfxGamma (void)
{
	if (!strstr(gl_extensions, "WGL_3DFX_gamma_control"))
	{
		GetDeviceGammaRamp_f = GetDeviceGammaRamp;
		SetDeviceGammaRamp_f = SetDeviceGammaRamp;
	}
	else
	{
		GetDeviceGammaRamp_f = (GAMMA_RAMP_FN) wglGetProcAddress_fp("wglGetDeviceGammaRamp3DFX");
		SetDeviceGammaRamp_f = (GAMMA_RAMP_FN) wglGetProcAddress_fp("wglSetDeviceGammaRamp3DFX");
		if (GetDeviceGammaRamp_f && SetDeviceGammaRamp_f)
			Con_SafePrintf("Using 3Dfx specific gamma control\n");
		else
		{
			GetDeviceGammaRamp_f = GetDeviceGammaRamp;
			SetDeviceGammaRamp_f = SetDeviceGammaRamp;
		}
	}
}

static void VID_InitGamma (void)
{
	GetDeviceGammaRamp_f = NULL;
	SetDeviceGammaRamp_f = NULL;

	VID_Check3dfxGamma ();

	if (GetDeviceGammaRamp_f)
		gammaworks = GetDeviceGammaRamp_f(maindc, orig_ramps);
	else
		gammaworks = false;

	if (!gammaworks)
	{
		// we can still adjust the brightness...
		Con_SafePrintf("gamma not available, using gl tricks\n");
		gl_dogamma = true;
	}
}

void VID_ShiftPalette (unsigned char *palette)
{
	if (gammaworks && SetDeviceGammaRamp_f)
		SetDeviceGammaRamp_f (maindc, ramps);
}


static void CheckMultiTextureExtensions (void)
{
	gl_mtexable = false;

	if (COM_CheckParm("-nomtex"))
	{
		Con_SafePrintf("Multitexture extensions disabled\n");
	}
	else if (strstr(gl_extensions, "GL_ARB_multitexture"))
	{
		Con_SafePrintf("ARB Multitexture extensions found\n");

		glGetIntegerv_fp(GL_MAX_TEXTURE_UNITS_ARB, &num_tmus);
		if (num_tmus < 2)
		{
			Con_SafePrintf("not enough TMUs, ignoring multitexture\n");
			return;
		}

		glMultiTexCoord2fARB_fp = (glMultiTexCoord2fARB_f) wglGetProcAddress_fp("glMultiTexCoord2fARB");
		glActiveTextureARB_fp = (glActiveTextureARB_f) wglGetProcAddress_fp("glActiveTextureARB");
		if ((glMultiTexCoord2fARB_fp == NULL) ||
		    (glActiveTextureARB_fp == NULL))
		{
			Con_SafePrintf ("Couldn't link to multitexture functions\n");
			return;
		}

		Con_SafePrintf("Found %i TMUs support\n", num_tmus);
		gl_mtexable = true;

		// start up with the correct texture selected!
		glDisable_fp(GL_TEXTURE_2D);
		glActiveTextureARB_fp(GL_TEXTURE0_ARB);
	}
	else
	{
		Con_SafePrintf("GL_ARB_multitexture not found\n");
	}
}

static void CheckStencilBuffer (void)
{
	have_stencil = false;

	if (pfd.cStencilBits)
	{
		Con_SafePrintf("Stencil buffer created with %d bits\n", pfd.cStencilBits);
		have_stencil = true;
	}
}


#ifdef GL_DLSYM
static qboolean GL_OpenLibrary (const char *name)
{
	Con_SafePrintf("Loading OpenGL library %s\n", name);

	// open the library
	if (!(hInstGL = LoadLibrary(name)))
	{
		Con_SafePrintf("Unable to LoadLibrary %s\n", name);
		return false;
	}

	// link to necessary wgl functions
#define GL_FUNCTION(ret, func, params) \
	func##_fp = (func##_f) GetProcAddress(hInstGL, #func); \
	if (func##_fp == 0) \
		Sys_Error("Couldn't link to %s", #func);
#define GL_FUNCTION_OPT(ret, func, params)
#include "wgl_func.h"
#undef	GL_FUNCTION_OPT
#undef	GL_FUNCTION

	return true;
}

static void GL_CloseLibrary (void)
{
	// clear the wgl function pointers
#define GL_FUNCTION(ret, func, params) \
	func##_fp = NULL;
#define GL_FUNCTION_OPT(ret, func, params)
#include "wgl_func.h"
#undef	GL_FUNCTION_OPT
#undef	GL_FUNCTION

	// free the library
	if (hInstGL != NULL)
		FreeLibrary(hInstGL);
	hInstGL = NULL;
}

static void GL_Init_Functions (void)
{
#define GL_FUNCTION(ret, func, params) \
	func##_fp = (func##_f) GetProcAddress(hInstGL, #func); \
	if (func##_fp == 0) \
		Sys_Error("%s not found in GL library", #func);
#define GL_FUNCTION_OPT(ret, func, params)
#include "gl_func.h"
#undef	GL_FUNCTION_OPT
#undef	GL_FUNCTION
}
#endif	// GL_DLSYM

static void GL_ResetFunctions (void)
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

	GetDeviceGammaRamp_f = NULL;
	SetDeviceGammaRamp_f = NULL;
}


/*
===============
GL_Init
===============
*/
static void GL_Init (void)
{
	PIXELFORMATDESCRIPTOR	new_pfd;

	Con_SafePrintf ("Video mode %s initialized\n", VID_GetModeDescription (vid_modenum));
	// DescribePixelFormat fails with old 3dfx minigl drivers: don't Sys_Error
	if (DescribePixelFormat(maindc, GetPixelFormat(maindc), sizeof(PIXELFORMATDESCRIPTOR), &new_pfd))
	{
		Con_SafePrintf("Pixel format: c: %d, z: %d, s: %d\n",
			new_pfd.cColorBits, new_pfd.cDepthBits, new_pfd.cStencilBits);
		if ((new_pfd.dwFlags & PFD_GENERIC_FORMAT) && !(new_pfd.dwFlags & PFD_GENERIC_ACCELERATED))
			Con_SafePrintf ("WARNING: Hardware acceleration not present\n");
		else if (new_pfd.dwFlags & PFD_GENERIC_ACCELERATED)
			Con_SafePrintf ("OpenGL: MCD acceleration found\n");
	}

#ifdef GL_DLSYM
	// initialize gl function pointers
	GL_Init_Functions();
#endif
	gl_vendor = (const char *)glGetString_fp (GL_VENDOR);
	Con_SafePrintf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = (const char *)glGetString_fp (GL_RENDERER);
	Con_SafePrintf ("GL_RENDERER: %s\n", gl_renderer);

	gl_version = (const char *)glGetString_fp (GL_VERSION);
	Con_SafePrintf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = (const char *)glGetString_fp (GL_EXTENSIONS);
	Con_SafeDPrintf ("GL_EXTENSIONS: %s\n", gl_extensions);

	glGetIntegerv_fp(GL_MAX_TEXTURE_SIZE, &gl_max_size);
	if (gl_max_size < 256)	// Refuse to work when less than 256
		Sys_Error ("hardware capable of min. 256k opengl texture size needed");
	if (gl_max_size > 1024)	// We're cool with 1024, write a cmdline override if necessary
		gl_max_size = 1024;
	Con_SafePrintf("OpenGL max.texture size: %i\n", gl_max_size);

	is_3dfx = false;
	if (!q_strncasecmp(gl_renderer, "3dfx", 4)	  ||
	    !q_strncasecmp(gl_renderer, "SAGE Glide", 10) ||
	    !q_strncasecmp(gl_renderer, "Mesa Glide", 10))
	{
		Con_SafePrintf("3dfx Voodoo found\n");
		is_3dfx = true;
	}

	if (!q_strncasecmp(gl_renderer, "PowerVR", 7))
		fullsbardraw = true;	// this actually seems useless, things aren't like those in quake

	VID_InitGamma ();

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

	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//	glTexEnvf_fp(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexEnvf_fp(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}


/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = WindowRect.right - WindowRect.left;
	*height = WindowRect.bottom - WindowRect.top;

//	if (!wglMakeCurrent_fp( maindc, baseRC ))
//		Sys_Error ("wglMakeCurrent failed");

//	glViewport_fp (*x, *y, *width, *height);
}


void GL_EndRendering (void)
{
	if (!scr_skipupdate)
		SwapBuffers(maindc);

// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED)
	{
		if (_enable_mouse.integer != enable_mouse)
		{
			if (_enable_mouse.integer)
			{
				IN_ActivateMouse ();
				IN_HideMouse ();
			}
			else
			{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}

			enable_mouse = _enable_mouse.integer;
		}
	}
	if (fullsbardraw)
		Sbar_Changed();
}


int ColorIndex[16] =
{
	0, 31, 47, 63, 79, 95, 111, 127, 143, 159, 175, 191, 199, 207, 223, 231
};

unsigned int ColorPercent[16] =
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
	r = (long) true_color[0];
	g = (long) true_color[1];
	b = (long) true_color[2];

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

	Con_SafePrintf ("Creating inverse palette\n");

	for (r = 0; r < (1 << INVERSE_PAL_R_BITS); r++)
	{
		for (g = 0; g < (1 << INVERSE_PAL_G_BITS); g++)
		{
			for (b = 0; b < (1 << INVERSE_PAL_B_BITS); b++)
			{
				true_color[0] = (unsigned char)(r << (8 - INVERSE_PAL_R_BITS));
				true_color[1] = (unsigned char)(g << (8 - INVERSE_PAL_G_BITS));
				true_color[2] = (unsigned char)(b << (8 - INVERSE_PAL_B_BITS));
				inverse_pal[idx] = ConvertTrueColorToPal(true_color, palette);
				idx++;
			}
		}
	}

	FS_CreatePath(va("%s/%s", fs_userdir, INVERSE_PALNAME));
	FS_WriteFile (INVERSE_PALNAME, inverse_pal, INVERSE_PAL_SIZE);
}
#else	/* USE_HEXEN2_PALTEX_CODE */
static void VID_Create8bitPalette (void)
{
	byte	*pal;
	unsigned short	r, g, b;
	int		v;
	unsigned short	i;
	int		r1, g1, b1;
	int		j, k, l, m;
	FILE	*f;
	char	s[MAX_OSPATH];
#if !defined(NO_SPLASHES)
	HWND	hDlg, hProgress;

	hDlg = CreateDialog(global_hInstance, MAKEINTRESOURCE(IDD_PROGRESS), NULL, NULL);
	hProgress = GetDlgItem(hDlg, IDC_PROGRESS);
	SendMessage(hProgress, PBM_SETSTEP, 1, 0);
	SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 33));
#endif

	// FIXME: Endianness ???

	// JACK: 3D distance calcs:
	// k is last closest, l is the distance
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
		{
#if !defined(NO_SPLASHES)
#ifdef DEBUG_BUILD
			q_snprintf(s, sizeof(s), "Done - %d\n", i);
			OutputDebugString(s);
#endif
			SendMessage(hProgress, PBM_STEPIT, 0, 0);
#endif
			m = 0;
		}
	}
	q_snprintf(s, sizeof(s), "%s/glhexen", fs_userdir);
	Sys_mkdir (s);
	q_snprintf(s, sizeof(s), "%s/glhexen/15to8.pal", fs_userdir);
	f = fopen(s, "wb");
	if (f)
	{
		fwrite(d_15to8table, 1<<15, 1, f);
		fclose(f);
	}
#if !defined(NO_SPLASHES)
	DestroyWindow(hDlg);
#endif
}
#endif	/* USE_HEXEN2_PALTEX_CODE */


void VID_SetPalette (unsigned char *palette)
{
	byte	*pal;
	unsigned short	r, g, b;
	unsigned short	i, p, c;
	unsigned int	v, *table;
	size_t		palsize;
#if !USE_HEXEN2_PALTEX_CODE
	FILE	*f;
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

		v = (255 << SHIFT_a) + (r << SHIFT_r) + (g << SHIFT_g) + (b << SHIFT_b);
		*table++ = v;
	}

	d_8to24table[255] &= MASK_rgb;	// 255 is transparent

	pal = palette;
	table = d_8to24TranslucentTable;

	for (i = 0; i < 16; i++)
	{
		c = ColorIndex[i] * 3;

		r = pal[c];
		g = pal[c + 1];
		b = pal[c + 2];

		for (p = 0; p < 16; p++)
		{
			v = (ColorPercent[15 - p] << SHIFT_a) + (r << SHIFT_r) + (g << SHIFT_g) + (b << SHIFT_b);
			*table++ = v;

			RTint[i*16+p] = ((float)r) / ((float)ColorPercent[15-p]);
			GTint[i*16+p] = ((float)g) / ((float)ColorPercent[15-p]);
			BTint[i*16+p] = ((float)b) / ((float)ColorPercent[15-p]);
		}
	}

	// Initialize the palettized textures data
	if (been_here)
		return;
	been_here = true;

#if USE_HEXEN2_PALTEX_CODE
	// This is original hexen2 code for palettized textures
	// Hexenworld replaced it with quake(world)'s code below
	inverse_pal = (unsigned char *) Hunk_AllocName (INVERSE_PAL_SIZE + 1, INVERSE_PALNAME);
	palsize = INVERSE_PAL_SIZE;
	pal = (byte *) FS_LoadBufFile (INVERSE_PALNAME, inverse_pal, &palsize);
	if (pal != inverse_pal || palsize != INVERSE_PAL_SIZE)
		VID_CreateInversePalette (palette);

#else /* end of HEXEN2_PALTEX_CODE */
	palsize = FS_OpenFile("glhexen/15to8.pal", &f, true);
	if (f && palsize == (1<<15))
	{
		fread(d_15to8table, 1<<15, 1, f);
		fclose(f);
	}
	else
	{
		if (f)
			fclose(f);
		VID_Create8bitPalette ();
	}
#endif	/* end of hexenworld 8_BIT_PALETTE_CODE */
}

//==========================================================================


static BOOL bSetupPixelFormat (HDC hDC)
{
	int	pixelformat;

	if ( (pixelformat = ChoosePixelFormat(hDC, &pfd)) == 0 )
	{
		MessageBox(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
		return FALSE;
	}

	if (SetPixelFormat(hDC, pixelformat, &pfd) == FALSE)
	{
		MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
		return FALSE;
	}

	return TRUE;
}



static byte scantokey[128] =
{
//	0        1       2       3       4       5       6       7
//	8        9       A       B       C       D       E       F
	0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6',
	'7',    '8',    '9',    '0',    '-',    '=', K_BACKSPACE, 9,	// 0
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
	'o',    'p',    '[',    ']',     13,   K_CTRL,  'a',    's',	// 1
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
	'\'',   '`',  K_SHIFT,  '\\',   'z',    'x',    'c',    'v',	// 2
	'b',    'n',    'm',    ',',    '.',    '/',  K_SHIFT,  '*',
	K_ALT,  ' ',     0 ,    K_F1,   K_F2,   K_F3,   K_F4,  K_F5,	// 3
	K_F6,  K_F7,   K_F8,    K_F9,  K_F10, K_PAUSE,   0 , K_HOME,
	K_UPARROW,K_PGUP,'-',K_LEFTARROW,'5',K_RIGHTARROW,'+',K_END,	// 4
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,   0 ,    0 ,     0 ,  K_F11,
	K_F12,   0 ,     0 ,     0 ,      0 ,    0 ,     0 ,     0 ,	// 5
	0  ,     0 ,     0 ,     0 ,      0 ,    0 ,     0 ,     0 ,
	0  ,     0 ,     0 ,     0 ,      0 ,    0 ,     0 ,     0 ,	// 6
	0  ,     0 ,     0 ,     0 ,      0 ,    0 ,     0 ,     0 ,
	0  ,     0 ,     0 ,     0 ,      0 ,    0 ,     0 ,     0	// 7
};

#if 0	/* not used */
static byte shiftscantokey[128] =
{
//	0       1       2       3       4       5       6       7
//	8       9       A       B       C       D       E       F
	0  ,    27,     '!',    '@',    '#',    '$',    '%',    '^',
	'&',    '*',    '(',    ')',    '_',    '+', K_BACKSPACE, 9,	// 0
	'Q',    'W',    'E',    'R',    'T',    'Y',    'U',    'I',
	'O',    'P',    '{',    '}',    13 ,   K_CTRL,  'A',    'S',	// 1
	'D',    'F',    'G',    'H',    'J',    'K',    'L',    ':',
	'"' ,    '~', K_SHIFT,  '|',    'Z',    'X',    'C',    'V',	// 2
	'B',    'N',    'M',    '<',    '>',    '?',  K_SHIFT,  '*',
	K_ALT,  ' ',     0 ,    K_F1,   K_F2,   K_F3,   K_F4,  K_F5,	// 3
	K_F6,  K_F7,   K_F8,    K_F9,  K_F10, K_PAUSE,   0 , K_HOME,
	K_UPARROW,K_PGUP,'_',K_LEFTARROW,'%',K_RIGHTARROW,'+',K_END,	// 4
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,   0 ,    0 ,     0 ,  K_F11,
	K_F12,   0 ,     0 ,     0 ,      0 ,    0 ,     0 ,     0 ,	// 5
	0  ,     0 ,     0 ,     0 ,      0 ,    0 ,     0 ,     0 ,
	0  ,     0 ,     0 ,     0 ,      0 ,    0 ,     0 ,     0 ,	// 6
	0  ,     0 ,     0 ,     0 ,      0 ,    0 ,     0 ,     0 ,
	0  ,     0 ,     0 ,     0 ,      0 ,    0 ,     0 ,     0	// 7
};
#endif

/*
=======
MapKey

Map from windows to quake keynums
=======
*/
static int MapKey (int key)
{
	key = (key >> 16) & 255;
	if (key > 127)
		return 0;
	if (scantokey[key] == 0)
		Con_DPrintf("key 0x%02x has no translation\n", key);
	return scantokey[key];
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
================
VID_UpdateWindowStatus
================
*/
static void VID_UpdateWindowStatus (void)
{
	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}


static void AppActivate (BOOL fActive, BOOL minimize)
/****************************************************************************
*
* Function:	AppActivate
* Parameters:	fActive - True if app is activating
*
* Description:	If the application is activating, then swap the system
*		into SYSPAL_NOSTATIC mode so that our palettes will display
*		correctly.
*
****************************************************************************/
{
	static BOOL	sound_active;

	ActiveApp = fActive;
	Minimized = minimize;

// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active)
	{
		S_BlockSound ();
		sound_active = false;
	}
	else if (ActiveApp && !sound_active)
	{
		S_UnblockSound ();
		sound_active = true;
	}

	if (fActive)
	{
		if (modestate == MS_FULLDIB)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
			if (vid_canalttab && vid_wassuspended)
			{
				vid_wassuspended = false;
				ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN);
				ShowWindow(mainwindow, SW_SHOWNORMAL);
				// Fix for alt-tab bug in NVidia drivers, from quakeforge
				MoveWindow(mainwindow, 0, 0, gdevmode.dmPelsWidth,
						gdevmode.dmPelsHeight, false);
			}
		}
		else if (modestate == MS_WINDOWED && _enable_mouse.integer)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
		VID_ShiftPalette(NULL);
	}

	if (!fActive)
	{
		if (maindc && gammaworks && SetDeviceGammaRamp_f)
		{
			SetDeviceGammaRamp_f(maindc, orig_ramps);
		}
		if (modestate == MS_FULLDIB)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			if (vid_canalttab)
			{ 
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		}
		else if (modestate == MS_WINDOWED && _enable_mouse.integer)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
	}
}


static int	MWheelAccumulator;
static UINT	uMSG_MOUSEWHEEL;
extern cvar_t	mwheelthreshold;

/* main window procedure */
static LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LONG	lRet = 0;
	int	fActive, fMinimized, temp;

	if (Win95 && mwheelthreshold.integer >= 1)
	{
		if (uMSG_MOUSEWHEEL && uMsg == uMSG_MOUSEWHEEL)
		{	/* http://msdn2.microsoft.com/en-us/library/ms645617.aspx
			   Win95 and WinNT-3.51 code using MSH_MOUSEWHEEL.	*/
			MWheelAccumulator += (int) wParam;
			while (MWheelAccumulator >= mwheelthreshold.integer)
			{
				Key_Event(K_MWHEELUP, true);
				Key_Event(K_MWHEELUP, false);
				MWheelAccumulator -= mwheelthreshold.integer;
			}
			while (MWheelAccumulator <= -mwheelthreshold.integer)
			{
				Key_Event(K_MWHEELDOWN, true);
				Key_Event(K_MWHEELDOWN, false);
				MWheelAccumulator += mwheelthreshold.integer;
			}
			return DefWindowProc (hWnd, uMsg, wParam, lParam);
		}
	}

	switch (uMsg)
	{
	case WM_KILLFOCUS:
		if (modestate == MS_FULLDIB)
			ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
		break;

	case WM_CREATE:
		if (Win95)
		{
			uMSG_MOUSEWHEEL = RegisterWindowMessage("MSWHEEL_ROLLMSG");
			if (!uMSG_MOUSEWHEEL)
				Con_SafePrintf ("couldn't register mousewheel\n");
		}
		break;

	case WM_MOVE:
		// ignore when fullscreen
		if (modestate == MS_FULLDIB)
			break;
		window_x = (int) LOWORD(lParam);
		window_y = (int) HIWORD(lParam);
		VID_UpdateWindowStatus ();
		break;

	case WM_SIZE:
		break;

	case WM_SYSCHAR:
		// keep Alt-Space from happening
		break;

	case WM_ACTIVATE:
		fActive = LOWORD(wParam);
		fMinimized = (BOOL) HIWORD(wParam);
		AppActivate(!(fActive == WA_INACTIVE), fMinimized);

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
		ClearAllStates ();

		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		Key_Event (MapKey(lParam), true);
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		Key_Event (MapKey(lParam), false);
		break;

	// this is complicated because Win32 seems to pack multiple mouse
	// events into one update sometimes, so we always check all states
	// and look for events
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_MOUSEMOVE:
		temp = 0;

		if (wParam & MK_LBUTTON)
			temp |= 1;

		if (wParam & MK_RBUTTON)
			temp |= 2;

		if (wParam & MK_MBUTTON)
			temp |= 4;

		// intellimouse explorer
		if (wParam & MK_XBUTTON1)
			temp |= 8;

		if (wParam & MK_XBUTTON2)
			temp |= 16;

		IN_MouseEvent (temp);

		break;

	case WM_MOUSEWHEEL:
		if ((short) HIWORD(wParam) > 0)
		{
			Key_Event(K_MWHEELUP, true);
			Key_Event(K_MWHEELUP, false);
		}
		else
		{
			Key_Event(K_MWHEELDOWN, true);
			Key_Event(K_MWHEELDOWN, false);
		}
		return 0;

	case WM_CLOSE:
		if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit",
					MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
		{
			Sys_Quit ();
		}
		break;

	case WM_DESTROY:
	//	PostQuitMessage (0);
		break;

	case MM_MCINOTIFY:
#if !defined(_NO_CDAUDIO)
		lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
#endif	/* ! _NO_CDAUDIO */
		break;

	default:
		/* pass all unhandled messages to DefWindowProc */
		lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
		break;
	}

	/* return 1 if handled message, 0 if not */
	return lRet;
}


/*
=================
VID_NumModes
=================
*/
static int VID_NumModes (void)
{
	return *nummodes;
}


/*
=================
VID_GetModePtr
=================
*/
static vmode_t *VID_GetModePtr (int modenum)
{
	if ((modenum >= 0) && (modenum < *nummodes))
		return &modelist[modenum];
	else
		return &badmode;
}


/*
=================
VID_GetModeDescription
=================
*/
static const char *VID_GetModeDescription (int mode)
{
	const char	*pinfo;
	vmode_t		*pv;

	if ((mode < 0) || (mode >= *nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	pinfo = pv->modedesc;

	return pinfo;
}


// KJB: Added this to return the mode driver name in description for console

static const char *VID_GetExtModeDescription (int mode)
{
	static char	pinfo[100];
	vmode_t		*pv;

	if ((mode < 0) || (mode >= *nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	if (modelist[mode].type == MS_FULLDIB)
	{
		q_snprintf(pinfo, sizeof(pinfo), "%s fullscreen", pv->modedesc);
	}
	else
	{
		if (modestate == MS_WINDOWED)
			q_snprintf(pinfo, sizeof(pinfo), "%s windowed", pv->modedesc);
		else
			q_snprintf(pinfo, sizeof(pinfo), "windowed");
	}

	return pinfo;
}


/*
=================
VID_DescribeCurrentMode_f
=================
*/
static void VID_DescribeCurrentMode_f (void)
{
	Con_Printf ("%s\n", VID_GetExtModeDescription (vid_modenum));
}


/*
=================
VID_NumModes_f
=================
*/
static void VID_NumModes_f (void)
{
	if (*nummodes == 1)
		Con_Printf ("1 video mode is available\n");
	else
		Con_Printf ("%d video modes are available\n", *nummodes);
}


/*
=================
VID_DescribeMode_f
=================
*/
static void VID_DescribeMode_f (void)
{
	int	modenum;

	modenum = atoi (Cmd_Argv(1));

	Con_Printf ("%s\n", VID_GetExtModeDescription (modenum));
}

/*
=================
VID_DescribeModes_f
=================
*/
static void VID_DescribeModes_f (void)
{
	int	i, lnummodes;
	const char	*pinfo;
	vmode_t	*pv;

	lnummodes = VID_NumModes ();

	for (i = 0; i < lnummodes; i++)
	{
		pv = VID_GetModePtr (i);
		pinfo = VID_GetExtModeDescription (i);
		Con_Printf ("%2d: %s\n", i, pinfo);
	}
}


static void VID_RegisterWndClass (HINSTANCE hInstance)
{
	WNDCLASS	wc;

	wc.style		= 0;
	wc.lpfnWndProc		= (WNDPROC)MainWndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 0;
	wc.hInstance		= hInstance;
	wc.hIcon		= 0;
	wc.hCursor		= LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground	= NULL;
	wc.lpszMenuName		= 0;
	wc.lpszClassName	= WM_CLASSNAME;

	if (!RegisterClass(&wc))
		Sys_Error ("Couldn't register main window class");

	classregistered = true;
}

static void VID_InitDIB (HINSTANCE hInstance)
{
	int		i;
	HDC	tempDC;

	// get desktop settings, hope that the user don't do a silly
	// thing like going back to desktop and changing its settings
	//vid_deskwidth = GetSystemMetrics (SM_CXSCREEN);
	//vid_deskheight = GetSystemMetrics (SM_CXSCREEN);
	tempDC = GetDC (GetDesktopWindow());
	vid_deskwidth = GetDeviceCaps (tempDC, HORZRES);
	vid_deskheight = GetDeviceCaps(tempDC, VERTRES);
	vid_deskbpp = GetDeviceCaps (tempDC, BITSPIXEL);
	ReleaseDC (GetDesktopWindow(), tempDC);

	// refuse to run if vid_deskbpp < 15
	if (vid_deskbpp < 15)
		Sys_Error ("Desktop color depth too low\n"
			   "Make sure you are running at 16 bpp or better");

	/* Register the frame class */
	VID_RegisterWndClass(hInstance);

	// initialize standart windowed modes list
	num_wmodes = 0;

	for (i = 0, num_wmodes = 0; i < MAX_STDMODES; i++)
	{
		if (std_modes[i].width <= vid_deskwidth && std_modes[i].height <= vid_deskheight)
		{
			wmodelist[num_wmodes].type = MS_WINDOWED;
			wmodelist[num_wmodes].width = std_modes[i].width;
			wmodelist[num_wmodes].height = std_modes[i].height;
			q_snprintf (wmodelist[num_wmodes].modedesc, MAX_DESC, "%dx%d",
				 wmodelist[num_wmodes].width, wmodelist[num_wmodes].height);
			wmodelist[num_wmodes].modenum = MODE_WINDOWED;
			wmodelist[num_wmodes].dib = 1;
			wmodelist[num_wmodes].fullscreen = 0;
			wmodelist[num_wmodes].halfscreen = 0;
			wmodelist[num_wmodes].bpp = 0;
			num_wmodes++;
		}
	}
}


/*
=================
VID_InitFullDIB
=================
*/
static void VID_InitFullDIB (HINSTANCE hInstance)
{
	DEVMODE	devmode;
	int	i, modenum, existingmode;
	int	j, bpp, done;
	BOOL	stat;

	num_fmodes = 0;

	modenum = 0;

	// enumerate >8 bpp modes
	do
	{
		stat = EnumDisplaySettings (NULL, modenum, &devmode);

		if ((devmode.dmBitsPerPel >= 15) &&
			(devmode.dmPelsWidth <= MAXWIDTH) &&
			(devmode.dmPelsHeight <= MAXHEIGHT) &&
			(num_fmodes < MAX_MODE_LIST))
		{
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
								DISP_CHANGE_SUCCESSFUL)
			{
				fmodelist[num_fmodes].type = MS_FULLDIB;
				fmodelist[num_fmodes].width = devmode.dmPelsWidth;
				fmodelist[num_fmodes].height = devmode.dmPelsHeight;
				fmodelist[num_fmodes].modenum = 0;
				fmodelist[num_fmodes].halfscreen = 0;
				fmodelist[num_fmodes].dib = 1;
				fmodelist[num_fmodes].fullscreen = 1;
				fmodelist[num_fmodes].bpp = devmode.dmBitsPerPel;
				q_snprintf (fmodelist[num_fmodes].modedesc, MAX_DESC, "%dx%dx%d",
						(int)devmode.dmPelsWidth, (int)devmode.dmPelsHeight,
						(int)devmode.dmBitsPerPel);

			// if the width is more than twice the height, reduce it by half because this
			// is probably a dual-screen monitor
				if (!COM_CheckParm("-noadjustaspect"))
				{
					if (fmodelist[num_fmodes].width > (fmodelist[num_fmodes].height << 1))
					{
						fmodelist[num_fmodes].width >>= 1;
						fmodelist[num_fmodes].halfscreen = 1;
						q_snprintf (fmodelist[num_fmodes].modedesc, MAX_DESC, "%dx%dx%d",
								 fmodelist[num_fmodes].width,
								 fmodelist[num_fmodes].height,
								 fmodelist[num_fmodes].bpp);
					}
				}

				for (i = 0, existingmode = 0; i < num_fmodes; i++)
				{
					if ((fmodelist[num_fmodes].width == fmodelist[i].width)   &&
						(fmodelist[num_fmodes].height == fmodelist[i].height) &&
						(fmodelist[num_fmodes].bpp == fmodelist[i].bpp))
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
				{
					num_fmodes++;
				}
			}
		}

		modenum++;
	} while (stat);

	// see if there are any low-res modes that aren't being reported
	bpp = 16;
	done = 0;

	do
	{
		for (j = 0; (j < NUM_LOWRESMODES) && (num_fmodes < MAX_MODE_LIST); j++)
		{
			devmode.dmBitsPerPel = bpp;
			devmode.dmPelsWidth = std_modes[j].width;
			devmode.dmPelsHeight = std_modes[j].height;
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
					DISP_CHANGE_SUCCESSFUL)
			{
				fmodelist[num_fmodes].type = MS_FULLDIB;
				fmodelist[num_fmodes].width = devmode.dmPelsWidth;
				fmodelist[num_fmodes].height = devmode.dmPelsHeight;
				fmodelist[num_fmodes].modenum = 0;
				fmodelist[num_fmodes].halfscreen = 0;
				fmodelist[num_fmodes].dib = 1;
				fmodelist[num_fmodes].fullscreen = 1;
				fmodelist[num_fmodes].bpp = devmode.dmBitsPerPel;
				q_snprintf (fmodelist[num_fmodes].modedesc, MAX_DESC, "%dx%dx%d",
						(int)devmode.dmPelsWidth, (int)devmode.dmPelsHeight,
						(int)devmode.dmBitsPerPel);

				for (i = 0, existingmode = 0; i < num_fmodes; i++)
				{
					if ((fmodelist[num_fmodes].width == fmodelist[i].width)   &&
						(fmodelist[num_fmodes].height == fmodelist[i].height) &&
						(fmodelist[num_fmodes].bpp == fmodelist[i].bpp))
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
				{
					num_fmodes++;
				}
			}
		}
		switch (bpp)
		{
			case 16:
				bpp = 32;
				break;

			case 32:
#if 0	// O.S: don't mess with silly 24 bit lowres modes, they don't work correctly
				bpp = 24;
				break;

			case 24:
#endif
				done = 1;
				break;
		}
	} while (!done);

	if (num_fmodes == 0)
		Con_SafePrintf ("No fullscreen DIB modes found\n");
}

/*
=================
VID_ChangeVideoMode
intended only as a callback for VID_Restart_f
=================
*/
static void VID_ChangeVideoMode (int newmode)
{
	unsigned int	j;
	int	temp, temp2;

	// Avoid window updates and alt+tab handling (which sets modes back)
	temp = scr_disabled_for_loading;
	temp2 = vid_canalttab;
	scr_disabled_for_loading = true;
	vid_canalttab = false;

	// restore gamma
	if (maindc && gammaworks && SetDeviceGammaRamp_f)
		SetDeviceGammaRamp_f(maindc, orig_ramps);
	CDAudio_Pause ();
	MIDI_Pause (MIDI_ALWAYS_PAUSE);
	S_ClearBuffer ();

	// Unload all textures and reset texture counts
	D_ClearOpenGLTextures(0);
	texture_extension_number = 1U;
	lightmap_textures = 0U;
	for (j = 0; j < MAX_LIGHTMAPS; j++)
		lightmap_modified[j] = true;

	// reset all opengl function pointers
	GL_ResetFunctions();

	// Avoid re-registering commands and re-allocating memory
	draw_reinit = true;

	// temporarily disable input devices
	IN_DeactivateMouse();

	// Kill device and rendering contexts
	wglMakeCurrent_fp(NULL, NULL);
	if (baseRC)
		wglDeleteContext_fp(baseRC);
	baseRC = NULL;
	if (maindc && mainwindow)
		ReleaseDC(mainwindow, maindc);
	maindc = NULL;
	// Destroy main window and unregister its class
	if (mainwindow)
	{
		ShowWindow(mainwindow, SW_HIDE);
		DestroyWindow(mainwindow);
	}
	if (classregistered)
		UnregisterClass(WM_CLASSNAME, global_hInstance);
	mainwindow = NULL;
	classregistered = false;

#ifdef GL_DLSYM
	// reload the opengl library
	GL_CloseLibrary();
	Sleep (100);
	if (!GL_OpenLibrary(gl_library))
		Sys_Error ("Unable to load GL library %s", gl_library);
#endif

	// Register main window class and create main window
	VID_RegisterWndClass(global_hInstance);
	VID_SetMode(newmode, host_basepal);

	// Obtain device context and set up pixel format
	maindc = GetDC(mainwindow);
	bSetupPixelFormat(maindc);

	// Create OpenGL rendering context and make it current
	baseRC = wglCreateContext_fp(maindc);
	if (!baseRC)
		Sys_Error("wglCreateContext failed");
	if (!wglMakeCurrent_fp(maindc, baseRC ))
		Sys_Error("wglMakeCurrent failed");

	// Reload graphics wad file (Draw_PicFromWad writes glpic_t data (sizes,
	// texnums) right on top of the original pic data, so the pic data will
	// be dirty after gl textures are loaded the first time; we need to load
	// a clean version)
	W_LoadWadFile ("gfx.wad");
	// Initialize extensions and default OpenGL parameters
	GL_Init();
	VID_Init8bitPalette();

	// Reload pre-map pics, fonts, console, etc
	Draw_Init();
	SCR_Init();
	// R_Init() stuff:
	R_InitParticleTexture();
#if defined(H2W)
	R_InitNetgraphTexture();
#endif	/* H2W */
	Sbar_Init();
	vid.recalc_refdef = 1;

	IN_ReInit ();
	CDAudio_Resume ();
	MIDI_Pause (MIDI_ALWAYS_RESUME);

	// Reload model textures and player skins
	Mod_ReloadTextures();
	// rebuild the lightmaps
	GL_BuildLightmaps();
	// finished reloading all images
	draw_reinit = false;
	scr_disabled_for_loading = temp;
	// apply our gamma
	VID_ShiftPalette(NULL);
	vid_canalttab = temp2;
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

	if (a1->bpp == a2->bpp)
	{
		if (a1->width == a2->width)
		{
			return a1->height - a2->height;	// lowres-to-highres
		//	return a2->height - a1->height;	// highres-to-lowres
		}
		else
		{
			return a1->width - a2->width;	// lowres-to-highres
		//	return a2->width - a1->width;	// highres-to-lowres
		}
	}
	else
	{
		return a1->bpp - a2->bpp;	// low bpp-to-high bpp
	//	return a2->bpp - a1->bpp;	// high bpp-to-low bpp
	}
}

static void VID_SortModes (void)
{
	int	i, j;

	if (num_fmodes == 0)
		return;

	// sort the fullscreen modes list
	qsort(fmodelist, num_fmodes, sizeof fmodelist[0], sort_modes);
	// find which bpp values are reported to us
	for (i = 0; i < MAX_NUMBPP; i++)
	{
		bpplist[i][0] = 0;
		bpplist[i][1] = 0;
	}
	bpplist[0][0] = fmodelist[0].bpp;
	bpplist[0][1] = 0;
	for (i = 1, j = 0; i < num_fmodes && j < MAX_NUMBPP; i++)
	{
		if (fmodelist[i-1].bpp != fmodelist[i].bpp)
		{
			bpplist[++j][0] = fmodelist[i].bpp;
			bpplist[j][1] = i;
		}
	}

	vid_deskmode = -1;

	// find the desktop mode number. shouldn't fail!
	for (i = 1; i < num_fmodes; i++)
	{
		if ((fmodelist[i].width == vid_deskwidth) &&
			(fmodelist[i].height == vid_deskheight) &&
			(fmodelist[i].bpp == vid_deskbpp))
		{
			vid_deskmode = i;
			break;
		}
	}
	if (vid_deskmode < 0)
		Con_SafePrintf ("WARNING: desktop resolution not found in modelist\n");
}


/*
===================
VID_Init
===================
*/
void	VID_Init (unsigned char *palette)
{
	int	i, j, existingmode;
	int	width, height, bpp, zbits, findbpp, done;
#if DO_MESH_CACHE
	char	gldir[MAX_OSPATH];
#endif
	HDC	hdc;
	const char	*read_vars[] = {
				"vid_config_fscr",
				"vid_config_gl8bit",
				"vid_config_bpp",
				"vid_config_glx",
				"vid_config_gly",
				"vid_config_consize",
				"gl_lightmapfmt" };
#define num_readvars	( sizeof(read_vars)/sizeof(read_vars[0]) )

	Cvar_RegisterVariable (&vid_config_gl8bit);
	Cvar_RegisterVariable (&vid_config_fscr);
	Cvar_RegisterVariable (&vid_config_bpp);
	Cvar_RegisterVariable (&vid_config_gly);
	Cvar_RegisterVariable (&vid_config_glx);
	Cvar_RegisterVariable (&vid_config_consize);
	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&_enable_mouse);
	Cvar_RegisterVariable (&gl_lightmapfmt);
	// these are for compatibility with the software version
	Cvar_RegisterVariable (&vid_wait);
	Cvar_RegisterVariable (&_vid_default_mode);
	Cvar_RegisterVariable (&_vid_default_mode_win);
	Cvar_RegisterVariable (&vid_config_x);
	Cvar_RegisterVariable (&vid_config_y);
	Cvar_RegisterVariable (&vid_stretch_by_2);
	Cvar_RegisterVariable (&vid_maxpages);
	Cvar_RegisterVariable (&vid_nopageflip);

	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f);
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);
	Cmd_AddCommand ("vid_restart", VID_Restart_f);

#if ENDIAN_RUNTIME_DETECT
	switch (host_byteorder)
	{
	case BIG_ENDIAN:	/* R G B A */
		MASK_r	=	0xff000000;
		MASK_g	=	0x00ff0000;
		MASK_b	=	0x0000ff00;
		MASK_a	=	0x000000ff;
		SHIFT_r	=	24;
		SHIFT_g	=	16;
		SHIFT_b	=	8;
		SHIFT_a	=	0;
		break;
	case LITTLE_ENDIAN:	/* A B G R */
		MASK_r	=	0x000000ff;
		MASK_g	=	0x0000ff00;
		MASK_b	=	0x00ff0000;
		MASK_a	=	0xff000000;
		SHIFT_r	=	0;
		SHIFT_g	=	8;
		SHIFT_b	=	16;
		SHIFT_a	=	24;
		break;
	case PDP_ENDIAN:	/* G R A B */
		MASK_r	=	0x00ff0000;
		MASK_g	=	0xff000000;
		MASK_b	=	0x000000ff;
		MASK_a	=	0x0000ff00;
		SHIFT_r	=	16;
		SHIFT_g	=	24;
		SHIFT_b	=	0;
		SHIFT_a	=	8;
		break;
	default:
		break;
	}
	MASK_rgb	=	(MASK_r|MASK_g|MASK_b);
#endif	/* ENDIAN_RUNTIME_DETECT */

#if DO_MESH_CACHE
	// prepare directories for caching mesh files
	q_snprintf (gldir, sizeof(gldir), "%s/glhexen", fs_userdir);
	Sys_mkdir (gldir);
	q_snprintf (gldir, sizeof(gldir), "%s/glhexen/boss", fs_userdir);
	Sys_mkdir (gldir);
	q_snprintf (gldir, sizeof(gldir), "%s/glhexen/puzzle", fs_userdir);
	Sys_mkdir (gldir);
#endif

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON2));

#if !USE_HEXEN2_PALTEX_CODE && !defined(NO_SPLASHES)
	InitCommonControls();
	VID_SetPalette (palette);
#endif

#ifdef GL_DLSYM
	i = COM_CheckParm("--gllibrary");
	if (i == 0)
		i = COM_CheckParm ("-gllibrary");
	if (i == 0)
		i = COM_CheckParm ("-g");
	if (i && i < com_argc - 1)
		gl_library = com_argv[i+1];
	else
		gl_library = "opengl32.dll";
	if (!GL_OpenLibrary(gl_library))
		Sys_Error ("Unable to load GL library %s", gl_library);
#endif

	VID_InitDIB (global_hInstance);

	VID_InitFullDIB (global_hInstance);

	Con_SafePrintf ("Desktop settings: %d x %d x %d\n", vid_deskwidth, vid_deskheight, vid_deskbpp);

	// sort the modes
	VID_SortModes();

	// perform an early read of config.cfg
	CFG_ReadCvars (read_vars, num_readvars);

	width = vid_config_glx.integer;
	height = vid_config_gly.integer;

	if (COM_CheckParm("-window") || COM_CheckParm("-w"))
	{
		Cvar_SetValue("vid_config_fscr", 0);
	}
	else if (COM_CheckParm("-fullscreen") || COM_CheckParm("-f"))
	{
		Cvar_SetValue("vid_config_fscr", 1);
	}

	if (vid_config_consize.integer != width)
		vid_conscale = true;

	if (!vid_config_fscr.integer)
	{
		hdc = GetDC (NULL);
		if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
		{
			Sys_Error ("Can't run in non-RGB mode");
		}
		ReleaseDC (NULL, hdc);

		modelist = wmodelist;
		nummodes = &num_wmodes;
		vid_default = RES_640X480;

		// start parsing any dimension request from user
		i = COM_CheckParm("-width");
		if (i && i < com_argc-1)
		{
			j = atoi(com_argv[i+1]);
			// don't allow requests larger than desktop
			if (j <= vid_deskwidth && j >= 320)
			{
				width = j;
				height = width * 3 / 4;
				i = COM_CheckParm("-height");
				if (i && i < com_argc-1)
				{
					j = atoi(com_argv[i+1]);
					if (j <= vid_deskheight && j >= 240)
					{
						height= j;
					}
				}
			}
		}
		// end of parsing user commandline

		wmodelist[num_wmodes].width = width;
		wmodelist[num_wmodes].height = height;
		wmodelist[num_wmodes].type = MS_WINDOWED;
		q_snprintf (wmodelist[num_wmodes].modedesc, MAX_DESC, "%dx%d",
			 wmodelist[num_wmodes].width, wmodelist[num_wmodes].height);
		wmodelist[num_wmodes].modenum = MODE_WINDOWED;
		wmodelist[num_wmodes].dib = 1;
		wmodelist[num_wmodes].fullscreen = 0;
		wmodelist[num_wmodes].halfscreen = 0;
		wmodelist[num_wmodes].bpp = 0;

		for (i = 0, existingmode = 0; i < num_wmodes; i++)
		{
			if ((wmodelist[num_wmodes].width == wmodelist[i].width) &&
				(wmodelist[num_wmodes].height == wmodelist[i].height))
			{
				existingmode = 1;
				vid_default = i;
				break;
			}
		}

		if (!existingmode)
		{
			q_strlcat (wmodelist[num_wmodes].modedesc, " (user mode)", MAX_DESC);
			vid_default = num_wmodes;
			num_wmodes++;
		}

	}
	else	// fullscreen, default
	{
		if (num_fmodes == 0)
		{
			Sys_Error ("No RGB fullscreen modes available");
		}

		modelist = fmodelist;
		nummodes = &num_fmodes;
		vid_default = -1;

		findbpp = 1;
		//bpp = bpplist[0][0];
		bpp = vid_config_bpp.integer;
		if (Win95old)
		{	// don't bother with multiple bpp values on
			// windows versions older than win95-osr2.
			// in fact, should we stop supporting it?
			bpp = vid_deskbpp;
			findbpp = 0;
		}

		if (COM_CheckParm("-current"))
		{	// user wants fullscreen and
			// with desktop dimensions
			if (vid_deskmode >= 0)
			{
				q_strlcat (modelist[vid_deskmode].modedesc, " (desktop)", MAX_DESC);
				vid_default = vid_deskmode;
			}
			else
			{
				Con_SafePrintf ("WARNING: desktop mode not available for the -current switch\n");
			}
		}
		else
		{
			// start parsing any dimension/bpp request from user
			i = COM_CheckParm("-width");
			if (i && i < com_argc-1)
			{
				width = atoi(com_argv[i+1]);
				if (width < 320)
					width = 320;
				height = 3 * width / 4;
				i = COM_CheckParm("-height");
				if (i && i < com_argc-1)
				{
					height = atoi(com_argv[i+1]);
					if (height < 240)
						height = 240;
				}
			}

			i = COM_CheckParm("-bpp");
			if (i && i < com_argc-1 && !Win95old)
			{
				bpp = atoi(com_argv[i+1]);
				findbpp = 0;
			}

			// if they want to force it, add the specified mode to the list
			if (COM_CheckParm("-force"))
			{
				fmodelist[num_fmodes].type = MS_FULLDIB;
				fmodelist[num_fmodes].width = width;
				fmodelist[num_fmodes].height = height;
				fmodelist[num_fmodes].modenum = 0;
				fmodelist[num_fmodes].halfscreen = 0;
				fmodelist[num_fmodes].dib = 1;
				fmodelist[num_fmodes].fullscreen = 1;
				fmodelist[num_fmodes].bpp = bpp;
				q_snprintf (fmodelist[num_fmodes].modedesc, MAX_DESC, "%dx%dx%d",
						fmodelist[num_fmodes].width, fmodelist[num_fmodes].height,
						fmodelist[num_fmodes].bpp);

				for (i = 0, existingmode = 0; i < num_fmodes; i++)
				{
					if ((fmodelist[num_fmodes].width == fmodelist[i].width)   &&
						(fmodelist[num_fmodes].height == fmodelist[i].height) &&
						(fmodelist[num_fmodes].bpp == modelist[i].bpp))
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
				{
					q_strlcat (fmodelist[num_fmodes].modedesc, " (user mode)", MAX_DESC);
					num_fmodes++;
					// re-sort the modes
					VID_SortModes();
					if (findbpp)
						bpp = bpplist[0][0];
				}
			}

			if (vid_deskmode >= 0)
				q_strlcat (fmodelist[vid_deskmode].modedesc, " (desktop)", MAX_DESC);

			j = done = 0;

			do
			{
				for (i = 0; i < *nummodes; i++)
				{
					if ((modelist[i].width == width) &&
						(modelist[i].height == height) &&
						(modelist[i].bpp == bpp))
					{
						vid_default = i;
						done = 1;
						break;
					}
				}

				if (!done)
				{
					if (findbpp)
					{
						j++;
						if (i >= MAX_NUMBPP || !bpplist[j][0])
							done = 1;
						if (!done)
							bpp = bpplist[j][0];
					}
					else
					{
						done = 1;
					}
				}
			} while (!done);

			if (vid_default < 0)
			{
				Sys_Error ("Specified video mode not available:\n\n"
					   "If you used -width,-height or -bpp commandline arguments,\n"
					   "try changing their values. If your config.cfg has bad or stale\n"
					   "video options, try changing them, or delete your config.cfg\n"
					   "altogether and try again.");
			}

			//pfd.cColorBits = modelist[vid_default].bpp;

			i = COM_CheckParm("-zbits");
			if (i && i < com_argc-1)
			{
				zbits = atoi(com_argv[i+1]);
				if (zbits)
					pfd.cDepthBits = zbits;
			}
		}
	}	// end of fullscreen parsing

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

	vid_initialized = true;

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

#if !defined(NO_SPLASHES)
	DestroyWindow (hwnd_dialog);
#endif

	if (COM_CheckParm("-paltex"))
		Cvar_SetValue ("vid_config_gl8bit", 1);

	j = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	Cvar_SetValue ("vid_mode", vid_default);
	VID_SetMode (vid_default, palette);

	maindc = GetDC(mainwindow);
	bSetupPixelFormat(maindc);

	baseRC = wglCreateContext_fp( maindc );
	if (!baseRC)
		Sys_Error ("Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.");
	if (!wglMakeCurrent_fp( maindc, baseRC ))
		Sys_Error ("wglMakeCurrent failed");

	GL_SetupLightmapFmt(true);
	GL_Init ();

	// set our palette
	VID_SetPalette (palette);

	// enable paletted textures
	VID_Init8bitPalette();

	// lock the early-read cvars until Host_Init is finished
	for (i = 0; i < (int)num_readvars; i++)
		Cvar_LockVar (read_vars[i]);

	scr_disabled_for_loading = j;
	vid.recalc_refdef = 1;

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	q_strlcpy (badmode.modedesc,"Bad mode", MAX_DESC);
	vid_canalttab = true;

	if (COM_CheckParm("-fullsbar"))
		fullsbardraw = true;
}


void	VID_Shutdown (void)
{
	HGLRC	hRC;
	HDC	hDC;

	if (vid_initialized)
	{
		vid_canalttab = false;
		hRC = wglGetCurrentContext_fp();
		hDC = wglGetCurrentDC_fp();

		if (maindc && gammaworks && SetDeviceGammaRamp_f)
			SetDeviceGammaRamp_f(maindc, orig_ramps);

		wglMakeCurrent_fp(NULL, NULL);

		if (hRC)
			wglDeleteContext_fp(hRC);

		if (hDC && mainwindow)
			ReleaseDC(mainwindow, hDC);

		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);

		if (maindc && mainwindow)
			ReleaseDC (mainwindow, maindc);
		maindc = NULL;

		AppActivate(false, false);
#ifdef GL_DLSYM
		GL_CloseLibrary();
#endif
		if (mainwindow)
		{
			ShowWindow(mainwindow, SW_HIDE);
			DestroyWindow(mainwindow);
		}
		if (classregistered)
			UnregisterClass(WM_CLASSNAME, global_hInstance);
		mainwindow = NULL;
		classregistered = false;
	}
}


void VID_ToggleFullscreen (void)
{
}


#ifndef H2W
//unused in hexenworld
void D_ShowLoadingSize (void)
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
static vmode_t	*vid_menulist;	// this changes when vid_menu_fs changes
static int	vid_menubpp;	// if this changes, vid_menunum already changes
static qboolean	vid_menu_fs;
static qboolean	want_fstoggle, need_apply;
static qboolean	vid_menu_firsttime = true;

enum {
	VID_FULLSCREEN,
	VID_RESOLUTION,
	VID_BPP,
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
		vid_menubpp = modelist[vid_modenum].bpp;
		vid_menu_fs = (modestate != MS_WINDOWED);
		vid_menulist = (modestate == MS_WINDOWED) ? wmodelist : fmodelist;
		vid_cursor = (num_fmodes) ? 0 : VID_RESOLUTION;
		vid_menu_firsttime = false;
	}

	want_fstoggle = ( ((modestate == MS_WINDOWED) && vid_menu_fs) || ((modestate != MS_WINDOWED) && !vid_menu_fs) );

	need_apply = (vid_menunum != vid_modenum) || want_fstoggle || (have8bit && (is8bit != !!vid_config_gl8bit.integer));

	M_Print (76, 92 + 8*VID_FULLSCREEN, "Fullscreen: ");
	M_DrawYesNo (76+12*8, 92 + 8*VID_FULLSCREEN, vid_menu_fs, !want_fstoggle);

	M_Print (76, 92 + 8*VID_RESOLUTION, "Resolution: ");
	if (vid_menunum == vid_modenum)
		M_PrintWhite (76+12*8, 92 + 8*VID_RESOLUTION, vid_menulist[vid_menunum].modedesc);
	else
		M_Print (76+12*8, 92 + 8*VID_RESOLUTION, vid_menulist[vid_menunum].modedesc);

	if (vid_menu_fs && num_fmodes && !Win95old)
	{
		M_Print (76, 92 + 8*VID_BPP, "Color BPP : ");
		if (vid_menubpp == modelist[vid_modenum].bpp)
			M_PrintWhite (76+12*8, 92 + 8*VID_BPP, va("%d",vid_menubpp));
		else
			M_Print (76+12*8, 92 + 8*VID_BPP, va("%d",vid_menubpp));
	}

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

static int find_bppnum (int incr)
{
	int	j, pos = -1;

	if (!vid_menu_fs)	// then it doesn't matter
		return 0;

	for (j = 0; j < MAX_NUMBPP; j++)
	{	// find the pos in the bpplist
		if (vid_menubpp == bpplist[j][0])
		{
			pos = j;
			j = j+incr;
			break;
		}
	}
	if (pos < 0)
		Sys_Error ("bpp unexpectedly not found in list");
	if (incr == 0)
		return pos;
	// find the next available bpp
	while (1)
	{
		if (j >= MAX_NUMBPP)
			j = 0;
		if (j < 0)
			j = MAX_NUMBPP-1;
		if (bpplist[j][0])
			break;
		j = j + incr;
	}
	return j;
}

static int match_mode_to_bpp (int bppnum)
{
	int	k, l;

	k = bpplist[bppnum][1];
	l = bpplist[bppnum][1] + 1;
	for ( ; l < num_fmodes && fmodelist[l].bpp == vid_menubpp; l++)
	{
		if (fmodelist[vid_menunum].width == fmodelist[l].width &&
		    fmodelist[vid_menunum].height == fmodelist[l].height)
		{
			k = l;
			break;
		}
	}
	return k;
}

static int match_windowed_fullscr_modes (void)
{
	int	l;
	vmode_t	*tmplist;
	int	*tmpcount;

	// choose the new mode
	tmplist = (vid_menu_fs) ? fmodelist : wmodelist;
	tmpcount = (vid_menu_fs) ? &num_fmodes : &num_wmodes;
	for (l = 0; l < *tmpcount; l++)
	{
		if (tmplist[l].width == vid_menulist[vid_menunum].width &&
		    tmplist[l].height == vid_menulist[vid_menunum].height)
		{
			return l;
		}
	}
	return 0;
}

/*
================
VID_MenuKey
================
*/
static void VID_MenuKey (int key)
{
	int	i;
	int	*tmpnum;

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
		if (!num_fmodes && vid_cursor == 0)
		{
			vid_cursor = VID_RESOLUTION;
		}
		if ( vid_cursor == VID_BPP && (!vid_menu_fs || !num_fmodes || Win95old ))
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
		if ( vid_cursor == VID_BPP && (!vid_menu_fs || !num_fmodes || Win95old ))
			vid_cursor++;
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
			vid_menunum = vid_modenum;
			vid_menubpp = modelist[vid_modenum].bpp;
			vid_menu_fs = (modestate != MS_WINDOWED);
			vid_menulist = (modestate == MS_WINDOWED) ? wmodelist : fmodelist;
			Cvar_SetValue ("vid_config_gl8bit", is8bit);
			vid_cursor = (num_fmodes) ? 0 : VID_RESOLUTION;
			break;
		case VID_APPLY:
			if (need_apply)
			{
				Cvar_SetValue("vid_mode", vid_menunum);
				modelist = (vid_menu_fs) ? fmodelist : wmodelist;
				nummodes = (vid_menu_fs) ? &num_fmodes : &num_wmodes;
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
			if (!num_fmodes)
				break;
			vid_menu_fs = !vid_menu_fs;
			vid_menunum = match_windowed_fullscr_modes();
			vid_menulist = (vid_menu_fs) ? fmodelist : wmodelist;
			vid_menubpp = vid_menulist[vid_menunum].bpp;
			break;
		case VID_RESOLUTION:
			S_LocalSound ("raven/menu1.wav");
			vid_menunum--;
			if (vid_menunum < 0 || vid_menubpp != vid_menulist[vid_menunum].bpp)
				vid_menunum++;
			break;
		case VID_BPP:
			i = find_bppnum (-1);
			if (vid_menubpp == bpplist[i][0])
				break;
			vid_menubpp = bpplist[i][0];
			//find a matching video mode for this bpp
			vid_menunum = match_mode_to_bpp(i);
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
			if (!num_fmodes)
				break;
			vid_menu_fs = !vid_menu_fs;
			vid_menunum = match_windowed_fullscr_modes();
			vid_menulist = (vid_menu_fs) ? fmodelist : wmodelist;
			vid_menubpp = vid_menulist[vid_menunum].bpp;
			break;
		case VID_RESOLUTION:
			S_LocalSound ("raven/menu1.wav");
			tmpnum = (vid_menu_fs) ? &num_fmodes : &num_wmodes;
			vid_menunum++;
			if (vid_menunum >= *tmpnum || vid_menubpp != vid_menulist[vid_menunum].bpp)
				vid_menunum--;
			break;
		case VID_BPP:
			i = find_bppnum (1);
			if (vid_menubpp == bpplist[i][0])
				break;
			vid_menubpp = bpplist[i][0];
			//find a matching video mode for this bpp
			vid_menunum = match_mode_to_bpp(i);
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

