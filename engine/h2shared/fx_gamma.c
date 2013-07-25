/*
 * fx_gamma.c
 * $Id$
 *
 * Small library providing gamma control functions for 3Dfx Voodoo1/2
 * cards by abusing the exposed glide symbols when using fxMesa.
 *
 * Author: O. Sezer <sezero@users.sourceforge.net>   License: GPL
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the accompanying COPYING file for more details.
 *
 * Compiling as a shared library:
 * gcc fx_gamma.c -O2 -fPIC -Wall -W -o lib3dfxgamma.so -shared
 *
 * How to use:
 * If you are linking to the opengl library at compile time (-lGL),
 * not much is necessary. If you dlopen() the opengl library, then
 * RTLD_GLOBAL flag is necessary so that the library's symbols would be
 * available to you: SDL_GL_LoadLibrary() is just fine in this regard.
 * In either case, if the gllib is an fxMesa library, then you will have
 * the necessary glide symbols exposed on you. Decide whether you have
 * a Voodoo1/2 and then use the functions here.
 *
 * Issues:
 * glSetDeviceGammaRamp3DFX works nicely with Voodoo2, but it crashes
 * Voodoo1. The ramp functions are added for completeness sake anyway.
 * do3dfxGammaCtrl works just fine for both Voodoo1 and Voodoo2.
 * Besides, the GammaRamp3DFX functions are only available for Glide3:
 * Glide2 users cannot benefit them, but the gamma control option is
 * available for both Glide2 and Glide3. Therefore employing the gamma
 * control option seems more beneficial.
 *
 * Revision history:
 * v0.0.1, 2005-06-04:	Initial version, do3dfxGammaCtrl works fine,
 *			glGetDeviceGammaRamp3DFX & co need more care
 * v0.0.2, 2005-06-13:	tried following the exact win32 versions for
 *			glGetDeviceGammaRamp3DFX/glSetDeviceGammaRamp3DFX
 * v0.0.3, 2005-12-05:	Updated documentation about the RTLD_GLOBAL flag.
 * v0.0.4, 2006-03-16:	Fixed incorrect prototype for the grGet function
 *			(it takes a signed int param*, not unsigned).
 *			Also renamed FX_Get to FX_GetInteger to be more
 *			explicit.
 * v0.0.5, 2013-07-24:	Several cleanups/tidy-ups.
 */

#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#if 0
#include <glide.h>
#else
#define FX_CALL		/*__stdcall*/
#define GR_GAMMA_TABLE_ENTRIES	0x05
typedef signed int	FxI32;
typedef unsigned int	FxU32;
typedef float		FxFloat;
#endif
#include "fx_gamma.h"

/**********************************************************************/

/**	PRIVATE STUFF			**/

/* 3dfx glide2 func for gamma correction */
static void (FX_CALL *grGammaCorrectionValue_fp)(FxFloat) = NULL;
/* 3dfx glide3 func for gamma correction */
static void (FX_CALL *guGammaCorrectionRGB_fp)(FxFloat, FxFloat, FxFloat) = NULL;

/* 3dfx glide3 funcs to make a replacement wglSetDeviceGammaRamp3DFX */
static FxU32 (FX_CALL *grGet_fp)(FxU32, FxU32, FxI32*) = NULL;
static void (FX_CALL *grLoadGammaTable_fp)(FxU32, FxU32*, FxU32*, FxU32*) = NULL;

/**********************************************************************/

/*
 * Init_3dfxGammaCtrl
 * Sends 0 for failure, 2 for glide2 or 3 for glide3 api.
 */
int Init_3dfxGammaCtrl (void)
{
	void	*symslist;
	int	ret;

	if (grGammaCorrectionValue_fp != NULL)
		return 2;	/* already have glide2x proc address */
	if (guGammaCorrectionRGB_fp != NULL)
		return 3;	/* already have glide3x proc address */

	symslist = (void *) dlopen(NULL, RTLD_LAZY);
	if (symslist != NULL)
	{
		if ((grGammaCorrectionValue_fp = (void (*) (FxFloat)) dlsym(symslist, "grGammaCorrectionValue")) != NULL)
			ret = 2;/* glide2x */
		else if ((guGammaCorrectionRGB_fp = (void (*) (FxFloat, FxFloat, FxFloat)) dlsym(symslist, "guGammaCorrectionRGB")) != NULL)
			ret = 3;/* glide3x */
		else	ret = 0;

		dlclose(symslist);
		return ret;
	}

	return 0;	/* shouldn't reach here */
}

void Shutdown_3dfxGamma (void)
{
	grGammaCorrectionValue_fp = NULL;
	guGammaCorrectionRGB_fp = NULL;
	grGet_fp = NULL;
	grLoadGammaTable_fp = NULL;
}

/*
 * do3dfxGammaCtrl
 */
int do3dfxGammaCtrl (float value)
{
	if (grGammaCorrectionValue_fp)	/* glide2x */
	{
		grGammaCorrectionValue_fp (value);
		return 1;
	}
	if (guGammaCorrectionRGB_fp)	/* glide3x */
	{
		guGammaCorrectionRGB_fp (value, value, value);
		return 1;
	}
	return 0;
}

/**********************************************************************/

static int Check_3DfxGammaRamp (void)
{
	void	*symslist;

	if (grLoadGammaTable_fp != NULL && grGet_fp != NULL)
		return 1;

	symslist = (void *) dlopen(NULL, RTLD_LAZY);
	if (symslist != NULL)
	{
		grGet_fp = (FxU32 (*) (FxU32, FxU32, FxI32 *)) dlsym(symslist, "grGet");
		grLoadGammaTable_fp = (void (*) (FxU32, FxU32 *, FxU32 *, FxU32 *)) dlsym(symslist, "grLoadGammaTable");
		dlclose(symslist);
		if (grLoadGammaTable_fp != NULL && grGet_fp != NULL)
			return 1;
	}

	return 0;
}

/*
 * glSetDeviceGammaRamp3DFX is adapted from Mesa-6.x
 *
 * glSetDeviceGammaRamp3DFX crashes Voodoo1, at least
 * currently, so it is not recommended yet.
 */
int glSetDeviceGammaRamp3DFX (void *arrays)
{
	FxI32		tableSize = 0;
	FxI32		i, inc, idx;
	unsigned short	*red, *green, *blue;
	FxU32		gammaTableR[256], gammaTableG[256], gammaTableB[256];

	if (grLoadGammaTable_fp == NULL || grGet_fp == NULL)
		return 0;

	grGet_fp (GR_GAMMA_TABLE_ENTRIES, 4, &tableSize);
	if (tableSize <= 0)
		return 0;

	inc = 256 / tableSize;

	red = (unsigned short *)arrays;
	green = (unsigned short *)arrays + 256;
	blue = (unsigned short *)arrays + 512;

	for (i = 0, idx = 0; i < tableSize; i++, idx += inc)
	{
		gammaTableR[i] = red[idx] >> 8;
		gammaTableG[i] = green[idx] >> 8;
		gammaTableB[i] = blue[idx] >> 8;
	}

	grLoadGammaTable_fp(tableSize, gammaTableR, gammaTableG, gammaTableB);

	return 1;
}

/*
 * glGetDeviceGammaRamp3DFX
 * Sends a 1.0 gamma table. Also to be used for querying the lib.
 */
int glGetDeviceGammaRamp3DFX (void *arrays)
{
	int		i;
	unsigned short	gammaTable[3][256];

	if (grLoadGammaTable_fp == NULL || grGet_fp == NULL)
	{
		if (Check_3DfxGammaRamp() == 0)
			return 0;
	}

	for (i = 0; i < 256; i++)
	{
		gammaTable[0][i] = i << 8;
		gammaTable[1][i] = i << 8;
		gammaTable[2][i] = i << 8;
	}

	memcpy (arrays, gammaTable, 3 * 256 * sizeof(unsigned short));

	return 1;
}

