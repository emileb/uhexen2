/*
   gl_dl_vidsdl.c
   $Header: /home/ozzie/Download/0000/uhexen2/hexen2/gl_vidsdl.c,v 1.17 2005-01-07 07:38:14 sezero Exp $

	Select window size and mode and init SDL in GL mode.

	Changed 7/11/04 by S.A.
	- Fixed fullscreen opengl mode, window sizes
	- Options are now:
	- fullscreen | -window, -height , -width , -bpp

	Changed 27/12/04
	- Fullscreen modes are normally 3-5, but we only use the traditional
	- modes 0 and 3. And mode 3 has been changed to "1" to allow it
	- to be represented as a boolean (and a menu selection)

	cvar_t vid_mode is either:
	MODE_WINDOWED			0
	MODE_FULLSCREEN_DEFAULT	1

	This file is a bit of a mess...
	The "-mode" option has been removed
*/

#include "quakedef.h"
#include "winquake.h"
#include "resource.h"

#include "SDL.h"
#include <dlfcn.h>

#define MAX_MODE_LIST	30
#define VID_ROW_SIZE	3
#define WARP_WIDTH		320
#define WARP_HEIGHT		200
#define MAXWIDTH		10000
#define MAXHEIGHT		10000
#define BASEWIDTH		320
#define BASEHEIGHT		200

#define MODE_WINDOWED			0
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 1)

SDL_Surface *screen;

glfunc_t  glfunc;

byte globalcolormap[VID_GRADES*256];

extern qboolean grab;
extern qboolean is_3dfx;
extern qboolean is_PowerVR;

cvar_t gl_purge_maptex = {"gl_purge_maptex", "1", true};
		/* whether or not map-specific OGL textures
		   are flushed from map. default == yes  */
extern int numgltextures;

qboolean in_mode_set = false;

typedef struct {
	modestate_t	type;
	int			width;
	int			height;
	int			modenum;
	int			dib;
	int			fullscreen;
	int			bpp;
	int			halfscreen;
	char		modedesc[17];
} vmode_t;

typedef struct {
	int			width;
	int			height;
} lmode_t;

lmode_t	lowresmodes[] = {
	{320, 200},
	{320, 240},
	{400, 300},
	{512, 384},
};

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;
char *gl_library;


qboolean	DDActive;
qboolean	scr_skipupdate;

static vmode_t	modelist[MAX_MODE_LIST];
static int	nummodes;
static vmode_t	badmode;

#if 0
static DEVMODE	gdevmode;
#endif
qboolean	vid_initialized = false;
static qboolean	windowed, leavecurrentmode;
static int		windowed_mouse;
#if 0
static HICON	hIcon;
#endif

//FX_DISPLAY_MODE_EXT fxDisplayModeExtension;
//FX_SET_PALETTE_EXT fxSetPaletteExtension;
//FX_MARK_PAL_TEXTURE_EXT fxMarkPalTextureExtension;

unsigned char inverse_pal[(1<<INVERSE_PAL_TOTAL_BITS)+1];

int		DIBWidth, DIBHeight;
int		WRHeight, WRWidth;

#if 0
RECT		WindowRect;
DWORD		WindowStyle, ExWindowStyle;
HWND		mainwindow, dibwindow;
#endif

int		vid_modenum = NO_MODE;
int		vid_realmode;
int		vid_default = MODE_WINDOWED;
static int	windowed_default;

unsigned char	vid_curpal[256*3];
float		RTint[256],GTint[256],BTint[256];

#if 0
HGLRC		baseRC;
HDC		maindc;
#endif

glvert_t	glv;

cvar_t		gl_ztrick = {"gl_ztrick","0",true};

#if 0
HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);
#endif

viddef_t	vid;				// global video state

unsigned short	d_8to16table[256];
unsigned	d_8to24table[256];
unsigned	d_8to24TranslucentTable[256];

float		gldepthmin, gldepthmax;

modestate_t	modestate = MS_UNINIT;

void VID_MenuDraw (void);
void VID_MenuKey (int key);

#if 0
LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AppActivate(BOOL fActive, BOOL minimize);
#endif
char *VID_GetModeDescription (int mode);
void ClearAllStates (void);
void VID_UpdateWindowStatus (void);
void GL_Init (void);
void GL_Init_Functions(void);
void VID_SetGamma(float value);
void VID_SetGamma_f(void);

#if 0
PROC glArrayElementEXT;
PROC glColorPointerEXT;
PROC glTexCoordPointerEXT;
PROC glVertexPointerEXT;
#endif

//====================================

cvar_t		vid_mode = {"vid_mode","0", false};
// Note that 0 is MODE_WINDOWED
cvar_t		_vid_default_mode = {"_vid_default_mode","0", true};
cvar_t		_vid_default_mode_win = {"_vid_default_mode_win","3", true};
cvar_t		vid_wait = {"vid_wait","0"};
cvar_t		vid_nopageflip = {"vid_nopageflip","0", true};
cvar_t		_vid_wait_override = {"_vid_wait_override", "0", true};
cvar_t		vid_config_x = {"vid_config_x","800", true};
cvar_t		vid_config_y = {"vid_config_y","600", true};
cvar_t		vid_stretch_by_2 = {"vid_stretch_by_2","1", true};
cvar_t		_windowed_mouse = {"_windowed_mouse","1", true};

int		window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;

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


#if 0
void CenterWindow(HWND hWndCenter, int width, int height, BOOL lefttopjustify)
{
    RECT    rect;
    int     CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	if (CenterX > CenterY*2)
		CenterX >>= 1;	// dual screens
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;
	SetWindowPos (hWndCenter, NULL, CenterX, CenterY, 0, 0,
			SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
}
#endif

/* Init SDL */

qboolean VID_SetWindowedMode (int modenum)
{
	// handle both fullscreen and windowed modes -S.A

	Uint32 flags;

	/* SDL doco recons you need this. S.A.
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 5 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 5 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 5 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 0 );
	*/

	if (modenum == MODE_FULLSCREEN_DEFAULT )
		flags = (SDL_OPENGL|SDL_FULLSCREEN);
	else 
		flags = (SDL_OPENGL);
		// flags = (SDL_OPENGL|SDL_NOFRAME);

	// debug printf ("VID_SetWindowedMode, modenum %i",modenum);

	// Linux version only uses modes 0, 3
	if (modenum==0)
		modestate = MS_WINDOWED;
	else	
		modestate = MS_FULLDIB;
	
	WRHeight = vid.height = vid.conheight = modelist[modenum].height; // BASEHEIGHT;
	WRWidth = vid.width = vid.conwidth =   modelist[modenum].width; //  BASEWIDTH ;

	vid.numpages = 2;
	
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		return false;

	if (SDL_GL_LoadLibrary(gl_library) < 0) 
		Sys_Error("VID: Couldn't load SDL: %s", SDL_GetError());
	
	if (!(screen = SDL_SetVideoMode (vid.width,vid.height,modelist[modenum].bpp, flags)))
		return false;

	// acknowledge Portal of Praevus S.A
#ifdef H2MP
	SDL_WM_SetCaption ("Portal of Praevus", "PRAEVUS");
#else
	SDL_WM_SetCaption ("Hexen II", "HEXEN2");
#endif
	return true;
}

// procedure VID_SetFullDIBMode is history
qboolean VID_SetFullDIBMode (int modenum) {
	return VID_SetWindowedMode (modenum);
}

int VID_SetMode (int modenum, unsigned char *palette)
{
	int				original_mode, temp;
	qboolean		stat;

	// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

#warning Is this correct?!?
#if 0
	Snd_ReleaseBuffer ();
#endif
	CDAudio_Pause ();

	if (vid_modenum == NO_MODE)
		original_mode = windowed_default;
	else
		original_mode = vid_modenum;

	if (modelist[modenum].type == MS_WINDOWED)
	{
		if (_windowed_mouse.value)
		{
			//debug puts ("trace1\n");
			stat = VID_SetWindowedMode(modenum);
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
		else
		{
			//debug puts ("trace2\n");
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
		Sys_Error ("VID_SetMode: Bad mode type in modelist");

	window_width = DIBWidth;
	window_height = DIBHeight;
	VID_UpdateWindowStatus ();

	CDAudio_Resume ();
#warning Is this correct?!?
#if 0
	Snd_AcquireBuffer ();
#endif
	scr_disabled_for_loading = temp;

	if (!stat)
	{
		Sys_Error ("Couldn't set video mode: %s", SDL_GetError());
	}

// now we try to make sure we get the focus on the mode switch, because
// sometimes in some systems we don't.  We grab the foreground, then
// finish setting up, pump all our messages, and sleep for a little while
// to let messages finish bouncing around the system, then we put
// ourselves at the top of the z order, then grab the foreground again,
// Who knows if it helps, but it probably doesn't hurt
#if 0
	SetForegroundWindow (mainwindow);
#endif
	VID_SetPalette (palette);
	vid_modenum = modenum;
	Cvar_SetValue ("vid_mode", (float)vid_modenum);

#if 0
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
#endif
// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	if (!msg_suppress_1)
		Con_SafePrintf ("%s\n", VID_GetModeDescription (vid_modenum));

	VID_SetPalette (palette);

	vid.recalc_refdef = 1;

	return true;
}


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

#if 0
BINDTEXFUNCPTR bindTexFunc;
#else
void *bindTexFunc;
#endif

#define TEXTURE_EXT_STRING "GL_EXT_texture_object"

#define FX_DISPLAY_MODE_EXT_STRING "gl3DfxDisplayModeEXT"

//#define FX_SET_PALETTE_EXT_STRING "gl3DfxSetPaletteEXT"
#define FX_SET_PALETTE_EXT_STRING "3DFX_set_global_palette"
#define VR_SET_PALETTE_EXT_STRING "POWERVR_set_global_palette"

//#define FX_MARK_PAL_TEXTURE_EXT_STRING "gl3DfxMarkPalettizedTextureEXT"

#define TRUE 1
#define FALSE 0

void Check3DfxDisplayModeExtension( void )
{
#warning STUB
#if 0
	char *tmp;
	qboolean display_mode_ext = false;

	tmp = ( unsigned char * )glGetString( GL_EXTENSIONS );
	while( *tmp )
	{
		if (strncmp((const char*)tmp, FX_DISPLAY_MODE_EXT_STRING, strlen(FX_DISPLAY_MODE_EXT_STRING)) == 0)
			display_mode_ext = TRUE;
		tmp++;
	}

	fxDisplayModeExtension = NULL;
	if( !display_mode_ext )
		return;
	if ((fxDisplayModeExtension = (FX_DISPLAY_MODE_EXT)
		wglGetProcAddress((LPCSTR) FX_DISPLAY_MODE_EXT_STRING)) == NULL)
	{
		Sys_Error ("GetProcAddress for gl3DfxDisplayModeExt failed");
		return;
	}
#endif
}

void CheckSetPaletteExtension( void )
{
        char *tmp;
	qboolean set_palette_ext = false;
	char *search;

#if 0
	fxSetPaletteExtension = NULL;
#endif
	
	search = NULL;
	if (is_3dfx) search = FX_SET_PALETTE_EXT_STRING;
	if (is_PowerVR) search = VR_SET_PALETTE_EXT_STRING;
	if (!search) return;


	tmp = ( unsigned char * )glfunc.glGetString_fp( GL_EXTENSIONS );
	while( *tmp )
	{
		if (strncmp((const char*)tmp, search, strlen(search)) == 0)
		{
			Con_Printf("Using palettized textures!\n");
			set_palette_ext = TRUE;
		}
		tmp++;
	}

	if( !set_palette_ext )
		return;
#if 0
	if ((fxSetPaletteExtension = (FX_SET_PALETTE_EXT)
		wglGetProcAddress((LPCSTR) search)) == NULL)
	{
		Sys_Error ("GetProcAddress for gl3DfxSetPaletteEXT failed");
		return;
	}
#endif
}

/*
void Check3DfxMarkPaletteTextureExtension( void )
{
	char *tmp;
	qboolean found_ext = false;

	tmp = ( unsigned char * )glGetString( GL_EXTENSIONS );
	while( *tmp )
	{
		if (strncmp((const char*)tmp, FX_SET_PALETTE_EXT_STRING, strlen(FX_SET_PALETTE_EXT_STRING)) == 0)
			found_ext = TRUE;
		tmp++;
	}

	fxMarkPalTextureExtension = NULL;
	if( !found_ext )
		return;
	if ((fxMarkPalTextureExtension = (FX_MARK_PAL_TEXTURE_EXT)
		wglGetProcAddress((LPCSTR) FX_MARK_PAL_TEXTURE_EXT_STRING)) == NULL)
	{
		Sys_Error ("GetProcAddress for fxMarkPalTextureExtension failed");
		return;
	}
}
*/

void CheckTextureExtensions (void)
{
	char		*tmp;
	qboolean	texture_ext;
#if 0
	HINSTANCE	hInstGL;
#endif

	texture_ext = FALSE;
	/* check for texture extension */
	tmp = (unsigned char *)glfunc.glGetString_fp(GL_EXTENSIONS);
	while (*tmp)
	{
		if (strncmp((const char*)tmp, TEXTURE_EXT_STRING, strlen(TEXTURE_EXT_STRING)) == 0)
			texture_ext = TRUE;
		tmp++;
	}

	if (!texture_ext || COM_CheckParm ("-gl11") )
	{
#if 0
		hInstGL = LoadLibrary("opengl32.dll");
		
		if (hInstGL == NULL)
			Sys_Error ("Couldn't load opengl32.dll\n");
#endif

#if 0
		bindTexFunc = (void *)SDL_GL_GetProcAddress(hInstGL,"glBindTexture");
#else
		bindTexFunc = (void *)SDL_GL_GetProcAddress("glBindTexture");
#endif
		if (!bindTexFunc)
			Sys_Error ("No texture objects!");
		return;
	}

/* load library and get procedure adresses for texture extension API */
#if 0
	if ((bindTexFunc = (BINDTEXFUNCPTR)
#else
	if ((bindTexFunc =
#endif
		SDL_GL_GetProcAddress("glBindTextureEXT")) == NULL)
	{
		Sys_Error ("GetProcAddress for BindTextureEXT failed");
		return;
	}
}

void CheckArrayExtensions (void)
{
	char		*tmp;

	/* check for texture extension */
	tmp = (unsigned char *)glfunc.glGetString_fp(GL_EXTENSIONS);
	while (*tmp)
	{
		if (strncmp((const char*)tmp, "GL_EXT_vertex_array", strlen("GL_EXT_vertex_array")) == 0)
		{
			if (
((SDL_GL_GetProcAddress("glArrayElementEXT")) == NULL) ||
((SDL_GL_GetProcAddress("glColorPointerEXT")) == NULL) ||
((SDL_GL_GetProcAddress("glTexCoordPointerEXT")) == NULL) ||
((SDL_GL_GetProcAddress("glVertexPointerEXT")) == NULL) )
			{
				Sys_Error ("GetProcAddress for vertex extension failed");
				return;
			}
			return;
		}
		tmp++;
	}

	Sys_Error ("Vertex array extension not present");
}

//int		texture_mode = GL_NEAREST;
//int		texture_mode = GL_NEAREST_MIPMAP_NEAREST;
//int		texture_mode = GL_NEAREST_MIPMAP_LINEAR;
int		texture_mode = GL_LINEAR;
//int		texture_mode = GL_LINEAR_MIPMAP_NEAREST;
//int		texture_mode = GL_LINEAR_MIPMAP_LINEAR;

int		texture_extension_number = 1;

/*
===============
GL_Init
===============
*/
void GL_Init (void)
{
        GL_Init_Functions();
	gl_vendor = glfunc.glGetString_fp (GL_VENDOR);
	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = glfunc.glGetString_fp (GL_RENDERER);
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);

	gl_version = glfunc.glGetString_fp (GL_VERSION);
	Con_Printf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = glfunc.glGetString_fp (GL_EXTENSIONS);
	// Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);

	if (!Q_strncasecmp ((char *)gl_renderer, "3dfx",4))
	{
		is_3dfx = true;
	}

	if (!Q_strncasecmp ((char *)gl_renderer, "PowerVR PCX1",12) ||
		!Q_strncasecmp ((char *)gl_renderer, "PowerVR PCX2",12))
	{
		is_PowerVR = true;
	}

	CheckTextureExtensions ();

    	//fxDisplayModeExtension = NULL;
	//fxSetPaletteExtension = NULL;
	//fxMarkPalTextureExtension = NULL;
	Check3DfxDisplayModeExtension();
	CheckSetPaletteExtension();
	//Check3DfxMarkPaletteTextureExtension();	

	glfunc.glClearColor_fp (1,0,0,0);
	glfunc.glCullFace_fp(GL_FRONT);
	glfunc.glEnable_fp(GL_TEXTURE_2D);

	glfunc.glEnable_fp(GL_ALPHA_TEST);
	glfunc.glAlphaFunc_fp(GL_GREATER, 0.666);

	glfunc.glPolygonMode_fp (GL_FRONT_AND_BACK, GL_FILL);
	glfunc.glShadeModel_fp (GL_FLAT);

	glfunc.glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glfunc.glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// previously GL_CLAMP was GL_REPEAT S.A
	glfunc.glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glfunc.glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glfunc.glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glfunc.glTexEnvf_fp(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

#if 0
	CheckArrayExtensions ();

	glfunc.glEnable_fp (GL_VERTEX_ARRAY_EXT);
	glfunc.glEnable_fp (GL_TEXTURE_COORD_ARRAY_EXT);
	glVertexPointerEXT (3, GL_FLOAT, 0, 0, &glv.x);
	glTexCoordPointerEXT (2, GL_FLOAT, 0, 0, &glv.s);
	glColorPointerEXT (3, GL_FLOAT, 0, 0, &glv.r);
#endif
}

void GL_Init_Functions(void)
{
  glfunc.glBegin_fp = (glBegin_f) SDL_GL_GetProcAddress("glBegin");
  if (glfunc.glBegin_fp == 0) {Sys_Error("glBegin not found in GL library");}
  glfunc.glEnd_fp = (glEnd_f) SDL_GL_GetProcAddress("glEnd");
  if (glfunc.glEnd_fp == 0) {Sys_Error("glEnd not found in GL library");}
  glfunc.glEnable_fp = (glEnable_f) SDL_GL_GetProcAddress("glEnable");
  if (glfunc.glEnable_fp == 0) {Sys_Error("glEnable not found in GL library");}
  glfunc.glDisable_fp = (glDisable_f) SDL_GL_GetProcAddress("glDisable");
  if (glfunc.glDisable_fp == 0) {Sys_Error("glDisable not found in GL library");}
  glfunc.glFinish_fp = (glFinish_f) SDL_GL_GetProcAddress("glFinish");
  if (glfunc.glFinish_fp == 0) {Sys_Error("glFinish not found in GL library");}
  glfunc.glClear_fp = (glClear_f) SDL_GL_GetProcAddress("glClear");
  if (glfunc.glClear_fp == 0) {Sys_Error("glClear not found in GL library");}
  
  glfunc.glOrtho_fp = (glOrtho_f) SDL_GL_GetProcAddress("glOrtho");
  if (glfunc.glOrtho_fp == 0) {Sys_Error("glOrtho not found in GL library");}
  glfunc.glFrustum_fp = (glFrustum_f) SDL_GL_GetProcAddress("glFrustum");
  if (glfunc.glFrustum_fp == 0) {Sys_Error("glFrustum not found in GL library");}
  glfunc.glViewport_fp = (glViewport_f) SDL_GL_GetProcAddress("glViewport");
  if (glfunc.glViewport_fp == 0) {Sys_Error("glViewport not found in GL library");}
  glfunc.glPushMatrix_fp = (glPushMatrix_f) SDL_GL_GetProcAddress("glPushMatrix");
  if (glfunc.glPushMatrix_fp == 0) {Sys_Error("glPushMatrix not found in GL library");}
  glfunc.glPopMatrix_fp = (glPopMatrix_f) SDL_GL_GetProcAddress("glPopMatrix");
  if (glfunc.glPopMatrix_fp == 0) {Sys_Error("glPopMatrix not found in GL library");}
  glfunc.glLoadIdentity_fp = (glLoadIdentity_f) SDL_GL_GetProcAddress("glLoadIdentity");
  if (glfunc.glLoadIdentity_fp == 0) {Sys_Error("glLoadIdentity not found in GL library");}
  glfunc.glMatrixMode_fp = (glMatrixMode_f) SDL_GL_GetProcAddress("glMatrixMode");
  if (glfunc.glMatrixMode_fp == 0) {Sys_Error("glMatrixMode not found in GL library");}
  glfunc.glLoadMatrixf_fp = (glLoadMatrixf_f) SDL_GL_GetProcAddress("glLoadMatrixf");
  if (glfunc.glLoadMatrixf_fp == 0) {Sys_Error("glLoadMatrixf not found in GL library");}

  glfunc.glVertex2f_fp = (glVertex2f_f) SDL_GL_GetProcAddress("glVertex2f");
  if (glfunc.glVertex2f_fp == 0) {Sys_Error("glVertex2f not found in GL library");}
  glfunc.glVertex3f_fp = (glVertex3f_f) SDL_GL_GetProcAddress("glVertex3f");
  if (glfunc.glVertex3f_fp == 0) {Sys_Error("glVertex3f not found in GL library");}
  glfunc.glVertex3fv_fp = (glVertex3fv_f) SDL_GL_GetProcAddress("glVertex3fv");
  if (glfunc.glVertex3fv_fp == 0) {Sys_Error("glVertex3fv not found in GL library");}
  glfunc.glTexCoord2f_fp = (glTexCoord2f_f) SDL_GL_GetProcAddress("glTexCoord2f");
  if (glfunc.glTexCoord2f_fp == 0) {Sys_Error("glTexCoord2f not found in GL library");}
  glfunc.glTexCoord3f_fp = (glTexCoord3f_f) SDL_GL_GetProcAddress("glTexCoord3f");
  if (glfunc.glTexCoord3f_fp == 0) {Sys_Error("glTexCoord3f not found in GL library");}
  glfunc.glColor4f_fp = (glColor4f_f) SDL_GL_GetProcAddress("glColor4f");
  if (glfunc.glColor4f_fp == 0) {Sys_Error("glColor4f not found in GL library");}
  glfunc.glColor4fv_fp = (glColor4fv_f) SDL_GL_GetProcAddress("glColor4fv");
  if (glfunc.glColor4fv_fp == 0) {Sys_Error("glColor4fv not found in GL library");}
  glfunc.glColor4ubv_fp = (glColor4ubv_f) SDL_GL_GetProcAddress("glColor4ubv");
  if (glfunc.glFrustum_fp == 0) {Sys_Error("glColor4ubv not found in GL library");}
  glfunc.glColor3f_fp = (glColor3f_f) SDL_GL_GetProcAddress("glColor3f");
  if (glfunc.glColor3f_fp == 0) {Sys_Error("glColor3f not found in GL library");}
  glfunc.glColor3ubv_fp = (glColor3ubv_f) SDL_GL_GetProcAddress("glColor3ubv");
  if (glfunc.glColor3ubv_fp == 0) {Sys_Error("glColor3ubv not found in GL library");}
  glfunc.glClearColor_fp = (glClearColor_f) SDL_GL_GetProcAddress("glClearColor");
  if (glfunc.glClearColor_fp == 0) {Sys_Error("glClearColor not found in GL library");}

  glfunc.glRotatef_fp = (glRotatef_f) SDL_GL_GetProcAddress("glRotatef");
  if (glfunc.glRotatef_fp == 0) {Sys_Error("glRotatef not found in GL library");}
  glfunc.glTranslatef_fp = (glTranslatef_f) SDL_GL_GetProcAddress("glTranslatef");
  if (glfunc.glTranslatef_fp == 0) {Sys_Error("glTranslatef not found in GL library");}

  glfunc.glBindTexture_fp = (glBindTexture_f) SDL_GL_GetProcAddress("glBindTexture");
  if (glfunc.glBindTexture_fp == 0) {Sys_Error("glBindTexture not found in GL library");}
  glfunc.glDeleteTextures_fp = (glDeleteTextures_f) SDL_GL_GetProcAddress("glDeleteTextures");
  if (glfunc.glDeleteTextures_fp == 0) {Sys_Error("glDeleteTextures not found in GL library");}
  glfunc.glTexParameterf_fp = (glTexParameterf_f) SDL_GL_GetProcAddress("glTexParameterf");
  if (glfunc.glTexParameterf_fp == 0) {Sys_Error("glTexParameterf not found in GL library");}
  glfunc.glTexEnvf_fp = (glTexEnvf_f) SDL_GL_GetProcAddress("glTexEnvf");
  if (glfunc.glTexEnvf_fp == 0) {Sys_Error("glTexEnvf not found in GL library");}
  glfunc.glScalef_fp = (glScalef_f) SDL_GL_GetProcAddress("glScalef");
  if (glfunc.glScalef_fp == 0) {Sys_Error("glScalef not found in GL library");}
  glfunc.glTexImage2D_fp = (glTexImage2D_f) SDL_GL_GetProcAddress("glTexImage2D");
  if (glfunc.glTexImage2D_fp == 0) {Sys_Error("glTexImage2D not found in GL library");}

  glfunc.glAlphaFunc_fp = (glAlphaFunc_f) SDL_GL_GetProcAddress("glAlphaFunc");
  if (glfunc.glAlphaFunc_fp == 0) {Sys_Error("glAlphaFunc not found in GL library");}
  glfunc.glBlendFunc_fp = (glBlendFunc_f) SDL_GL_GetProcAddress("glBlendFunc");
  if (glfunc.glBlendFunc_fp == 0) {Sys_Error("glBlendFunc not found in GL library");}
  glfunc.glShadeModel_fp = (glShadeModel_f) SDL_GL_GetProcAddress("glShadeModel");
  if (glfunc.glShadeModel_fp == 0) {Sys_Error("glShadeModel not found in GL library");}
  glfunc.glPolygonMode_fp = (glPolygonMode_f) SDL_GL_GetProcAddress("glPolygonMode");
  if (glfunc.glPolygonMode_fp == 0) {Sys_Error("glPolygonMode not found in GL library");}
  glfunc.glDepthMask_fp = (glDepthMask_f) SDL_GL_GetProcAddress("glDepthMask");
  if (glfunc.glDepthMask_fp == 0) {Sys_Error("glDepthMask not found in GL library");}
  glfunc.glDepthRange_fp = (glDepthRange_f) SDL_GL_GetProcAddress("glDepthRange");
  if (glfunc.glDepthRange_fp == 0) {Sys_Error("glDepthRange not found in GL library");}
  glfunc.glDepthFunc_fp = (glDepthFunc_f) SDL_GL_GetProcAddress("glDepthFunc");
  if (glfunc.glDepthFunc_fp == 0) {Sys_Error("glDepthFunc not found in GL library");}

  glfunc.glDrawBuffer_fp = (glDrawBuffer_f) SDL_GL_GetProcAddress("glDrawBuffer");
  if (glfunc.glDrawBuffer_fp == 0) {Sys_Error("glDrawBuffer not found in GL library");}
  glfunc.glReadBuffer_fp = (glDrawBuffer_f) SDL_GL_GetProcAddress("glReadBuffer");
  if (glfunc.glReadBuffer_fp == 0) {Sys_Error("glReadBuffer not found in GL library");}
  glfunc.glReadPixels_fp = (glReadPixels_f) SDL_GL_GetProcAddress("glReadPixels");
  if (glfunc.glReadPixels_fp == 0) {Sys_Error("glReadPixels not found in GL library");}
  glfunc.glHint_fp = (glHint_f) SDL_GL_GetProcAddress("glHint");
  if (glfunc.glHint_fp == 0) {Sys_Error("glHint not found in GL library");}
  glfunc.glCullFace_fp = (glCullFace_f) SDL_GL_GetProcAddress("glCullFace");
  if (glfunc.glCullFace_fp == 0) {Sys_Error("glCullFace not found in GL library");}

  glfunc.glGetIntegerv_fp = (glGetIntegerv_f) SDL_GL_GetProcAddress("glGetIntegerv");
  if (glfunc.glGetIntegerv_fp == 0) {Sys_Error("glGetIntegerv not found in GL library");}

  glfunc.glGetString_fp = (glGetString_f) SDL_GL_GetProcAddress("glGetString");
  if (glfunc.glGetString_fp == 0) {Sys_Error("glGetString not found in GL library");}
  glfunc.glGetFloatv_fp = (glGetFloatv_f) SDL_GL_GetProcAddress("glGetFloatv");
  if (glfunc.glGetFloatv_fp == 0) {Sys_Error("glGetFloatv not found in GL library");}
}

/*
=================
VID_SetGamma

=================
*/

void VID_SetGamma(float value)
{
//#ifdef PLATFORM_UNIX
  SDL_SetGamma(value,value,value);
//#endif
}

void VID_ApplyGamma (void)
{
  if ((v_gamma.value != 0)&&(v_gamma.value > (1/GAMMA_MAX)))
    VID_SetGamma(1/v_gamma.value);

  else VID_SetGamma(GAMMA_MAX);

//  Con_Printf ("Gamma changed to %f\n", v_gamma.value);
}

void VID_SetGamma_f (void)
{
  float value = 1;

  value = atof (Cmd_Argv(1));

  if (value != 0) {
    if (value > (1/GAMMA_MAX))
      v_gamma.value = value;

    else v_gamma.value =  1/GAMMA_MAX;
  }

/* if value==0 , just apply current settings.
   this is usefull at startup */

  VID_ApplyGamma();

//  Con_Printf ("Gamma changed to %f\n", v_gamma.value);
}


/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
//	extern cvar_t gl_clear;

	*x = *y = 0;
#if 0
	*width = WindowRect.right - WindowRect.left;
	*height = WindowRect.bottom - WindowRect.top;
#else
	*width = WRWidth;
	*height = WRHeight;
#endif

//    if (!wglMakeCurrent( maindc, baseRC ))
//		Sys_Error ("wglMakeCurrent failed");

//	glViewport (*x, *y, *width, *height);
}


void GL_EndRendering (void)
{
#if 0
	if (!scr_skipupdate)
		SwapBuffers(maindc);
#else
	if (!scr_skipupdate)
		SDL_GL_SwapBuffers();
#endif

// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED)
	{
		if ((int)_windowed_mouse.value != windowed_mouse)
		{
			if (_windowed_mouse.value)
			{
				IN_ActivateMouse ();
				IN_HideMouse ();
			}
			else
			{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}

			windowed_mouse = (int)_windowed_mouse.value;
		}
	}
}


int ColorIndex[16] =
{
	0, 31, 47, 63, 79, 95, 111, 127, 143, 159, 175, 191, 199, 207, 223, 231
};

unsigned ColorPercent[16] =
{
	25, 51, 76, 102, 114, 127, 140, 153, 165, 178, 191, 204, 216, 229, 237, 247
};

#ifdef DO_BUILD
static int ConvertTrueColorToPal( unsigned char *true_color, unsigned char *palette )
{
	int i;
	long min_dist;
	int min_index;
	long r, g, b;

	min_dist = 256 * 256 + 256 * 256 + 256 * 256;
	min_index = -1;
	r = ( long )true_color[0];
	g = ( long )true_color[1];
	b = ( long )true_color[2];

	for( i = 0; i < 256; i++ )
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
		if( dist < min_dist )
		{
			min_dist = dist;
			min_index = i;
		}
	}
	return min_index;
}
#endif

void	VID_CreateInversePalette( unsigned char *palette )
{
#ifdef DO_BUILD
	FILE *FH;

	long r, g, b;
	long index = 0;
	unsigned char true_color[3];
	static qboolean been_here = false;

	if( been_here )
		return;

	been_here = true;
	
	for( r = 0; r < ( 1 << INVERSE_PAL_R_BITS ); r++ )
	{
		for( g = 0; g < ( 1 << INVERSE_PAL_G_BITS ); g++ )
		{
			for( b = 0; b < ( 1 << INVERSE_PAL_B_BITS ); b++ )
			{
				true_color[0] = ( unsigned char )( r << ( 8 - INVERSE_PAL_R_BITS ) );
				true_color[1] = ( unsigned char )( g << ( 8 - INVERSE_PAL_G_BITS ) );
				true_color[2] = ( unsigned char )( b << ( 8 - INVERSE_PAL_B_BITS ) );
				inverse_pal[index] = ConvertTrueColorToPal( true_color, palette );
				index++;
			}
		}
	}

	FH = fopen("data1\\gfx\\invpal.lmp","wb");
	fwrite(inverse_pal,1,sizeof(inverse_pal),FH);
	fclose(FH);
#else
	COM_LoadStackFile ("gfx/invpal.lmp",inverse_pal,sizeof(inverse_pal));
#endif
}

#if 0
void VID_Download3DfxPalette( void )
{
	unsigned long fxPalette[256];
	int i;

	for( i = 0; i < 256; i++ )
	{
		fxPalette[i] = 0xff000000 | 
			( (  d_8to24table[i] & 0x000000ff ) << 16 ) |
			( (  d_8to24table[i] & 0x0000ff00 ) ) |
			( (  d_8to24table[i] & 0x00ff0000 ) >> 16 );

//		fxPalette[i] = i<<16; // 0x00rrggbb
	}
	if( fxSetPaletteExtension )
		fxSetPaletteExtension( fxPalette );
}
#endif

void VID_SetPalette (unsigned char *palette)
{
	byte	*pal;
	int		r,g,b,v;
	int		i,c,p;
	unsigned	*table;

	if( is_3dfx || is_PowerVR)
		VID_CreateInversePalette( palette );

//
// 8 8 8 encoding
//
	pal = palette;
	table = d_8to24table;
	
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
	}

	d_8to24table[255] &= 0xffffff;	// 255 is transparent

#if 0
	if( is_3dfx || is_PowerVR )
		VID_Download3DfxPalette();
#endif

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
}

BOOL	gammaworks;
void	VID_ShiftPalette (unsigned char *palette)
{
	extern	byte ramps[3][256];
	
//	VID_SetPalette (palette);

//	gammaworks = SetDeviceGammaRamp (maindc, ramps);
}

void VID_SetDefaultMode (void)
{
	IN_DeactivateMouse ();
}


void	VID_Shutdown (void)
{
	SDL_Quit();
#if 0
   	HGLRC hRC;
   	HDC	  hDC;

	if (vid_initialized)
	{
    	hRC = wglGetCurrentContext();
    	hDC = wglGetCurrentDC();

    	wglMakeCurrent(NULL, NULL);

    	if (hRC)
    	    wglDeleteContext(hRC);

		if (hDC && dibwindow)
			ReleaseDC(dibwindow, hDC);

		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);

		if (maindc && dibwindow)
			ReleaseDC (dibwindow, maindc);

		AppActivate(false, false);
	}
#endif 
}


//==========================================================================

#if 0
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
#endif



byte        scantokey[128] = 
					{ 
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

byte        shiftscantokey[128] = 
					{ 
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
	K_F6, K_F7, K_F8, K_F9, K_F10,0  ,    0  , K_HOME, 
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
	static char	pinfo[40];
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
	int		t, modenum;
	
	modenum = atoi (Cmd_Argv(1));

	t = leavecurrentmode;
	leavecurrentmode = 0;

	Con_Printf ("%s\n", VID_GetExtModeDescription (modenum));

	leavecurrentmode = t;
}

void VID_Switch_f (void)
{
#warning STUB!!!
#if 0
	int newmode;

	newmode = atoi (Cmd_Argv(1));
	if( !fxDisplayModeExtension )
		return;

//	fxDisplayModeExtension( newmode );
	fxDisplayModeExtension( 0 );
	Sleep( 2 );
	fxDisplayModeExtension( 1 );
#endif
}


/*
=================
VID_DescribeModes_f
=================
*/
void VID_DescribeModes_f (void)
{
	int			i, lnummodes, t;
	char		*pinfo;
	vmode_t		*pv;

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

// "if 0" removed

/*
===================
VID_Init
===================
*/
void	VID_Init (unsigned char *palette)
{
	int	basenummodes, width, height, bpp, findbpp;
	char	gldir[MAX_OSPATH];
#if 0
	HDC	hdc;
	DEVMODE	devmode;
#endif

	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&vid_wait);
	Cvar_RegisterVariable (&vid_nopageflip);
	Cvar_RegisterVariable (&_vid_wait_override);
	Cvar_RegisterVariable (&_vid_default_mode);
	Cvar_RegisterVariable (&_vid_default_mode_win);
	Cvar_RegisterVariable (&vid_config_x);
	Cvar_RegisterVariable (&vid_config_y);
	Cvar_RegisterVariable (&vid_stretch_by_2);
	Cvar_RegisterVariable (&_windowed_mouse);
	Cvar_RegisterVariable (&gl_ztrick);
	Cvar_RegisterVariable (&gl_purge_maptex);

	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f);
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);
	Cmd_AddCommand ("vid_switch", VID_Switch_f);
	Cmd_AddCommand ("vid_setgamma", VID_SetGamma_f);

	gl_library=NULL;
	if (COM_CheckParm("--gllibrary")) {
	  gl_library = com_argv[COM_CheckParm("--gllibrary")+1];
	  Con_Printf("Using GL library: %s\n",gl_library);
	}
	else if (COM_CheckParm("-g")) {
	  gl_library = com_argv[COM_CheckParm("-g")+1];
	  Con_Printf("Using GL library: %s\n",gl_library);
	}

	modelist[0].type = MS_WINDOWED;
	modelist[0].width = 640;
	modelist[0].height = 480;
	strcpy (modelist[0].modedesc, "640x480");
	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].dib = 1;
	modelist[0].fullscreen = 0;
	modelist[0].halfscreen = 0;
	modelist[0].bpp = 16;

	// mode[1] has been hacked to be like the (missing) mode[3] S.A.

	modelist[1].type = MS_FULLDIB;
	modelist[1].width = 640;
	modelist[1].height = 480;
	strcpy (modelist[1].modedesc, "640x480");
	modelist[1].modenum = MODE_FULLSCREEN_DEFAULT;
	modelist[1].dib = 1;
	modelist[1].fullscreen = 1;
	modelist[1].halfscreen = 0;
	modelist[1].bpp = 16;

	// modelist[2-5] have been removed

        vid_default = MODE_WINDOWED;

        windowed_default = vid_default;

        nummodes = 6;

        basenummodes = nummodes;

        vid.maxwarpwidth = WARP_WIDTH;
        vid.maxwarpheight = WARP_HEIGHT;
        vid.colormap = host_colormap;
        vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	/*********************************
 	 * command line processing (S.A) *
 	 *********************************/

	// window mode is default

	grab = 1;

	if (COM_CheckParm("-fullscreen") || COM_CheckParm("-f") ||
		COM_CheckParm("-fs") || COM_CheckParm("--fullscreen"))
	{
		windowed = false;
		vid_default = MODE_FULLSCREEN_DEFAULT;
	}

	if (COM_CheckParm("-window") || COM_CheckParm("-w") ||
		COM_CheckParm("--windowed"))
	{
		windowed = true;
		vid_default = MODE_WINDOWED;
	}

	if (COM_CheckParm("-width"))
	{
		width = atoi(com_argv[COM_CheckParm("-width")+1]);
	}
	else
	{
		if (vid_default == MODE_WINDOWED)
			width=640;
		else
			width=800;
	}

	if (COM_CheckParm("-height"))
	{
		height = atoi(com_argv[COM_CheckParm("-height")+1]);
	}
	else
	{
		switch (width) {
			case 1600: height=1200; break;
			case 1280: height=1024; break;
			case 1024: height=768;  break;
			case 800:  height=600;  break;
			case 640:  height=480;  break;
			case 512:  height=384;  break;
			case 400:  height=300;  break;
			case 320:  height=240;  break;

			default:   height=480;
		}
	}
	if (COM_CheckParm("-bpp"))
	{
		bpp = atoi(com_argv[COM_CheckParm("-bpp")+1]);
		findbpp = 0;
	}
	else
	{
		// this should be native bpp ??? S.A.
		bpp = 16;
	}
	modelist[vid_default].width = width;
	modelist[vid_default].height = height;
	sprintf (modelist[vid_default].modedesc,"%dx%dx%d",width,height,bpp);

	vid_initialized = true;

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));


	VID_SetMode (vid_default, palette);


	GL_Init ();

	// use com_userdir instead of com_gamedir to stock cached mesh files

	/*sprintf (gldir, "%s/glhexen", com_gamedir);
	Sys_mkdir (gldir);
	sprintf (gldir, "%s/glhexen/boss", com_gamedir);
	Sys_mkdir (gldir);
	sprintf (gldir, "%s/glhexen/puzzle", com_gamedir);
	Sys_mkdir (gldir);*/

	sprintf (gldir, "%s/glhexen", com_userdir);
	Sys_mkdir (gldir);
	sprintf (gldir, "%s/glhexen/boss", com_userdir);
	Sys_mkdir (gldir);
	sprintf (gldir, "%s/glhexen/puzzle", com_userdir);
	Sys_mkdir (gldir);

	vid_realmode = vid_modenum;

	VID_SetPalette (palette);

	// vid_menudrawfn = NULL;
	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	strcpy (badmode.modedesc, "Bad mode");
}


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
	char		*ptr;
	int		lnummodes, i, k, column, row;
	vmode_t		*pv;

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

	// if (vid_wmodes > 0) S.A
	if (0)
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

	M_Print (0*8, 4 + MODE_AREA_HEIGHT * 8 + 8*2,
			 "Select video modes from the command line");
	M_Print (13*8, 4 + MODE_AREA_HEIGHT * 8 + 8*4,
			 "-window");
	M_Print (13*8, 4 + MODE_AREA_HEIGHT * 8 + 8*5,
			 "-fullscreen");
	M_Print (13*8, 4 + MODE_AREA_HEIGHT * 8 + 8*6,
			 "-width  <width>");
	M_Print (13*8, 4 + MODE_AREA_HEIGHT * 8 + 8*7,
			 "-height <height>");
	M_Print (13*8, 4 + MODE_AREA_HEIGHT * 8 + 8*8,
			 "-bpp    <depth>");
}

void ToggleFullScreenSA ()
{
	if (SDL_WM_ToggleFullScreen(screen)==1) {

		Cvar_SetValue ("vid_mode", !vid_mode.value);

	} else {
		puts ("SDL_WM_ToggleFullScreen failed");
	}
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
/*	unused // Toggle fullscreen/window modes S.A.
	case K_SPACE:
		ToggleFullScreenSA ();
		break;
*/
	case K_ESCAPE:
	case K_ENTER:
		S_LocalSound ("raven/menu1.wav");
		M_Menu_Options_f ();
		break;

	default:
		break;
	}
}

void D_ShowLoadingSize(void)
{
	if (!vid_initialized)
		return;

	glfunc.glDrawBuffer_fp  (GL_FRONT);

	SCR_DrawLoading();

	glfunc.glDrawBuffer_fp  (GL_BACK);
}
