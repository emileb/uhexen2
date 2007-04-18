/*
	r_local.h
	private refresh defs

	$Header: /home/ozzie/Download/0000/uhexen2/hexen2/r_local.h,v 1.13 2007-04-18 08:47:49 sezero Exp $
*/

#ifndef __R_LOCAL_H
#define __R_LOCAL_H

#ifndef GLQUAKE

#include "r_shared.h"


#define ALIAS_BASE_SIZE_RATIO	(1.0 / 11.0)
				// normalizing factor so player model works out
				// to about 1 pixel per triangle

#define BMODEL_FULLY_CLIPPED	0x10
				// value returned by R_BmodelCheckBBox ()
				// if bbox is trivially rejected

//===========================================================================
// viewmodel lighting

typedef struct {
	int			ambientlight;
	int			shadelight;
	float		*plightvec;
} alight_t;

//===========================================================================
// clipped bmodel edges

typedef struct bedge_s
{
	mvertex_t		*v[2];
	struct bedge_s	*pnext;
} bedge_t;

typedef struct {
	float	fv[3];		// viewspace x, y
} auxvert_t;

//===========================================================================

extern cvar_t	r_draworder;
extern cvar_t	r_speeds;
extern cvar_t	r_timegraph;
extern cvar_t	r_graphheight;
extern cvar_t	r_clearcolor;
extern cvar_t	r_waterwarp;
extern cvar_t	r_fullbright;
extern cvar_t	r_drawentities;
extern cvar_t	r_aliasstats;
extern cvar_t	r_dspeeds;
extern cvar_t	r_drawflat;
extern cvar_t	r_ambient;
extern cvar_t	r_reportsurfout;
extern cvar_t	r_maxsurfs;
extern cvar_t	r_numsurfs;
extern cvar_t	r_reportedgeout;
extern cvar_t	r_maxedges;
extern cvar_t	r_numedges;
extern cvar_t	r_aliasmip;
extern cvar_t	r_transwater;
extern cvar_t	r_texture_external;

#define XCENTERING	(1.0 / 2.0)
#define YCENTERING	(1.0 / 2.0)

#define CLIP_EPSILON		0.001

#define BACKFACE_EPSILON	0.01

//===========================================================================

#define	DIST_NOT_SET	98765

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct clipplane_s
{
	vec3_t		normal;
	float		dist;
	struct clipplane_s	*next;
	byte		leftedge;
	byte		rightedge;
	byte		reserved[2];
} clipplane_t;

extern	clipplane_t	view_clipplanes[4];

//=============================================================================

void R_RenderWorld (void);

//=============================================================================

extern	mplane_t	screenedge[4];

extern	vec3_t	r_origin;

extern	vec3_t	r_entorigin;

extern	float	screenAspect;
extern	float	verticalFieldOfView;
extern	float	xOrigin, yOrigin;

extern	int	r_visframecount;

//=============================================================================

extern	int	vstartscan;


void R_ClearPolyList (void);
void R_DrawPolyList (void);

//
// current entity info
//
extern	qboolean	insubmodel;


void R_DrawSprite (void);
void R_RenderFace (msurface_t *fa, int clipflags);
void R_RenderPoly (msurface_t *fa, int clipflags);
void R_RenderBmodelFace (bedge_t *pedges, msurface_t *psurf);
void R_TransformPlane (mplane_t *p, float *normal, float *dist);
void R_TransformFrustum (void);
void R_SetSkyFrame (void);
texture_t *R_TextureAnimation (texture_t *base);

void R_GenSkyTile (void *pdest);
void R_GenSkyTile16 (void *pdest);
void R_Surf8Patch (void);
void R_Surf16Patch (void);
void R_DrawSubmodelPolygons (model_t *pmodel, int clipflags);
void R_DrawSolidClippedSubmodelPolygons (model_t *pmodel);

void R_AddPolygonEdges (emitpoint_t *pverts, int numverts, int miplevel);
surf_t *R_GetSurf (void);
void R_AliasDrawModel (alight_t *plighting);
void R_BeginEdgeFrame (void);
void R_ScanEdges (qboolean Translucent);
void D_DrawSurfaces (qboolean Translucent);

extern void R_Surf8Start (void);
extern void R_Surf8End (void);
extern void R_Surf16Start (void);
extern void R_Surf16End (void);
extern void R_EdgeCodeStart (void);
extern void R_EdgeCodeEnd (void);
extern void R_EdgeCodeStartT (void);
extern void R_EdgeCodeEndT (void);

extern void R_RotateBmodel (void);

extern	int	c_faceclip;
extern	int	r_polycount;
extern	int	r_wholepolycount;

extern	int	*pfrustum_indexes[4];

// !!! if this is changed, it must be changed in asm_draw.h too !!!
#define	NEAR_CLIP	0.01

extern	int	ubasestep, errorterm, erroradjustup, erroradjustdown;
extern	int	vstartscan;

extern	fixed16_t	sadjust, tadjust;
extern	fixed16_t	bbextents, bbextentt;

#define MAXBVERTINDEXES	1000	// new clipped vertices when clipping bmodels
								//  to the world BSP
extern	mvertex_t	*r_ptverts, *r_ptvertsmax;

extern	vec3_t	sbaseaxis[3], tbaseaxis[3];
extern	float	entity_rotation[3][3];

extern	int	r_currentkey;
extern	int	r_currentbkey;

typedef struct btofpoly_s
{
	int			clipflags;
	msurface_t	*psurf;
} btofpoly_t;

#define MAX_BTOFPOLYS	5000	// FIXME: tune this

extern	int	numbtofpolys;
extern	btofpoly_t	*pbtofpolys;

void R_ZDrawSubmodelPolys (model_t *clmodel);

//=========================================================
// Alias models
//=========================================================

#define MAXALIASVERTS		2000	// TODO: tune this
#define ALIAS_Z_CLIP_PLANE	5

extern	int		numverts;
extern	mtriangle_t	*ptriangles;
extern	int		numtriangles;
extern	aliashdr_t	*paliashdr;
extern	newmdl_t	*pmdl;
extern	float		leftclip, topclip, rightclip, bottomclip;
extern	int		r_acliptype;
extern	finalvert_t	*pfinalverts;
extern	auxvert_t	*pauxverts;

qboolean R_AliasCheckBBox (void);

//=========================================================
// turbulence stuff

#define	AMP		8*0x10000
#define	AMP2		3
#define	SPEED		20

//=========================================================
// particle stuff

void R_DrawParticles (void);
void R_InitParticles (void);
void R_ClearParticles (void);

void R_ReadPointFile_f (void);
void R_SurfacePatch (void);

extern	int	r_amodels_drawn;
extern	edge_t	*auxedges;
extern	int	r_numallocatededges;
extern	edge_t	*r_edges, *edge_p, *edge_max;

extern	edge_t	*newedges[MAXHEIGHT];
extern	edge_t	*removeedges[MAXHEIGHT];

extern	int	screenwidth;

// FIXME: make stack vars when debugging done
extern	edge_t	edge_head;
extern	edge_t	edge_tail;
extern	edge_t	edge_aftertail;
extern	int	r_bmodelactive;
extern	vrect_t	*pconupdate;

extern	float	aliasxscale, aliasyscale, aliasxcenter, aliasycenter;
extern	float	r_aliastransition, r_resfudge;

extern	int	r_outofsurfaces;
extern	int	r_outofedges;

extern	mvertex_t	*r_pcurrentvertbase;
extern	int	r_maxvalidedgeoffset;

void R_AliasClipTriangle (mtriangle_t *ptri);

extern	float	r_time1;
extern	float	dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
extern	float	se_time1, se_time2, de_time1, de_time2, dv_time1, dv_time2;
extern	int	r_frustum_indexes[4*6];
extern	int	r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
extern	qboolean	r_surfsonstack;
extern	cshift_t	cshift_water;
extern	qboolean	r_dowarpold, r_viewchanged;

extern	float	r_lasttime1;

extern	mleaf_t	*r_viewleaf, *r_oldviewleaf;

extern	vec3_t	r_emins, r_emaxs;
extern	mnode_t	*r_pefragtopnode;
extern	int	r_clipflags;
extern	int	r_dlightframecount;
extern	qboolean	r_fov_greater_than_90;

extern	int	TransCount;

void R_StoreEfrags (efrag_t **ppefrag);
void R_TimeRefresh_f (void);
void R_TimeGraph (void);
void R_PrintAliasStats (void);
void R_PrintTimes (void);
void R_PrintDSpeeds (void);
void R_AnimateLight (void);
int R_LightPoint (vec3_t p);
int *R_LightPointColour (vec3_t p);
void R_SetupFrame (void);
void R_cshift_f (void);
void R_SplitEntityOnNode2 (mnode_t *node);
void R_MarkLights (dlight_t *light, int bit, mnode_t *node);

#endif	/*  !GLQUAKE	*/

#endif	/* __R_LOCAL_H	*/

