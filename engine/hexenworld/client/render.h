/*
	refresh.h
	public interface to refresh functions

	$Header: /cvsroot/uhexen2/engine/hexenworld/client/render.h,v 1.8 2008-04-22 13:06:10 sezero Exp $
*/

#ifndef __HX2_RENDER_H
#define __HX2_RENDER_H

#define	TOP_RANGE	16			// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

typedef struct efrag_s
{
	struct mleaf_s		*leaf;
	struct efrag_s		*leafnext;
	struct entity_s		*entity;
	struct efrag_s		*entnext;
} efrag_t;


typedef struct entity_s
{
	int			keynum;		// for matching entities in different frames
	vec3_t			origin;
	vec3_t			angles;
	vec3_t			angleAdd;	// For clientside rotation stuff
	struct qmodel_s		*model;		// NULL = no model
	int			frame;
	byte			*colormap, *sourcecolormap;
	byte			colorshade;
	int			skinnum;	// for Alias models
	int			scale;		// for Alias models
	int			drawflags;	// for Alias models
	int			abslight;	// for Alias models

	struct player_info_s	*scoreboard;	// identify player

	float			syncbase;

	struct efrag_s		*efrag;		// linked list of efrags (FIXME)
	int			visframe;	// last frame this entity was
						// found in an active leaf
						// only used for static objects

	int			dlightframe;	// dynamic lighting
	int			dlightbits;

// FIXME: could turn these into a union
	int			trivial_accept;
	struct mnode_s		*topnode;	// for bmodels, first world node
						// that splits bmodel, or NULL if
						// not split
} entity_t;


// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	vrect_t		vrect;		// subwindow in video for refresh
					// FIXME: not need vrect next field here?

	vrect_t		aliasvrect;	// scaled Alias version

	int		vrectright, vrectbottom;
					// right & bottom screen coords
	int		aliasvrectright, aliasvrectbottom;
					// scaled Alias versions

	float		vrectrightedge;	// rightmost right edge we care about,
					// for use in edge list

	float		fvrectx, fvrecty;
					// for floating-point compares

	float		fvrectx_adj, fvrecty_adj;
					// left and top edges, for clamping

	int		vrect_x_adj_shift20;
					// (vrect.x + 0.5 - epsilon) << 20

	int		vrectright_adj_shift20;
					// (vrectright + 0.5 - epsilon) << 20

	float		fvrectright_adj, fvrectbottom_adj;
					// right and bottom edges, for clamping

	float		fvrectright;	// rightmost edge, for Alias clamping
	float		fvrectbottom;	// bottommost edge, for Alias clamping
	float		horizontalFieldOfView;
					// at Z = 1.0, this many X is visible
					// 2.0 = 90 degrees

	float		xOrigin;	// should probably always be 0.5
	float		yOrigin;	// between be around 0.3 to 0.5

	vec3_t		vieworg;
	vec3_t		viewangles;

	int		ambientlight;
} refdef_t;


//
// refresh
//
extern	refdef_t	r_refdef;
extern	vec3_t		r_origin, vpn, vright, vup;

extern	struct texture_s	*r_notexture_mip;

extern	entity_t	r_worldentity;

void R_Init (void);
void R_InitTextures (void);
void R_InitEfrags (void);
void R_RenderView (void);	// must set r_refdef first

void R_ViewChanged (vrect_t *pvrect, int lineadj, float aspect);
				// called whenever r_refdef or vid change

void R_NewMap (void);

void R_InitSky (struct texture_s *mt);	// called at level load

void R_DrawName (vec3_t origin, const char *name, int red);

void R_PushDlights (void);

void R_AddEfrags (entity_t *ent);
void R_RemoveEfrags (entity_t *ent);


//
// surface cache related
//
extern	qboolean	r_cache_thrash;
				// set if thrashing the surface cache

int  D_SurfaceCacheForRes (int width, int height);
void D_FlushCaches (void);
void D_DeleteSurfaceCache (void);
void D_InitCaches (void *buffer, int size);
void R_SetVrect (vrect_t *pvrect, vrect_t *pvrectin, int lineadj);

#endif	/* __HX2_RENDER_H */

