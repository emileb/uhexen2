/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

/*
 * tyrlight/tyrlite.h
 * Modifications by Kevin Shanahan, 1999-2000
 */

#ifndef __TYRLITE_H__
#define __TYRLITE_H__

#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "entities.h"
#include "threads.h"

#define JSH2COLOR_VER	"1.2.3"	// version string
#ifdef _WIN32
#define PLATFORM_VER	"win32"
#else
#define PLATFORM_VER	"unix"
#endif

#define	ON_EPSILON	0.1

#define	MAXLIGHTS	1024

// js features
#ifdef _MAX_PATH
#define	MAX_OSPATH	_MAX_PATH
#else
#define	MAX_OSPATH	256
#endif
#define	MAX_ENTRYNUM	32784
#define	MAX_TEX_NAME	64

extern	float		scaledist;
extern	float		scalecos;
extern	float		rangescale;
extern	int		worldminlight;
extern	vec3_t		minlight_color;
extern	int		sunlight;
extern	vec3_t		sunlight_color;
extern	vec3_t		sunmangle;

//extern int		c_culldistplane, c_proper;
extern	byte		*filebase;
extern	vec3_t		bsp_origin;

extern	qboolean	extrasamples;
extern	qboolean	compress_ents;
extern	qboolean	colored;
extern	qboolean	force;
extern	qboolean	nominlimit;
extern	qboolean	makelit;
extern	qboolean	force;
extern	qboolean	external;
extern	qboolean	nodefault;

//void	TransformSample (vec3_t in, vec3_t out);
//void	RotateSample (vec3_t in, vec3_t out);
//void	LoadNodes (char *file);

byte	*GetFileSpace (int size);

qboolean TestLine (vec3_t start, vec3_t stop);
// TYR - added TestSky
qboolean TestSky  (vec3_t start, vec3_t dirn);
void	TestLightFace (int surfnum, qboolean nolight, vec3_t faceoffset);
void	LightFace (int surfnum, qboolean nolight, vec3_t faceoffset);
void	LightFaceLIT (int surfnum, qboolean nolight, vec3_t faceoffset);
void	CheckTex (void);
void	FindTexlightColour (int *surf_r, int *surf_g, int *surf_b, const char *texname);

void	LightLeaf (dleaf_t *leaf);
void	MakeTnodes (dmodel_t *bm);

// js features
void	InitDefFile (const char *fname);
void	CloseDefFile (void);
void	DecisionTime (const char *msg);

#endif	/* __TYRLITE_H__ */

