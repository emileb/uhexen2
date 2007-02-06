/*
	sv_model.h
	header for model loading and caching

	This version of model.c and model.h are based on a quake dedicated
	server application, lhnqserver, by LordHavoc.

	$Id: model.h,v 1.3 2007-02-06 12:23:48 sezero Exp $
*/

#ifndef __HX2_MODEL_H
#define __HX2_MODEL_H

#include "genmodel.h"
#include "spritegn.h"

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


//
// in memory representation
//
// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	vec3_t		position;
} mvertex_t;

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2


// plane_t structure
typedef struct mplane_s
{
	vec3_t	normal;
	float	dist;
	byte	type;			// for texture axis selection and fast side tests
	byte	signbits;		// signx + signy<<1 + signz<<1
	byte	pad[2];
} mplane_t;

#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWSPRITE		8
#define SURF_DRAWTURB		0x10
#define SURF_DRAWTILED		0x20
#define SURF_DRAWBACKGROUND	0x40
#define SURF_TRANSLUCENT	0x80
#define SURF_DRAWBLACK		0x200

typedef struct
{
	unsigned short	v[2];
} medge_t;

typedef struct
{
	float		vecs[2][4];
	int		flags;
} mtexinfo_t;

typedef struct msurface_s
{
	mplane_t	*plane;
	int		flags;

	short		texturemins[2];
	short		extents[2];

	mtexinfo_t	*texinfo;

// lighting info
	byte		styles[MAXLIGHTMAPS];
	byte		*samples;		// [numstyles*surfsize]
} msurface_t;

typedef struct mnode_s
{
// common with leaf
	int		contents;		// 0, to differentiate from leafs

	struct mnode_s	*parent;

// node specific
	mplane_t	*plane;
	struct mnode_s	*children[2];	

	unsigned short	firstsurface;
	unsigned short	numsurfaces;
} mnode_t;



typedef struct mleaf_s
{
// common with node
	int		contents;		// wil be a negative contents number

	struct mnode_s	*parent;

// leaf specific
	byte		*compressed_vis;
} mleaf_t;

// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct
{
	dclipnode_t	*clipnodes;
	mplane_t	*planes;
	int		firstclipnode;
	int		lastclipnode;
	vec3_t		clip_mins;
	vec3_t		clip_maxs;
} hull_t;


//===================================================================

//
// entity effects
//
#define	EF_BRIGHTFIELD			0x00000001
#define	EF_MUZZLEFLASH			0x00000002
#define	EF_BRIGHTLIGHT			0x00000004
#define	EF_DIMLIGHT			0x00000008
#define	EF_DARKLIGHT			0x00000010
#define	EF_DARKFIELD			0x00000020
#define	EF_LIGHT			0x00000040
#define	EF_NODRAW			0x00000080

//===================================================================

//
// Whole model
//

typedef enum {mod_brush, mod_sprite, mod_alias} modtype_t;

typedef struct model_s
{
	char		name[MAX_QPATH];
	qboolean	needload;		// bmodels and sprites don't cache normally

	modtype_t	type;
	int		flags;
	int		numframes;

//
// volume occupied by the model graphics
//		
	vec3_t		mins, maxs;
	float		radius;

// solid volume for clipping
	qboolean	clipbox;
	vec3_t		clipmins, clipmaxs;

//
// brush model
//
	int		firstmodelsurface, nummodelsurfaces;

	int		numsubmodels;
	dmodel_t	*submodels;

	int		numplanes;
	mplane_t	*planes;

	int		numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int		numvertexes;
	mvertex_t	*vertexes;

	int		numnodes;
	mnode_t		*nodes;

	int		numtexinfo;
	mtexinfo_t	*texinfo;

	int		numsurfaces;
	msurface_t	*surfaces;

	int		numclipnodes;
	dclipnode_t	*clipnodes;

	hull_t		hulls[MAX_MAP_HULLS];

	byte		*visdata;
	byte		*lightdata;
	char		*entities;
} model_t;

//============================================================================

void	Mod_Init (void);
void	Mod_ClearAll (void);
model_t *Mod_ForName (const char *name, qboolean crash);
model_t *Mod_FindName (const char *name);

mleaf_t *Mod_PointInLeaf (float *p, model_t *model);
byte	*Mod_LeafPVS (mleaf_t *leaf, model_t *model);

#endif	/* __HX2_MODEL_H */

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.2  2006/09/24 17:28:42  sezero
 * protected all headers against multiple inclusion
 *
 * Revision 1.1  2006/06/25 12:57:06  sezero
 * added a hexen2 dedicated server which seems to work much better than
 * the client/server application running in dedicated mode. model loading
 * implementation taken from LordHavoc's old lhnqserver, as it seems better
 * than the one in hexenworld server.
 *
 */

