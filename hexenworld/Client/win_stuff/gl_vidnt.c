// gl_vidnt.c -- NT GL vid component

#include "quakedef.h"
#include "quakeinc.h"
#include "resource.h"
#include "wgl_func.h"
//#include <commctrl.h>

#define MAX_MODE_LIST	30
#define VID_ROW_SIZE	3
#define WARP_WIDTH	320
#define WARP_HEIGHT	200
#define MAXWIDTH	10000
#define MAXHEIGHT	10000
#define BASEWIDTH	320
#define BASEHEIGHT	200

#define MODE_WINDOWED		0
#define NO_MODE			(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 1)

byte globalcolormap[VID_GRADES*256];

typedef struct {
	modestate_t	type;
	int	width;
	int	height;
	int	modenum;
	int	dib;
	int	fullscreen;
	int	bpp;
	int	halfscreen;
	char	modedesc[33];
} vmode_t;

typedef struct {
	int	width;
	int	height;
} lmode_t;

lmode_t	lowresmodes[] = {
	{320, 200},
	{320, 240},
	{400, 300},
	{512, 384},
};

#ifdef GL_DLSYM
HINSTANCE	hInstGL;
const char	*gl_library;
#endif
const char	*gl_vendor;
const char	*gl_renderer;
const char	*gl_version;
const char	*gl_extensions;
int		gl_max_size = 256;
qboolean	is8bit = false;
qboolean	is_3dfx = false;
qboolean	gl_mtexable = false;
int		num_tmus = 1;

qboolean	DDActive;
qboolean	scr_skipupdate;

static vmode_t	modelist[MAX_MODE_LIST];
static int	nummodes;
static vmode_t	badmode;

static DEVMODE	gdevmode;
static qboolean	vid_initialized = false;
static qboolean	windowed, leavecurrentmode;
static qboolean vid_canalttab = false;
static qboolean vid_wassuspended = false;
static int	enable_mouse;
static HICON	hIcon;

int		DIBWidth, DIBHeight;
RECT		WindowRect;
DWORD		WindowStyle, ExWindowStyle;

HWND		mainwindow, dibwindow;

int		vid_modenum = NO_MODE;
int		vid_realmode;
int		vid_default = MODE_WINDOWED;
static int	windowed_default;
unsigned char	vid_curpal[256*3];
static qboolean fullsbardraw = false;

HGLRC		baseRC;
HDC		maindc;

cvar_t	gl_ztrick = {"gl_ztrick","0",true};
cvar_t		gl_purge_maptex = {"gl_purge_maptex", "1", true};
		/* whether or not map-specific OGL textures
		   are flushed from map. default == yes  */

HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);

viddef_t	vid;				// global video state

unsigned short	d_8to16table[256];
unsigned	d_8to24table[256];
unsigned	d_8to24TranslucentTable[256];
unsigned	d_8to24table3dfx[256];
unsigned char	d_15to8table[65536];
float		RTint[256],GTint[256],BTint[256];

float		gldepthmin, gldepthmax;

modestate_t	modestate = MS_UNINIT;

void VID_MenuDraw (void);
void VID_MenuKey (int key);

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AppActivate(BOOL fActive, BOOL minimize);
char *VID_GetModeDescription (int mode);
void ClearAllStates (void);
void VID_UpdateWindowStatus (void);
void GL_Init (void);

typedef void (APIENTRY *FX_SET_PALETTE_EXT)(int, int, int, int, int, const void*);
FX_SET_PALETTE_EXT MyglColorTableEXT;
typedef BOOL (APIENTRY *GAMMA_RAMP_FN)(HDC, LPVOID);
GAMMA_RAMP_FN GetDeviceGammaRamp_f;
GAMMA_RAMP_FN SetDeviceGammaRamp_f;

//====================================

cvar_t		vid_mode = {"vid_mode","0", false};
// Note that 0 is MODE_WINDOWED
cvar_t		_vid_default_mode = {"_vid_default_mode","0", true};
// Note that 3 is MODE_FULLSCREEN_DEFAULT
cvar_t		_vid_default_mode_win = {"_vid_default_mode_win","3", true};
cvar_t		vid_wait = {"vid_wait","0"};
cvar_t		vid_nopageflip = {"vid_nopageflip","0", true};
cvar_t		_vid_wait_override = {"_vid_wait_override", "0", true};
cvar_t		vid_config_x = {"vid_config_x","800", true};
cvar_t		vid_config_y = {"vid_config_y","600", true};
cvar_t		vid_stretch_by_2 = {"vid_stretch_by_2","1", true};
cvar_t		_enable_mouse = {"_enable_mouse","0", true};

int		window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;


extern unsigned short	ramps[3][256];
unsigned short	orig_ramps[3][256];
qboolean	gammaworks = false;
qboolean	gl_dogamma = false;


// direct draw software compatability stuff

void VID_HandlePause (qboolean pause)
{
}

void VID_ForceLockState (int lk)
{
}

void VID_LockBuffer (void)
{
}

void VID_UnlockBuffer (void)
{
}

int VID_ForceUnlockedAndReturnState (void)
{
	return 0;
}

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}


void CenterWindow(HWND hWndCenter, int width, int height, BOOL lefttopjustify)
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

void VID_ConWidth(int modenum)
{
	int i;

	vid.width  = vid.conwidth  = modelist[modenum].width;
	vid.height = vid.conheight = modelist[modenum].height;

	// This will display a bigger hud and readable fonts at high
	// resolutions. The fonts will be somewhat distorted, though
	i = COM_CheckParm("-conwidth");
	if (i != 0 && i < com_argc-1) {
		vid.conwidth = atoi(com_argv[i+1]);
		vid.conwidth &= 0xfff8; // make it a multiple of eight
		if (vid.conwidth < 320)
			vid.conwidth = 320;
		// pick a conheight that matches with correct aspect
		vid.conheight = vid.conwidth*3 / 4;
		i = COM_CheckParm("-conheight");
		if (i != 0 && i < com_argc-1)
			vid.conheight = atoi(com_argv[i+1]);
		if (vid.conheight < 200)
			vid.conheight = 200;
		if (vid.conwidth > modelist[modenum].width)
			vid.conwidth = modelist[modenum].width;
		if (vid.conheight > modelist[modenum].height)
			vid.conheight = modelist[modenum].height;

		vid.width = vid.conwidth;
		vid.height = vid.conheight;
	}
}

qboolean VID_SetWindowedMode (int modenum)
{
	HDC	hdc;
	int	lastmodestate, width, height;
	RECT	rect;

	lastmodestate = modestate;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU |
				  WS_MINIMIZEBOX;
	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	dibwindow = CreateWindowEx (
		 ExWindowStyle,
#ifndef H2W
		 "HexenII",
		 "HexenII",
#else
		 "HexenWorld",
		 "HexenWorld",
#endif
		 WindowStyle,
		 rect.left, rect.top,
		 width,
		 height,
		 NULL,
		 NULL,
		 global_hInstance,
		 NULL);

	if (!dibwindow)
		Sys_Error ("Couldn't create DIB window");

	// Center and show the DIB window
	CenterWindow(dibwindow, WindowRect.right - WindowRect.left,
				 WindowRect.bottom - WindowRect.top, false);

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	modestate = MS_WINDOWED;

// because we have set the background brush for the window to NULL
// (to avoid flickering when re-sizing the window on the desktop),
// we clear the window to black when created, otherwise it will be
// empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(dibwindow, hdc);

	VID_ConWidth(modenum);

	vid.numpages = 2;

	mainwindow = dibwindow;

	SendMessage (mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	return true;
}


qboolean VID_SetFullDIBMode (int modenum)
{
	HDC	hdc;
	int	lastmodestate, width, height;
	RECT	rect;

	if (!leavecurrentmode)
	{
		gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		gdevmode.dmBitsPerPel = modelist[modenum].bpp;
		gdevmode.dmPelsWidth = modelist[modenum].width <<
							   modelist[modenum].halfscreen;
		gdevmode.dmPelsHeight = modelist[modenum].height;
		gdevmode.dmSize = sizeof (gdevmode);

		if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			Sys_Error ("Couldn't set fullscreen DIB mode");
	}

	lastmodestate = modestate;
	modestate = MS_FULLDIB;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	WindowStyle = WS_POPUP;
	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	dibwindow = CreateWindowEx (
		 ExWindowStyle,
#ifndef H2W
		 "HexenII",
		 "HexenII",
#else
		 "HexenWorld",
		 "HexenWorld",
#endif
		 WindowStyle,
		 rect.left, rect.top,
		 width,
		 height,
		 NULL,
		 NULL,
		 global_hInstance,
		 NULL);

	if (!dibwindow)
		Sys_Error ("Couldn't create DIB window");

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	// Because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop), we
	// clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(dibwindow, hdc);

	VID_ConWidth(modenum);

	vid.numpages = 2;

// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = 0;
	window_y = 0;

	mainwindow = dibwindow;

	SendMessage (mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	return true;
}


int VID_SetMode (int modenum, unsigned char *palette)
{
	int		original_mode, temp;
	qboolean	stat = false;
	MSG		msg;

	if ((windowed && (modenum != 0)) ||
		(!windowed && (modenum < 1)) ||
		(!windowed && (modenum >= nummodes)))
	{
		Sys_Error ("Bad video mode\n");
	}

// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();

	if (vid_modenum == NO_MODE)
		original_mode = windowed_default;
	else
		original_mode = vid_modenum;

	// Set either the fullscreen or windowed mode
	if (modelist[modenum].type == MS_WINDOWED)
	{
		if (_enable_mouse.value)
		{
			stat = VID_SetWindowedMode(modenum);
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
		else
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			stat = VID_SetWindowedMode(modenum);
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
		Sys_Error ("VID_SetMode: Bad mode type in modelist");
	}

	if (!stat)
	{
		Sys_Error ("Couldn't set video mode");
	}

	window_width = DIBWidth;
	window_height = DIBHeight;
	VID_UpdateWindowStatus ();

	CDAudio_Resume ();
	scr_disabled_for_loading = temp;

// now we try to make sure we get the focus on the mode switch, because
// sometimes in some systems we don't.  We grab the foreground, then
// finish setting up, pump all our messages, and sleep for a little while
// to let messages finish bouncing around the system, then we put
// ourselves at the top of the z order, then grab the foreground again,
// Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);
	VID_SetPalette (palette);
	vid_modenum = modenum;
	Cvar_SetValue ("vid_mode", (float)vid_modenum);

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

	if (!msg_suppress_1)
		Con_SafePrintf ("Video mode %s initialized.\n", VID_GetModeDescription (vid_modenum));

	VID_SetPalette (palette);

	vid.recalc_refdef = 1;

	return true;
}


/*
================
VID_UpdateWindowStatus
================
*/
void VID_UpdateWindowStatus (void)
{
	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}


//====================================

void Check3DfxGammaControlExtension (void)
{
	if (!strstr(gl_extensions, "WGL_3DFX_gamma_control"))
	{
		GetDeviceGammaRamp_f = GetDeviceGammaRamp;
		SetDeviceGammaRamp_f = SetDeviceGammaRamp;
	}
	else
	{
		GetDeviceGammaRamp_f = wglGetProcAddress_fp("wglGetDeviceGammaRamp3DFX");
		SetDeviceGammaRamp_f = wglGetProcAddress_fp("wglSetDeviceGammaRamp3DFX");
		if (GetDeviceGammaRamp_f && SetDeviceGammaRamp_f)
			Con_Printf("Using 3Dfx specific gamma control\n");
		else {
			GetDeviceGammaRamp_f = GetDeviceGammaRamp;
			SetDeviceGammaRamp_f = SetDeviceGammaRamp;
		}
	}
}

int		texture_extension_number = 1;

void CheckMultiTextureExtensions(void)
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
		if (num_tmus < 2) {
			Con_Printf("not enough TMUs, ignoring multitexture\n");
			return;
		}

		glMultiTexCoord2fARB_fp = (void *) wglGetProcAddress_fp("glMultiTexCoord2fARB");
		glActiveTextureARB_fp = (void *) wglGetProcAddress_fp("glActiveTextureARB");
		if ((glMultiTexCoord2fARB_fp == NULL) ||
		    (glActiveTextureARB_fp == NULL)) {
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

#ifdef GL_DLSYM
static void GL_CloseLibrary(void)
{
	// clear the wgl function pointers
	wglGetProcAddress_fp = NULL;
	wglCreateContext_fp = NULL;
	wglDeleteContext_fp = NULL;
	wglMakeCurrent_fp = NULL;
	wglGetCurrentContext_fp = NULL;
	wglGetCurrentDC_fp = NULL;

	// free the library
	if (hInstGL != NULL)
		FreeLibrary(hInstGL);
        hInstGL = NULL;
}

static qboolean GL_OpenLibrary(const char *name)
{
	Con_Printf("Loading OpenGL library %s\n", name);

	// open the library
	if (!(hInstGL = LoadLibrary(name)))
	{
		Con_Printf("Unable to LoadLibrary %s\n", name);
		return false;
	}

	// link to necessary wgl functions
	wglGetProcAddress_fp = (void *)GetProcAddress(hInstGL, "wglGetProcAddress");
	if (wglGetProcAddress_fp == NULL) {Sys_Error("Couldn't link to wglGetProcAddress");}
	wglCreateContext_fp = (void *)GetProcAddress(hInstGL, "wglCreateContext");
	if (wglCreateContext_fp == NULL) {Sys_Error("Couldn't link to wglCreateContext");}
	wglDeleteContext_fp = (void *)GetProcAddress(hInstGL, "wglDeleteContext");
	if (wglDeleteContext_fp == NULL) {Sys_Error("Couldn't link to wglDeleteContext");}
	wglMakeCurrent_fp = (void *)GetProcAddress(hInstGL, "wglMakeCurrent");
	if (wglMakeCurrent_fp == NULL) {Sys_Error("Couldn't link to wglMakeCurrent");}
	wglGetCurrentContext_fp = (void *)GetProcAddress(hInstGL, "wglGetCurrentContext");
	if (wglGetCurrentContext_fp == NULL) {Sys_Error("Couldn't link to wglGetCurrentContext");}
	wglGetCurrentDC_fp = (void *)GetProcAddress(hInstGL, "wglGetCurrentDC");
	if (wglGetCurrentDC_fp == NULL) {Sys_Error("Couldn't link to wglGetCurrentDC");}

	return true;
}

void GL_Init_Functions(void)
{
  glBegin_fp = (glBegin_f) wglGetProcAddress_fp("glBegin");
  if (glBegin_fp == 0) {Sys_Error("glBegin not found in GL library");}
  glEnd_fp = (glEnd_f) wglGetProcAddress_fp("glEnd");
  if (glEnd_fp == 0) {Sys_Error("glEnd not found in GL library");}
  glEnable_fp = (glEnable_f) wglGetProcAddress_fp("glEnable");
  if (glEnable_fp == 0) {Sys_Error("glEnable not found in GL library");}
  glDisable_fp = (glDisable_f) wglGetProcAddress_fp("glDisable");
  if (glDisable_fp == 0) {Sys_Error("glDisable not found in GL library");}
#ifdef H2W
  glIsEnabled_fp = (glIsEnabled_f) wglGetProcAddress_fp("glIsEnabled");
  if (glIsEnabled_fp == 0) {Sys_Error("glIsEnabled not found in GL library");}
#endif
  glFinish_fp = (glFinish_f) wglGetProcAddress_fp("glFinish");
  if (glFinish_fp == 0) {Sys_Error("glFinish not found in GL library");}
  glClear_fp = (glClear_f) wglGetProcAddress_fp("glClear");
  if (glClear_fp == 0) {Sys_Error("glClear not found in GL library");}

  glOrtho_fp = (glOrtho_f) wglGetProcAddress_fp("glOrtho");
  if (glOrtho_fp == 0) {Sys_Error("glOrtho not found in GL library");}
  glFrustum_fp = (glFrustum_f) wglGetProcAddress_fp("glFrustum");
  if (glFrustum_fp == 0) {Sys_Error("glFrustum not found in GL library");}
  glViewport_fp = (glViewport_f) wglGetProcAddress_fp("glViewport");
  if (glViewport_fp == 0) {Sys_Error("glViewport not found in GL library");}
  glPushMatrix_fp = (glPushMatrix_f) wglGetProcAddress_fp("glPushMatrix");
  if (glPushMatrix_fp == 0) {Sys_Error("glPushMatrix not found in GL library");}
  glPopMatrix_fp = (glPopMatrix_f) wglGetProcAddress_fp("glPopMatrix");
  if (glPopMatrix_fp == 0) {Sys_Error("glPopMatrix not found in GL library");}
  glLoadIdentity_fp = (glLoadIdentity_f) wglGetProcAddress_fp("glLoadIdentity");
  if (glLoadIdentity_fp == 0) {Sys_Error("glLoadIdentity not found in GL library");}
  glMatrixMode_fp = (glMatrixMode_f) wglGetProcAddress_fp("glMatrixMode");
  if (glMatrixMode_fp == 0) {Sys_Error("glMatrixMode not found in GL library");}
  glLoadMatrixf_fp = (glLoadMatrixf_f) wglGetProcAddress_fp("glLoadMatrixf");
  if (glLoadMatrixf_fp == 0) {Sys_Error("glLoadMatrixf not found in GL library");}

  glVertex2f_fp = (glVertex2f_f) wglGetProcAddress_fp("glVertex2f");
  if (glVertex2f_fp == 0) {Sys_Error("glVertex2f not found in GL library");}
  glVertex3f_fp = (glVertex3f_f) wglGetProcAddress_fp("glVertex3f");
  if (glVertex3f_fp == 0) {Sys_Error("glVertex3f not found in GL library");}
  glVertex3fv_fp = (glVertex3fv_f) wglGetProcAddress_fp("glVertex3fv");
  if (glVertex3fv_fp == 0) {Sys_Error("glVertex3fv not found in GL library");}
  glTexCoord2f_fp = (glTexCoord2f_f) wglGetProcAddress_fp("glTexCoord2f");
  if (glTexCoord2f_fp == 0) {Sys_Error("glTexCoord2f not found in GL library");}
  glTexCoord3f_fp = (glTexCoord3f_f) wglGetProcAddress_fp("glTexCoord3f");
  if (glTexCoord3f_fp == 0) {Sys_Error("glTexCoord3f not found in GL library");}
  glColor4f_fp = (glColor4f_f) wglGetProcAddress_fp("glColor4f");
  if (glColor4f_fp == 0) {Sys_Error("glColor4f not found in GL library");}
  glColor4fv_fp = (glColor4fv_f) wglGetProcAddress_fp("glColor4fv");
  if (glColor4fv_fp == 0) {Sys_Error("glColor4fv not found in GL library");}
#ifdef H2W
  glColor4ub_fp = (glColor4ub_f) wglGetProcAddress_fp("glColor4ub");
  if (glColor4ub_fp == 0) {Sys_Error("glColor4ub not found in GL library");}
#endif
  glColor4ubv_fp = (glColor4ubv_f) wglGetProcAddress_fp("glColor4ubv");
  if (glColor4ubv_fp == 0) {Sys_Error("glColor4ubv not found in GL library");}
  glColor3f_fp = (glColor3f_f) wglGetProcAddress_fp("glColor3f");
  if (glColor3f_fp == 0) {Sys_Error("glColor3f not found in GL library");}
  glColor3ubv_fp = (glColor3ubv_f) wglGetProcAddress_fp("glColor3ubv");
  if (glColor3ubv_fp == 0) {Sys_Error("glColor3ubv not found in GL library");}
  glClearColor_fp = (glClearColor_f) wglGetProcAddress_fp("glClearColor");
  if (glClearColor_fp == 0) {Sys_Error("glClearColor not found in GL library");}

  glRotatef_fp = (glRotatef_f) wglGetProcAddress_fp("glRotatef");
  if (glRotatef_fp == 0) {Sys_Error("glRotatef not found in GL library");}
  glTranslatef_fp = (glTranslatef_f) wglGetProcAddress_fp("glTranslatef");
  if (glTranslatef_fp == 0) {Sys_Error("glTranslatef not found in GL library");}

  glBindTexture_fp = (glBindTexture_f) wglGetProcAddress_fp("glBindTexture");
  if (glBindTexture_fp == 0) {Sys_Error("glBindTexture not found in GL library");}
  glDeleteTextures_fp = (glDeleteTextures_f) wglGetProcAddress_fp("glDeleteTextures");
  if (glDeleteTextures_fp == 0) {Sys_Error("glDeleteTextures not found in GL library");}
  glTexParameterf_fp = (glTexParameterf_f) wglGetProcAddress_fp("glTexParameterf");
  if (glTexParameterf_fp == 0) {Sys_Error("glTexParameterf not found in GL library");}
  glTexEnvf_fp = (glTexEnvf_f) wglGetProcAddress_fp("glTexEnvf");
  if (glTexEnvf_fp == 0) {Sys_Error("glTexEnvf not found in GL library");}
  glScalef_fp = (glScalef_f) wglGetProcAddress_fp("glScalef");
  if (glScalef_fp == 0) {Sys_Error("glScalef not found in GL library");}
  glTexImage2D_fp = (glTexImage2D_f) wglGetProcAddress_fp("glTexImage2D");
  if (glTexImage2D_fp == 0) {Sys_Error("glTexImage2D not found in GL library");}
#ifdef H2W
  glTexSubImage2D_fp = (glTexSubImage2D_f) wglGetProcAddress_fp("glTexSubImage2D");
  if (glTexSubImage2D_fp == 0) {Sys_Error("glTexSubImage2D not found in GL library");}
#endif

  glAlphaFunc_fp = (glAlphaFunc_f) wglGetProcAddress_fp("glAlphaFunc");
  if (glAlphaFunc_fp == 0) {Sys_Error("glAlphaFunc not found in GL library");}
  glBlendFunc_fp = (glBlendFunc_f) wglGetProcAddress_fp("glBlendFunc");
  if (glBlendFunc_fp == 0) {Sys_Error("glBlendFunc not found in GL library");}
  glShadeModel_fp = (glShadeModel_f) wglGetProcAddress_fp("glShadeModel");
  if (glShadeModel_fp == 0) {Sys_Error("glShadeModel not found in GL library");}
  glPolygonMode_fp = (glPolygonMode_f) wglGetProcAddress_fp("glPolygonMode");
  if (glPolygonMode_fp == 0) {Sys_Error("glPolygonMode not found in GL library");}
  glDepthMask_fp = (glDepthMask_f) wglGetProcAddress_fp("glDepthMask");
  if (glDepthMask_fp == 0) {Sys_Error("glDepthMask not found in GL library");}
  glDepthRange_fp = (glDepthRange_f) wglGetProcAddress_fp("glDepthRange");
  if (glDepthRange_fp == 0) {Sys_Error("glDepthRange not found in GL library");}
  glDepthFunc_fp = (glDepthFunc_f) wglGetProcAddress_fp("glDepthFunc");
  if (glDepthFunc_fp == 0) {Sys_Error("glDepthFunc not found in GL library");}

  glDrawBuffer_fp = (glDrawBuffer_f) wglGetProcAddress_fp("glDrawBuffer");
  if (glDrawBuffer_fp == 0) {Sys_Error("glDrawBuffer not found in GL library");}
  glReadBuffer_fp = (glDrawBuffer_f) wglGetProcAddress_fp("glReadBuffer");
  if (glReadBuffer_fp == 0) {Sys_Error("glReadBuffer not found in GL library");}
  glReadPixels_fp = (glReadPixels_f) wglGetProcAddress_fp("glReadPixels");
  if (glReadPixels_fp == 0) {Sys_Error("glReadPixels not found in GL library");}
  glHint_fp = (glHint_f) wglGetProcAddress_fp("glHint");
  if (glHint_fp == 0) {Sys_Error("glHint not found in GL library");}
  glCullFace_fp = (glCullFace_f) wglGetProcAddress_fp("glCullFace");
  if (glCullFace_fp == 0) {Sys_Error("glCullFace not found in GL library");}

  glGetIntegerv_fp = (glGetIntegerv_f) wglGetProcAddress_fp("glGetIntegerv");
  if (glGetIntegerv_fp == 0) {Sys_Error("glGetIntegerv not found in GL library");}

  glGetString_fp = (glGetString_f) wglGetProcAddress_fp("glGetString");
  if (glGetString_fp == 0) {Sys_Error("glGetString not found in GL library");}
  glGetFloatv_fp = (glGetFloatv_f) wglGetProcAddress_fp("glGetFloatv");
  if (glGetFloatv_fp == 0) {Sys_Error("glGetFloatv not found in GL library");}
}
#endif	// GL_DLSYM

/*
===============
GL_Init
===============
*/
void GL_Init (void)
{
#ifdef GL_DLSYM
	GL_Init_Functions();
#endif
	gl_vendor = glGetString_fp (GL_VENDOR);
	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = glGetString_fp (GL_RENDERER);
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);

	gl_version = glGetString_fp (GL_VERSION);
	Con_Printf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = glGetString_fp (GL_EXTENSIONS);
	Con_DPrintf ("GL_EXTENSIONS: %s\n", gl_extensions);

	glGetIntegerv_fp(GL_MAX_TEXTURE_SIZE, &gl_max_size);
	if (gl_max_size < 256)	// Refuse to work when less than 256
		Sys_Error ("hardware capable of min. 256k opengl texture size needed");
	if (gl_max_size > 1024)	// We're cool with 1024, write a cmdline override if necessary
		gl_max_size = 1024;
	Con_Printf("OpenGL max.texture size: %i\n", gl_max_size);

	if (!Q_strncasecmp ((char *)gl_renderer, "3dfx",  4)  ||
	    !Q_strncasecmp ((char *)gl_renderer, "Glide", 5)  ||
	    !Q_strncasecmp ((char *)gl_renderer, "Mesa Glide", 10))
	{
		Con_Printf("3dfx Voodoo found\n");
		is_3dfx = true;
	}

	if (Q_strncasecmp(gl_renderer,"PowerVR",7)==0)
		fullsbardraw = true;

	GetDeviceGammaRamp_f = NULL;
	SetDeviceGammaRamp_f = NULL;
	CheckMultiTextureExtensions ();
	Check3DfxGammaControlExtension();

	glClearColor_fp (1,0,0,0);
	glCullFace_fp(GL_FRONT);
	glEnable_fp(GL_TEXTURE_2D);

	glEnable_fp(GL_ALPHA_TEST);
	glAlphaFunc_fp(GL_GREATER, 0.666);

	glPolygonMode_fp (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel_fp (GL_FLAT);

	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// previously GL_CLAMP was GL_REPEAT S.A
	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

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
	if (!scr_skipupdate || block_drawing)
		SwapBuffers(maindc);

// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED)
	{
		if ((int)_enable_mouse.value != enable_mouse)
		{
			if (_enable_mouse.value)
			{
				IN_ActivateMouse ();
				IN_HideMouse ();
			}
			else
			{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}

			enable_mouse = (int)_enable_mouse.value;
		}
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

void VID_SetPalette (unsigned char *palette)
{
	byte		*pal;
	unsigned short	r,g,b;
	int		v;
	int		r1,g1,b1;
	int		j,k,l,m;
	unsigned short	i, p, c;
	unsigned	*table, *table3dfx;
	FILE		*f;
	char		s[MAX_OSPATH];
#if !defined(NO_SPLASHES)
	HWND		hDlg, hProgress;
#endif

//
// 8 8 8 encoding
//
	pal = palette;
	table = d_8to24table;
	table3dfx = d_8to24table3dfx;
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;
		
//		v = (255<<24) + (r<<16) + (g<<8) + (b<<0);
//		v = (255<<0) + (r<<8) + (g<<16) + (b<<24);
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		*table++ = v;
		v = (255<<24) + (r<<16) + (g<<8) + (b<<0);
		*table3dfx++ = v;
	}

	pal = palette;
	table = d_8to24TranslucentTable;

	for (i=0; i<16;i++)
	{
		c = ColorIndex[i]*3;

		r = pal[c];
		g = pal[c+1];
		b = pal[c+2];

		for(p=0;p<16;p++)
		{
			v = (ColorPercent[15-p]<<24) + (r<<0) + (g<<8) + (b<<16);
			//v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
			*table++ = v;

			RTint[i*16+p] = ((float)r) / ((float)ColorPercent[15-p]) ;
			GTint[i*16+p] = ((float)g) / ((float)ColorPercent[15-p]);
			BTint[i*16+p] = ((float)b) / ((float)ColorPercent[15-p]);
		}
	}

	// JACK: 3D distance calcs - k is last closest, l is the distance.
	// FIXME: Precalculate this and cache to disk.

	COM_FOpenFile("glhexen/15to8.pal", &f, true);
	if (f)
	{
		fread(d_15to8table, 1<<15, 1, f);
		fclose(f);
	} 
	else 
	{
#if !defined(NO_SPLASHES)
		hDlg = CreateDialog(global_hInstance, MAKEINTRESOURCE(IDD_PROGRESS), 
			NULL, NULL);
		hProgress = GetDlgItem(hDlg, IDC_PROGRESS);
		SendMessage(hProgress, PBM_SETSTEP, 1, 0);
		SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 33));
#endif
		for (i=0,m=0; i < (1<<15); i++,m++) 
		{
			/* Maps
 			000000000000000
 			000000000011111 = Red  = 0x1F
 			000001111100000 = Blue = 0x03E0
 			111110000000000 = Grn  = 0x7C00
 			*/
 			r = ((i & 0x1F) << 3)+4;
 			g = ((i & 0x03E0) >> 2)+4;
 			b = ((i & 0x7C00) >> 7)+4;
#if 0
			r = (i << 11);
			g = (i << 6);
			b = (i << 1);
			r >>= 11;
			g >>= 11;
			b >>= 11;
#endif
			pal = (unsigned char *)d_8to24table;
			for (v=0,k=0,l=10000; v<256; v++,pal+=4)
			{
 				r1 = r-pal[0];
 				g1 = g-pal[1];
 				b1 = b-pal[2];
				j = sqrt(((r1*r1)+(g1*g1)+(b1*b1)));
				if (j<l)
				{
					k=v;
					l=j;
				}
			}
			d_15to8table[i]=k;
			if (m >= 1000)
			{
#if !defined(NO_SPLASHES)
#ifdef DEBUG_BUILD
				sprintf(s, "Done - %d\n", i);
				OutputDebugString(s);
#endif
				SendMessage(hProgress, PBM_STEPIT, 0, 0);
#endif
				m=0;
			}
		}
		sprintf(s, "%s/glhexen", com_gamedir);
 		Sys_mkdir (s);
		sprintf(s, "%s/glhexen/15to8.pal", com_gamedir);
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

	d_8to24table[255] &= 0xffffff;	// 255 is transparent
}

void VID_ApplyGamma(void)
{
}

void	VID_ShiftPalette (unsigned char *palette)
{
	if (gammaworks && SetDeviceGammaRamp_f)
		SetDeviceGammaRamp_f (maindc, ramps);
}


void VID_SetDefaultMode (void)
{
	IN_DeactivateMouse ();
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

		if (hDC && dibwindow)
			ReleaseDC(dibwindow, hDC);

		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);

		if (maindc && dibwindow)
			ReleaseDC (dibwindow, maindc);

		AppActivate(false, false);
#ifdef GL_DLSYM
		GL_CloseLibrary();
#endif
	}
}


//==========================================================================


BOOL bSetupPixelFormat(HDC hDC)
{
    static PIXELFORMATDESCRIPTOR pfd = {
	sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
	1,				// version number
	PFD_DRAW_TO_WINDOW 		// support window
	|  PFD_SUPPORT_OPENGL 	// support OpenGL
	|  PFD_DOUBLEBUFFER ,	// double buffered
	PFD_TYPE_RGBA,			// RGBA type
	24,				// 24-bit color depth
	0, 0, 0, 0, 0, 0,		// color bits ignored
	0,				// no alpha buffer
	0,				// shift bit ignored
	0,				// no accumulation buffer
	0, 0, 0, 0, 			// accum bits ignored
	32,				// 32-bit z-buffer	
	0,				// no stencil buffer
	0,				// no auxiliary buffer
	PFD_MAIN_PLANE,			// main layer
	0,				// reserved
	0, 0, 0				// layer masks ignored
    };
    int pixelformat;

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



byte	scantokey[128] = { 
//  0           1       2       3       4       5       6       7 
//  8           9       A       B       C       D       E       F 
	0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6', 
	'7',    '8',    '9',    '0',    '-',    '=',    K_BACKSPACE, 9, // 0 
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i', 
	'o',    'p',    '[',    ']',    13 ,    K_CTRL,'a',  's',      // 1 
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';', 
	'\'' ,    '`',    K_SHIFT,'\\',  'z',    'x',    'c',    'v',      // 2 
	'b',    'n',    'm',    ',',    '.',    '/',    K_SHIFT,'*', 
	K_ALT,' ',   0  ,    K_F1, K_F2, K_F3, K_F4, K_F5,   // 3 
	K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE,    0  , K_HOME, 
	K_UPARROW,K_PGUP,'-',K_LEFTARROW,'5',K_RIGHTARROW,'+',K_END, //4 
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0,0,             0,              K_F11, 
	K_F12,0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7 
};

byte	shiftscantokey[128] = { 
//  0           1       2       3       4       5       6       7 
//  8           9       A       B       C       D       E       F 
	0  ,    27,     '!',    '@',    '#',    '$',    '%',    '^', 
	'&',    '*',    '(',    ')',    '_',    '+',    K_BACKSPACE, 9, // 0 
	'Q',    'W',    'E',    'R',    'T',    'Y',    'U',    'I', 
	'O',    'P',    '{',    '}',    13 ,    K_CTRL,'A',  'S',      // 1 
	'D',    'F',    'G',    'H',    'J',    'K',    'L',    ':', 
	'"' ,    '~',    K_SHIFT,'|',  'Z',    'X',    'C',    'V',      // 2 
	'B',    'N',    'M',    '<',    '>',    '?',    K_SHIFT,'*', 
	K_ALT,' ',   0  ,    K_F1, K_F2, K_F3, K_F4, K_F5,   // 3 
	K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE,    0  , K_HOME, 
	K_UPARROW,K_PGUP,'_',K_LEFTARROW,'%',K_RIGHTARROW,'+',K_END, //4 
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0,0,             0,              K_F11, 
	K_F12,0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7 
};


/*
=======
MapKey

Map from windows to quake keynums
=======
*/
int MapKey (int key)
{
	key = (key>>16)&255;
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
void ClearAllStates (void)
{
	int		i;
	
// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
	{
		Key_Event (i, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}

void AppActivate(BOOL fActive, BOOL minimize)
/****************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
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
			if (vid_canalttab && vid_wassuspended) {
				vid_wassuspended = false;
				ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN);
				ShowWindow(mainwindow, SW_SHOWNORMAL);
				// Fix for alt-tab bug in NVidia drivers, from quakeforge
				MoveWindow(mainwindow, 0, 0, gdevmode.dmPelsWidth,
						gdevmode.dmPelsHeight, false);
			}
		}
		else if ((modestate == MS_WINDOWED) && _enable_mouse.value)
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
			if (vid_canalttab) { 
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		}
		else if ((modestate == MS_WINDOWED) && _enable_mouse.value)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
	}
}

static int MWheelAccumulator;
extern cvar_t mwheelthreshold;
extern LONG CDAudio_MessageHandler(HWND,UINT,WPARAM,LPARAM);

/* main window procedure */
LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LONG	lRet = 0;
	int	fActive, fMinimized, temp;

	if (uMsg == uMSG_MOUSEWHEEL && mwheelthreshold.value >= 1)
	{
		MWheelAccumulator += *(int *)&wParam;
		while (MWheelAccumulator >= mwheelthreshold.value)
		{
			Key_Event(K_MWHEELUP, true);
			Key_Event(K_MWHEELUP, false);
			MWheelAccumulator -= mwheelthreshold.value;
		}
		while (MWheelAccumulator <= -mwheelthreshold.value)
		{
			Key_Event(K_MWHEELDOWN, true);
			Key_Event(K_MWHEELDOWN, false);
			MWheelAccumulator += mwheelthreshold.value;
		}
	}

	switch (uMsg)
	{
		case WM_KILLFOCUS:
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
			break;

		case WM_CREATE:
			break;

		case WM_MOVE:
			if (modestate == MS_FULLDIB)
				break;	// ignore when fullscreen
			window_x = (int) LOWORD(lParam);
			window_y = (int) HIWORD(lParam);
			VID_UpdateWindowStatus ();
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			Key_Event (MapKey(lParam), true);
			break;
			
		case WM_KEYUP:
		case WM_SYSKEYUP:
			Key_Event (MapKey(lParam), false);
			break;

		case WM_SYSCHAR:
		// keep Alt-Space from happening
			break;

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			IN_MouseEvent (temp);

			break;

		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper
		// Event.
		case WM_MOUSEWHEEL: 
			if ((short) HIWORD(wParam) > 0) {
				Key_Event(K_MWHEELUP, true);
				Key_Event(K_MWHEELUP, false);
			} else {
				Key_Event(K_MWHEELDOWN, true);
				Key_Event(K_MWHEELDOWN, false);
			}
			break;

		case WM_SIZE:
			break;

		case WM_CLOSE:
			if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit",
						MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
			{
				Sys_Quit ();
			}

			break;

		case WM_ACTIVATE:
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
			AppActivate(!(fActive == WA_INACTIVE), fMinimized);

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
			ClearAllStates ();

			break;

		case WM_DESTROY:
			{
				if (dibwindow)
					DestroyWindow (dibwindow);

				PostQuitMessage (0);
			}
			break;

		case MM_MCINOTIFY:
			lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
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
int VID_NumModes (void)
{
	return nummodes;
}

	
/*
=================
VID_GetModePtr
=================
*/
vmode_t *VID_GetModePtr (int modenum)
{
	if ((modenum >= 0) && (modenum < nummodes))
		return &modelist[modenum];
	else
		return &badmode;
}


/*
=================
VID_GetModeDescription
=================
*/
char *VID_GetModeDescription (int mode)
{
	char		*pinfo;
	vmode_t		*pv;
	static char	temp[100];

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	if (!leavecurrentmode)
	{
		pv = VID_GetModePtr (mode);
		pinfo = pv->modedesc;
	}
	else
	{
		sprintf (temp, "Desktop resolution (%dx%d)",
				 modelist[MODE_FULLSCREEN_DEFAULT].width,
				 modelist[MODE_FULLSCREEN_DEFAULT].height);
		pinfo = temp;
	}

	return pinfo;
}


// KJB: Added this to return the mode driver name in description for console

char *VID_GetExtModeDescription (int mode)
{
	static char	pinfo[100];
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	if (modelist[mode].type == MS_FULLDIB)
	{
		if (!leavecurrentmode)
		{
			sprintf(pinfo,"%s fullscreen", pv->modedesc);
		}
		else
		{
			sprintf (pinfo, "Desktop resolution (%dx%d)",
					 modelist[MODE_FULLSCREEN_DEFAULT].width,
					 modelist[MODE_FULLSCREEN_DEFAULT].height);
		}
	}
	else
	{
		if (modestate == MS_WINDOWED)
			sprintf(pinfo, "%s windowed", pv->modedesc);
		else
			sprintf(pinfo, "windowed");
	}

	return pinfo;
}


/*
=================
VID_DescribeCurrentMode_f
=================
*/
void VID_DescribeCurrentMode_f (void)
{
	Con_Printf ("%s\n", VID_GetExtModeDescription (vid_modenum));
}


/*
=================
VID_NumModes_f
=================
*/
void VID_NumModes_f (void)
{
	if (nummodes == 1)
		Con_Printf ("%d video mode is available\n", nummodes);
	else
		Con_Printf ("%d video modes are available\n", nummodes);
}


/*
=================
VID_DescribeMode_f
=================
*/
void VID_DescribeMode_f (void)
{
	int	t, modenum;

	modenum = atoi (Cmd_Argv(1));

	t = leavecurrentmode;
	leavecurrentmode = 0;

	Con_Printf ("%s\n", VID_GetExtModeDescription (modenum));

	leavecurrentmode = t;
}

/*
=================
VID_DescribeModes_f
=================
*/
void VID_DescribeModes_f (void)
{
	int	i, lnummodes, t;
	char	*pinfo;
	vmode_t	*pv;

	lnummodes = VID_NumModes ();

	t = leavecurrentmode;
	leavecurrentmode = 0;

	for (i=1 ; i<lnummodes ; i++)
	{
		pv = VID_GetModePtr (i);
		pinfo = VID_GetExtModeDescription (i);
		Con_Printf ("%2d: %s\n", i, pinfo);
	}

	leavecurrentmode = t;
}


void VID_InitDIB (HINSTANCE hInstance)
{
	WNDCLASS	wc;

	/* Register the frame class */
	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC)MainWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = 0;
	wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName  = 0;
#ifndef H2W
	wc.lpszClassName = "HexenII";
#else
	wc.lpszClassName = "HexenWorld";
#endif

	if (!RegisterClass (&wc) )
		Sys_Error ("Couldn't register window class");

	modelist[0].type = MS_WINDOWED;

	if (COM_CheckParm("-width"))
		modelist[0].width = atoi(com_argv[COM_CheckParm("-width")+1]);
	else
		modelist[0].width = 640;

	if (modelist[0].width < 320)
		modelist[0].width = 320;

	if (COM_CheckParm("-height"))
		modelist[0].height= atoi(com_argv[COM_CheckParm("-height")+1]);
	else
		modelist[0].height = modelist[0].width * 240/320;

	if (modelist[0].height < 240)
		modelist[0].height = 240;

	sprintf (modelist[0].modedesc, "%dx%d",
			 modelist[0].width, modelist[0].height);

	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].dib = 1;
	modelist[0].fullscreen = 0;
	modelist[0].halfscreen = 0;
	modelist[0].bpp = 0;

	nummodes = 1;
}


/*
=================
VID_InitFullDIB
=================
*/
void VID_InitFullDIB (HINSTANCE hInstance)
{
	DEVMODE	devmode;
	int	i, modenum, originalnummodes, existingmode, numlowresmodes;
	int	j, bpp, done;
	BOOL	stat;

// enumerate >8 bpp modes
	originalnummodes = nummodes;
	modenum = 0;

	do
	{
		stat = EnumDisplaySettings (NULL, modenum, &devmode);

		if ((devmode.dmBitsPerPel >= 15) &&
			(devmode.dmPelsWidth <= MAXWIDTH) &&
			(devmode.dmPelsHeight <= MAXHEIGHT) &&
			(nummodes < MAX_MODE_LIST))
		{
			devmode.dmFields = DM_BITSPERPEL |
					   DM_PELSWIDTH |
					   DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
					DISP_CHANGE_SUCCESSFUL)
			{
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
						(int)devmode.dmPelsWidth, (int)devmode.dmPelsHeight,
						(int)devmode.dmBitsPerPel);

			// if the width is more than twice the height, reduce it by half because this
			// is probably a dual-screen monitor
				if (!COM_CheckParm("-noadjustaspect"))
				{
					if (modelist[nummodes].width > (modelist[nummodes].height << 1))
					{
						modelist[nummodes].width >>= 1;
						modelist[nummodes].halfscreen = 1;
						sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
								 modelist[nummodes].width,
								 modelist[nummodes].height,
								 modelist[nummodes].bpp);
					}
				}

				for (i=originalnummodes, existingmode = 0 ; i<nummodes ; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width)   &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp))
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
				{
					nummodes++;
				}
			}
		}

		modenum++;
	} while (stat);

// see if there are any low-res modes that aren't being reported
	numlowresmodes = sizeof(lowresmodes) / sizeof(lowresmodes[0]);
	bpp = 16;
	done = 0;

	do
	{
		for (j=0 ; (j<numlowresmodes) && (nummodes < MAX_MODE_LIST) ; j++)
		{
			devmode.dmBitsPerPel = bpp;
			devmode.dmPelsWidth = lowresmodes[j].width;
			devmode.dmPelsHeight = lowresmodes[j].height;
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
					DISP_CHANGE_SUCCESSFUL)
			{
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
						(int)devmode.dmPelsWidth, (int)devmode.dmPelsHeight,
						(int)devmode.dmBitsPerPel);

				for (i=originalnummodes, existingmode = 0 ; i<nummodes ; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width)   &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp))
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
				{
					nummodes++;
				}
			}
		}
		switch (bpp)
		{
			case 16:
				bpp = 32;
				break;

			case 32:
				bpp = 24;
				break;

			case 24:
				done = 1;
				break;
		}
	} while (!done);

	if (nummodes == originalnummodes)
		Con_SafePrintf ("No fullscreen DIB modes found\n");
}

void VID_Init8bitPalette() 
{
	// Check for 8bit Extensions and initialize them.
	int	i;
	char	thePalette[256*3];
	char	*oldPalette, *newPalette;

	MyglColorTableEXT = (void *)wglGetProcAddress_fp("glColorTableEXT");
	if (MyglColorTableEXT && strstr(gl_extensions, "GL_EXT_shared_texture_palette"))
	{
		Con_SafePrintf("8-bit GL extensions enabled.\n");
		glEnable_fp( GL_SHARED_TEXTURE_PALETTE_EXT );
		oldPalette = (char *) d_8to24table; //d_8to24table3dfx;
		newPalette = thePalette;
		for (i=0;i<256;i++) {
			*newPalette++ = *oldPalette++;
			*newPalette++ = *oldPalette++;
			*newPalette++ = *oldPalette++;
			oldPalette++;
		}
		MyglColorTableEXT(GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, 256, GL_RGB, GL_UNSIGNED_BYTE,
			(void *) thePalette);
		is8bit = TRUE;
	}
}

/*
===================
VID_Init
===================
*/
void	VID_Init (unsigned char *palette)
{
	int	i, existingmode;
	int	basenummodes, width, height, bpp, findbpp, done;
	char	gldir[MAX_OSPATH];
	HDC	hdc;
	DEVMODE	devmode;

	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&vid_wait);
	Cvar_RegisterVariable (&vid_nopageflip);
	Cvar_RegisterVariable (&_vid_wait_override);
	Cvar_RegisterVariable (&_vid_default_mode);
	Cvar_RegisterVariable (&_vid_default_mode_win);
	Cvar_RegisterVariable (&vid_config_x);
	Cvar_RegisterVariable (&vid_config_y);
	Cvar_RegisterVariable (&vid_stretch_by_2);
	Cvar_RegisterVariable (&_enable_mouse);
	Cvar_RegisterVariable (&gl_ztrick);
	Cvar_RegisterVariable (&gl_purge_maptex);

	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f);
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON2));

	//InitCommonControls();
	VID_SetPalette (palette);

#ifdef GL_DLSYM
	gl_library = "opengl32.dll";
	i = COM_CheckParm("-gllibrary");
	if (i && i < com_argc - 1)
		gl_library = com_argv[i+1];
	if (!GL_OpenLibrary(gl_library))
		Sys_Error ("Unable to load GL library %s", gl_library);
#endif

	VID_InitDIB (global_hInstance);
	basenummodes = nummodes = 1;

	VID_InitFullDIB (global_hInstance);

	if (COM_CheckParm("-window"))
	{
		hdc = GetDC (NULL);

		if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
		{
			Sys_Error ("Can't run in non-RGB mode");
		}

		ReleaseDC (NULL, hdc);

		windowed = true;

		vid_default = MODE_WINDOWED;
	}
	else
	{
		if (nummodes == 1)
			Sys_Error ("No RGB fullscreen modes available");

		windowed = false;

		if (COM_CheckParm("-mode"))
		{
			vid_default = atoi(com_argv[COM_CheckParm("-mode")+1]);
		}
		else
		{
			if (COM_CheckParm("-current"))
			{
				modelist[MODE_FULLSCREEN_DEFAULT].width =
						GetSystemMetrics (SM_CXSCREEN);
				modelist[MODE_FULLSCREEN_DEFAULT].height =
						GetSystemMetrics (SM_CYSCREEN);
				vid_default = MODE_FULLSCREEN_DEFAULT;
				leavecurrentmode = 1;
			}
			else
			{
				if (COM_CheckParm("-width"))
				{
					width = atoi(com_argv[COM_CheckParm("-width")+1]);
				}
				else
				{
					width = 640;
				}

				if (COM_CheckParm("-bpp"))
				{
					bpp = atoi(com_argv[COM_CheckParm("-bpp")+1]);
					findbpp = 0;
				}
				else
				{
					bpp = 15;
					findbpp = 1;
				}

				if (COM_CheckParm("-height"))
					height = atoi(com_argv[COM_CheckParm("-height")+1]);

			// if they want to force it, add the specified mode to the list
				if (COM_CheckParm("-force") && (nummodes < MAX_MODE_LIST))
				{
					modelist[nummodes].type = MS_FULLDIB;
					modelist[nummodes].width = width;
					modelist[nummodes].height = height;
					modelist[nummodes].modenum = 0;
					modelist[nummodes].halfscreen = 0;
					modelist[nummodes].dib = 1;
					modelist[nummodes].fullscreen = 1;
					modelist[nummodes].bpp = bpp;
					sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
							(int)devmode.dmPelsWidth, (int)devmode.dmPelsHeight,
							(int)devmode.dmBitsPerPel);

					for (i=nummodes, existingmode = 0 ; i<nummodes ; i++)
					{
						if ((modelist[nummodes].width == modelist[i].width)   &&
							(modelist[nummodes].height == modelist[i].height) &&
							(modelist[nummodes].bpp == modelist[i].bpp))
						{
							existingmode = 1;
							break;
						}
					}

					if (!existingmode)
					{
						nummodes++;
					}
				}

				done = 0;

				do
				{
					if (COM_CheckParm("-height"))
					{
						height = atoi(com_argv[COM_CheckParm("-height")+1]);

						for (i=1, vid_default=0 ; i<nummodes ; i++)
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
					}
					else
					{
						for (i=1, vid_default=0 ; i<nummodes ; i++)
						{
							if ((modelist[i].width == width) && (modelist[i].bpp == bpp))
							{
								vid_default = i;
								done = 1;
								break;
							}
						}
					}

					if (!done)
					{
						if (findbpp)
						{
							switch (bpp)
							{
							case 15:
								bpp = 16;
								break;
							case 16:
								bpp = 32;
								break;
							case 32:
								bpp = 24;
								break;
							case 24:
								done = 1;
								break;
							}
						}
						else
						{
							done = 1;
						}
					}
				} while (!done);

				if (!vid_default)
				{
					Sys_Error ("Specified video mode not available");
				}
			}
		}
	}

	vid_initialized = true;

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

#if !defined(NO_SPLASHES)
	DestroyWindow (hwnd_dialog);
#endif

	VID_SetMode (vid_default, palette);

	maindc = GetDC(mainwindow);
	bSetupPixelFormat(maindc);

	baseRC = wglCreateContext_fp( maindc );
	if (!baseRC)
		Sys_Error ("Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.");
	if (!wglMakeCurrent_fp( maindc, baseRC ))
		Sys_Error ("wglMakeCurrent failed");

	GL_Init ();

	if (GetDeviceGammaRamp_f)
		gammaworks = GetDeviceGammaRamp_f(maindc, orig_ramps);
	else
		gammaworks = false;
	if (!gammaworks)
	{
		// we can still adjust the brightness...
		Con_Printf("gamma not available, using gl tricks\n");
		gl_dogamma = true;
	}

	sprintf (gldir, "%s/glhexen", com_gamedir);
	Sys_mkdir (gldir);
	sprintf (gldir, "%s/glhexen/boss", com_gamedir);
	Sys_mkdir (gldir);
	sprintf (gldir, "%s/glhexen/puzzle", com_gamedir);
	Sys_mkdir (gldir);

	vid_realmode = vid_modenum;

	VID_SetPalette (palette);
	
	// Check for 3DFX Extensions and initialize them.
	if (COM_CheckParm("-paltex"))
		VID_Init8bitPalette();

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	strcpy (badmode.modedesc, "Bad mode");
	vid_canalttab = true;

	if (COM_CheckParm("-fullsbar"))
		fullsbardraw = true;
}


//========================================================
// Video menu stuff
//========================================================

extern void M_Menu_Options_f (void);
extern void M_Print (int cx, int cy, char *str);
extern void M_PrintWhite (int cx, int cy, char *str);
extern void M_DrawCharacter (int cx, int line, int num);
extern void M_DrawTransPic (int x, int y, qpic_t *pic);
extern void M_DrawPic (int x, int y, qpic_t *pic);

static int	vid_wmodes;

typedef struct
{
	int	modenum;
	char	*desc;
	int	iscur;
} modedesc_t;

#define MAX_COLUMN_SIZE		9
#define MODE_AREA_HEIGHT	(MAX_COLUMN_SIZE + 2)
#define MAX_MODEDESCS		(MAX_COLUMN_SIZE*3)

static modedesc_t	modedescs[MAX_MODEDESCS];

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	char	*ptr;
	int	lnummodes, i, k, column, row;
	vmode_t	*pv;

	ScrollTitle("gfx/menu/title7.lmp");

	vid_wmodes = 0;
	lnummodes = VID_NumModes ();
	
	for (i=1 ; (i<lnummodes) && (vid_wmodes < MAX_MODEDESCS) ; i++)
	{
		ptr = VID_GetModeDescription (i);
		pv = VID_GetModePtr (i);

		k = vid_wmodes;

		modedescs[k].modenum = i;
		modedescs[k].desc = ptr;
		modedescs[k].iscur = 0;

		if (i == vid_modenum)
			modedescs[k].iscur = 1;

		vid_wmodes++;

	}

	if (vid_wmodes > 0)
	{
		M_Print (2*8, 60+0*8, "Fullscreen Modes (WIDTHxHEIGHTxBPP)");

		column = 8;
		row = 60+2*8;

		for (i=0 ; i<vid_wmodes ; i++)
		{
			if (modedescs[i].iscur)
				M_PrintWhite (column, row, modedescs[i].desc);
			else
				M_Print (column, row, modedescs[i].desc);

			column += 13*8;

			if ((i % VID_ROW_SIZE) == (VID_ROW_SIZE - 1))
			{
				column = 8;
				row += 8;
			}
		}
	}

	M_Print (3*8, 60 + MODE_AREA_HEIGHT * 8 + 8*2,
			 "Video modes must be set from the");
	M_Print (3*8, 60 + MODE_AREA_HEIGHT * 8 + 8*3,
			 "command line with -width <width>");
	M_Print (3*8, 60 + MODE_AREA_HEIGHT * 8 + 8*4,
			 "and -bpp <bits-per-pixel>");
	M_Print (3*8, 60 + MODE_AREA_HEIGHT * 8 + 8*6,
			 "Select windowed mode with -window");
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
		S_LocalSound ("raven/menu1.wav");
		M_Menu_Options_f ();
		break;

	default:
		break;
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

	glDrawBuffer_fp (GL_BACK);
}
#endif

void ToggleFullScreenSA(void)
{
}

