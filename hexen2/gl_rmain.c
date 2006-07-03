/*
	gl_main.c

	$Id: gl_rmain.c,v 1.47 2006-07-03 14:05:36 sezero Exp $
*/


#include "quakedef.h"

entity_t	r_worldentity;
entity_t	*currententity;
vec3_t		modelorg, r_entorigin;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

qboolean	r_cache_thrash;			// compatability
qboolean	envmap;				// true during envmap command capture 

GLuint			currenttexture = GL_UNUSED_TEXTURE;	// to avoid unnecessary texture sets

GLuint			particletexture;	// little dot for particles
GLuint			playertextures[16];	// up to 16 color translated skins
GLuint			gl_extra_textures[MAX_EXTRA_TEXTURES];   // generic textures for models

int			mirrortexturenum;	// quake texturenum, not gltexturenum
qboolean	mirror;
mplane_t	*mirror_plane;

static float	model_constant_alpha;

static float	r_time1;
static float	r_lasttime1 = 0;

extern model_t *player_models[MAX_PLAYER_CLASS];

//
// view origin
//
vec3_t		vup, vpn, vright, r_origin;

float		r_world_matrix[16];
static float	r_base_world_matrix[16];	// for R_Mirror()

//
// screen size info
//
refdef_t	r_refdef;
mleaf_t		*r_viewleaf, *r_oldviewleaf;

extern	int	glwidth, glheight;

texture_t	*r_notexture_mip;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

int		gl_coloredstatic;	// used to store what type of static light
					// we loaded in Mod_LoadLighting()

static	qboolean AlwaysDrawModel;

extern	qboolean gl_dogamma;
extern	cvar_t	v_gamma;

cvar_t	r_norefresh = {"r_norefresh", "0", CVAR_NONE};
cvar_t	r_drawentities = {"r_drawentities", "1", CVAR_NONE};
cvar_t	r_drawviewmodel = {"r_drawviewmodel", "1", CVAR_NONE};
cvar_t	r_speeds = {"r_speeds", "0", CVAR_NONE};
cvar_t	r_fullbright = {"r_fullbright", "0", CVAR_NONE};
cvar_t	r_lightmap = {"r_lightmap", "0", CVAR_NONE};
cvar_t	r_shadows = {"r_shadows", "0", CVAR_ARCHIVE};
cvar_t	r_mirroralpha = {"r_mirroralpha", "1", CVAR_NONE};
cvar_t	r_wateralpha = {"r_wateralpha", "0.33", CVAR_ARCHIVE};
cvar_t	r_skyalpha = {"r_skyalpha", "0.67", CVAR_ARCHIVE};
cvar_t	r_dynamic = {"r_dynamic", "1", CVAR_NONE};
cvar_t	r_novis = {"r_novis", "0", CVAR_NONE};
cvar_t	r_wholeframe = {"r_wholeframe", "1", CVAR_ARCHIVE};

cvar_t	gl_clear = {"gl_clear", "0", CVAR_NONE};
cvar_t	gl_cull = {"gl_cull", "1", CVAR_NONE};
cvar_t	gl_ztrick = {"gl_ztrick", "0", CVAR_ARCHIVE};
cvar_t	gl_multitexture = {"gl_multitexture", "0",CVAR_ARCHIVE};
cvar_t	gl_smoothmodels = {"gl_smoothmodels", "1", CVAR_NONE};
cvar_t	gl_affinemodels = {"gl_affinemodels", "0", CVAR_NONE};
cvar_t	gl_polyblend = {"gl_polyblend", "1", CVAR_NONE};
cvar_t	gl_flashblend = {"gl_flashblend", "0", CVAR_NONE};
cvar_t	gl_playermip = {"gl_playermip", "0", CVAR_NONE};
cvar_t	gl_nocolors = {"gl_nocolors", "0", CVAR_NONE};
cvar_t	gl_keeptjunctions = {"gl_keeptjunctions", "1", CVAR_ARCHIVE};
cvar_t	gl_reporttjunctions = {"gl_reporttjunctions", "0", CVAR_NONE};
cvar_t	gl_waterripple = {"gl_waterripple", "2", CVAR_ARCHIVE};
cvar_t	gl_waterwarp = {"gl_waterwarp", "0", CVAR_ARCHIVE};
cvar_t	gl_stencilshadow = {"gl_stencilshadow", "0", CVAR_ARCHIVE};
cvar_t	gl_glows = {"gl_glows", "1", CVAR_ARCHIVE};
cvar_t	gl_other_glows = {"gl_other_glows", "0", CVAR_ARCHIVE};
cvar_t	gl_missile_glows = {"gl_missile_glows", "1", CVAR_ARCHIVE};

cvar_t	gl_coloredlight = {"gl_coloredlight", "0", CVAR_ARCHIVE};
cvar_t	gl_colored_dynamic_lights = {"gl_colored_dynamic_lights", "0", CVAR_ARCHIVE};
cvar_t	gl_extra_dynamic_lights = {"gl_extra_dynamic_lights", "0", CVAR_ARCHIVE};

//=============================================================================


/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	for (i=0 ; i<4 ; i++)
		if (BoxOnPlaneSide (mins, maxs, &frustum[i]) == 2)
			return true;
	return false;
}


/*
=================
R_RotateForEntity
=================
*/
void R_RotateForEntity (entity_t *e)
{
	glTranslatef_fp (e->origin[0], e->origin[1], e->origin[2]);

	glRotatef_fp (e->angles[1], 0, 0, 1);
	glRotatef_fp (-e->angles[0], 0, 1, 0);
	glRotatef_fp (-e->angles[2], 1, 0, 0);
}

/*
=================
R_RotateForEntity2

Same as R_RotateForEntity(), but checks for
EF_ROTATE and modifies yaw appropriately.
=================
*/
static void R_RotateForEntity2 (entity_t *e)
{
	float	forward, yaw, pitch;
	vec3_t			angles;

	glTranslatef_fp(e->origin[0], e->origin[1], e->origin[2]);

	if (e->model->flags & EF_FACE_VIEW)
	{
		VectorSubtract(e->origin,r_origin,angles);
		VectorSubtract(r_origin,e->origin,angles);
		VectorNormalize(angles);

		if (angles[1] == 0 && angles[0] == 0)
		{
			yaw = 0;
			if (angles[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
			yaw = (int) (atan2(angles[1], angles[0]) * 180 / M_PI);
			if (yaw < 0)
				yaw += 360;

			forward = sqrt (angles[0]*angles[0] + angles[1]*angles[1]);
			pitch = (int) (atan2(angles[2], forward) * 180 / M_PI);
			if (pitch < 0)
				pitch += 360;
		}

		angles[0] = pitch;
		angles[1] = yaw;
		angles[2] = 0;

		glRotatef_fp (-angles[0], 0, 1, 0);
		glRotatef_fp (angles[1], 0, 0, 1);
//		glRotatef_fp (-angles[2], 1, 0, 0);
		glRotatef_fp (-e->angles[2], 1, 0, 0);
	}
	else
	{
		if (e->model->flags & EF_ROTATE)
		{
			glRotatef_fp (anglemod((e->origin[0]+e->origin[1])*0.8 + (108*cl.time)), 0, 0, 1);
		}
		else
		{
			glRotatef_fp (e->angles[1], 0, 0, 1);
		}

		glRotatef_fp (-e->angles[0], 0, 1, 0);
		glRotatef_fp (-e->angles[2], 1, 0, 0);
	}
}

/*
=============================================================

SPRITE MODELS

=============================================================
*/

/*
================
R_GetSpriteFrame
================
*/
static mspriteframe_t *R_GetSpriteFrame (msprite_t *psprite)
{
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int			i, numframes, frame;
	float		*pintervals, fullinterval, targettime, time;

	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_Printf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time + currententity->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}


/*
=================
R_DrawSpriteModel

=================
*/
typedef struct
{
	vec3_t		vup, vright, vpn;	// in worldspace
} spritedesc_t;

static void R_DrawSpriteModel (entity_t *e)
{
	vec3_t		point;
	mspriteframe_t	*frame;
	msprite_t	*psprite;
	vec3_t		tvec;
	float		dot, angle, sr, cr;
	spritedesc_t	r_spritedesc;
	int			i;

	psprite = currententity->model->cache.data;
	frame = R_GetSpriteFrame (psprite);

	if (currententity->drawflags & DRF_TRANSLUCENT)
	{
		glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable_fp (GL_BLEND);
		glColor4f_fp (1,1,1,r_wateralpha.value);
	}
	else if (currententity->model->flags & EF_TRANSPARENT)
	{
		glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable_fp (GL_BLEND);
		glColor3f_fp (1,1,1);
	}
	else
	{
		glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable_fp (GL_BLEND);
		glColor3f_fp (1,1,1);
	}

	if (psprite->type == SPR_FACING_UPRIGHT)
	{
	// generate the sprite's axes, with vup straight up in worldspace, and
	// r_spritedesc.vright perpendicular to modelorg.
	// This will not work if the view direction is very close to straight up or
	// down, because the cross product will be between two nearly parallel
	// vectors and starts to approach an undefined state, so we don't draw if
	// the two vectors are less than 1 degree apart
		tvec[0] = -modelorg[0];
		tvec[1] = -modelorg[1];
		tvec[2] = -modelorg[2];
		VectorNormalize (tvec);
		dot = tvec[2];	// same as DotProduct (tvec, r_spritedesc.vup)
				// because r_spritedesc.vup is 0, 0, 1

		if ((dot > 0.999848) || (dot < -0.999848))	// cos(1 degree) = 0.999848
			return;

		r_spritedesc.vup[0] = 0;
		r_spritedesc.vup[1] = 0;
		r_spritedesc.vup[2] = 1;
		r_spritedesc.vright[0] = tvec[1];
								// CrossProduct (r_spritedesc.vup, -modelorg,
		r_spritedesc.vright[1] = -tvec[0];
								//		 r_spritedesc.vright)
		r_spritedesc.vright[2] = 0;
		VectorNormalize (r_spritedesc.vright);
		r_spritedesc.vpn[0] = -r_spritedesc.vright[1];
		r_spritedesc.vpn[1] = r_spritedesc.vright[0];
		r_spritedesc.vpn[2] = 0;
					// CrossProduct (r_spritedesc.vright, r_spritedesc.vup,
					//		 r_spritedesc.vpn)
	}
	else if (psprite->type == SPR_VP_PARALLEL)
	{
	// generate the sprite's axes, completely parallel to the viewplane. There
	// are no problem situations, because the sprite is always in the same
	// position relative to the viewer
		for (i=0 ; i<3 ; i++)
		{
			r_spritedesc.vup[i] = vup[i];
			r_spritedesc.vright[i] = vright[i];
			r_spritedesc.vpn[i] = vpn[i];
		}
	}
	else if (psprite->type == SPR_VP_PARALLEL_UPRIGHT)
	{
	// generate the sprite's axes, with vup straight up in worldspace, and
	// r_spritedesc.vright parallel to the viewplane.
	// This will not work if the view direction is very close to straight up or
	// down, because the cross product will be between two nearly parallel
	// vectors and starts to approach an undefined state, so we don't draw if
	// the two vectors are less than 1 degree apart
		dot = vpn[2];	// same as DotProduct (vpn, r_spritedesc.vup)
				// because r_spritedesc.vup is 0, 0, 1

		if ((dot > 0.999848) || (dot < -0.999848))	// cos(1 degree) = 0.999848
			return;

		r_spritedesc.vup[0] = 0;
		r_spritedesc.vup[1] = 0;
		r_spritedesc.vup[2] = 1;
		r_spritedesc.vright[0] = vpn[1];
							// CrossProduct (r_spritedesc.vup, vpn,
		r_spritedesc.vright[1] = -vpn[0];	//		 r_spritedesc.vright)
		r_spritedesc.vright[2] = 0;
		VectorNormalize (r_spritedesc.vright);
		r_spritedesc.vpn[0] = -r_spritedesc.vright[1];
		r_spritedesc.vpn[1] = r_spritedesc.vright[0];
		r_spritedesc.vpn[2] = 0;
					// CrossProduct (r_spritedesc.vright, r_spritedesc.vup,
					//		 r_spritedesc.vpn)
	}
	else if (psprite->type == SPR_ORIENTED)
	{
	// generate the sprite's axes, according to the sprite's world orientation
		AngleVectors (currententity->angles, r_spritedesc.vpn, r_spritedesc.vright, r_spritedesc.vup);
	}
	else if (psprite->type == SPR_VP_PARALLEL_ORIENTED)
	{
	// generate the sprite's axes, parallel to the viewplane, but rotated in
	// that plane around the center according to the sprite entity's roll
	// angle. So vpn stays the same, but vright and vup rotate
		angle = currententity->angles[ROLL] * (M_PI*2 / 360);
		sr = sin(angle);
		cr = cos(angle);

		for (i=0 ; i<3 ; i++)
		{
			r_spritedesc.vpn[i] = vpn[i];
			r_spritedesc.vright[i] = vright[i] * cr + vup[i] * sr;
			r_spritedesc.vup[i] = vright[i] * -sr + vup[i] * cr;
		}
	}
	else
	{
		Sys_Error ("R_DrawSprite: Bad sprite type %d", psprite->type);
	}

	GL_Bind(frame->gl_texturenum);

	glBegin_fp (GL_QUADS);

	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glTexCoord2f_fp (0, 1);
	VectorMA (e->origin, frame->down, r_spritedesc.vup, point);
	VectorMA (point, frame->left, r_spritedesc.vright, point);
	glVertex3fv_fp (point);

	glTexCoord2f_fp (0, 0);
	VectorMA (e->origin, frame->up, r_spritedesc.vup, point);
	VectorMA (point, frame->left, r_spritedesc.vright, point);
	glVertex3fv_fp (point);

	glTexCoord2f_fp (1, 0);
	VectorMA (e->origin, frame->up, r_spritedesc.vup, point);
	VectorMA (point, frame->right, r_spritedesc.vright, point);
	glVertex3fv_fp (point);

	glTexCoord2f_fp (1, 1);
	VectorMA (e->origin, frame->down, r_spritedesc.vup, point);
	VectorMA (point, frame->right, r_spritedesc.vright, point);
	glVertex3fv_fp (point);

	glEnd_fp ();

	//restore tex parms
	// replaced GL_REPEAT with GL_CLAMP below (courtesy of Pa3PyX)
	// fixing the demoness flame's "lines" bug S.A
	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glDisable_fp (GL_BLEND);
}


/*
=============================================================

ALIAS MODELS

=============================================================
*/

#if defined(H2W)
// hexenworld needs r_avertexnormals in R_EntityParticles
#define NUMVERTEXNORMALS	162
float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};
#endif	// H2W

static vec3_t	shadevector;
static float	shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT		16
static float	r_avertexnormal_dots[SHADEDOT_QUANT][256] = {
#include "anorm_dots.h"
};

static float	shadelightcolor[4];
static float	*shadedots = r_avertexnormal_dots[0];

static int	lastposenum;

/*
=============
GL_DrawAliasFrame
=============
*/
extern float	RTint[256], GTint[256], BTint[256];

static void GL_DrawAliasFrame (aliashdr_t *paliashdr, int posenum)
{
	float		l;
	trivertx_t	*verts;
	int		*order;
	int		count;
	float		r, g, b;
	byte		ColorShade;

	lastposenum = posenum;

	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	ColorShade = currententity->colorshade;

	if (ColorShade)
	{
		r = RTint[ColorShade];
		g = GTint[ColorShade];
		b = BTint[ColorShade];
	}
	else
		r = g = b = 1;

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			glBegin_fp (GL_TRIANGLE_FAN);
		}
		else
			glBegin_fp (GL_TRIANGLE_STRIP);

		do
		{
			// texture coordinates come from the draw list
			glTexCoord2f_fp (((float *)order)[0], ((float *)order)[1]);
			order += 2;

			// normals and vertexes come from the frame list

			if (gl_lightmap_format == GL_RGBA)
			{
				l = shadedots[verts->lightnormalindex];
				glColor4f_fp (l * shadelightcolor[0], l * shadelightcolor[1], l * shadelightcolor[2], model_constant_alpha);
			}
			else
			{
				l = shadedots[verts->lightnormalindex] * shadelight;
				glColor4f_fp (r*l, g*l, b*l, model_constant_alpha);
			}

			glVertex3f_fp (verts->v[0], verts->v[1], verts->v[2]);
			verts++;
		} while (--count);

		glEnd_fp ();
	}
}


/*
=============
GL_DrawAliasShadow
=============
*/
extern	vec3_t			lightspot;

static void GL_DrawAliasShadow (aliashdr_t *paliashdr, int posenum)
{
	trivertx_t	*verts;
	int		*order;
	vec3_t		point;
	float		height, lheight;
	int		count;

	lheight = currententity->origin[2] - lightspot[2];

	height = 0;
	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	height = -lheight + 1.0;

	if (have_stencil == true && gl_stencilshadow.value != 0)
	{
		glEnable_fp(GL_STENCIL_TEST);
		glStencilFunc_fp(GL_EQUAL,1,2);
		glStencilOp_fp(GL_KEEP,GL_KEEP,GL_INCR);
	}

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			glBegin_fp (GL_TRIANGLE_FAN);
		}
		else
			glBegin_fp (GL_TRIANGLE_STRIP);

		do
		{
			// texture coordinates come from the draw list
			// (skipped for shadows) glTexCoord2fv_fp ((float *)order);
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0]*(point[2]+lheight);
			point[1] -= shadevector[1]*(point[2]+lheight);
			point[2] = height;
//			height -= 0.001;
			glVertex3fv_fp (point);

			verts++;
		} while (--count);

		glEnd_fp ();
	}

	if (have_stencil == true && gl_stencilshadow.value != 0)
		glDisable_fp(GL_STENCIL_TEST);
}


/*
=================
R_SetupAliasFrame

=================
*/
static void R_SetupAliasFrame (int frame, aliashdr_t *paliashdr)
{
	int				pose, numposes;
	float			interval;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		interval = paliashdr->frames[frame].interval;
		pose += (int)(cl.time / interval) % numposes;
	}

	GL_DrawAliasFrame (paliashdr, pose);
}


/*
=================
R_DrawAliasModel

=================
*/
static void R_DrawAliasModel (entity_t *e)
{
	int		i;
	int		lnum;
	vec3_t		dist;
	float		add;
	model_t		*clmodel;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr;
	float		an;
	static float	tmatrix[3][4];
	float		entScale;
	float		xyfact = 1.0, zfact = 1.0; // avoid compiler warning
	qpic_t		*stonepic;
	glpic_t		*gl;
	char		temp[80];
	int		mls;
	int		*lpc;
	vec3_t		adjust_origin;

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

	if (!AlwaysDrawModel && R_CullBox (mins, maxs))
		return;

	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	//
	// get lighting information
	//

	VectorCopy(currententity->origin, adjust_origin);
	adjust_origin[2] += (currententity->model->mins[2] + currententity->model->maxs[2]) / 2;
	ambientlight = shadelight = R_LightPoint (adjust_origin);

	if (gl_lightmap_format == GL_RGBA)
	{	// get lighting information
		lpc = R_LightPointColour (adjust_origin);
		shadelightcolor[0] = (float) lpc[0];
		shadelightcolor[1] = (float) lpc[1];
		shadelightcolor[2] = (float) lpc[2];
		shadelightcolor[3] = (float) lpc[3];
		ambientlight = shadelightcolor[3];
	}

	// always give the gun some light
	if (e == &cl.viewent && ambientlight < 24)
		ambientlight = shadelight = 24;

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if (cl_dlights[lnum].die >= cl.time)
		{
			VectorSubtract (currententity->origin,
							cl_dlights[lnum].origin,
							dist);
			add = cl_dlights[lnum].radius - VectorLength(dist);

			if (add > 0)
			{
				ambientlight += add;
				shadelightcolor[0] += (cl_dlights[lnum].color[0] * add);
				shadelightcolor[1] += (cl_dlights[lnum].color[1] * add);
				shadelightcolor[2] += (cl_dlights[lnum].color[2] * add);
				shadelightcolor[3] += add;
			}
		}
	}

	// clamp lighting so it doesn't overbright as much
	if (ambientlight > 128)
		ambientlight = 128;
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	mls = currententity->drawflags&MLS_MASKIN;
	if (currententity->model->flags&EF_ROTATE)
	{
		ambientlight = shadelight =
					60+34+sin(currententity->origin[0]
						+ currententity->origin[1]
						+ (cl.time*3.8))*34;
	}
	else if (mls == MLS_ABSLIGHT)
	{
		shadelightcolor[0] =
		shadelightcolor[1] =
		shadelightcolor[2] =
		ambientlight =
		shadelight =
				currententity->abslight;
	}
	else if (mls != MLS_NONE)
	{
		// Use a model light style (25-30)
		shadelightcolor[0] =
		shadelightcolor[1] =
		shadelightcolor[2] =
		ambientlight =
		shadelight =
				d_lightstylevalue[24+mls]/2;
	}

	shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	shadelight = shadelight / 200.0;

	VectorScale(shadelightcolor, 1.0f / 200.0f, shadelightcolor);

	an = e->angles[1]/180*M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);

	//
	// locate the proper data
	//
	paliashdr = (aliashdr_t *)Mod_Extradata (currententity->model);

	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//

	glPushMatrix_fp ();
	R_RotateForEntity2(e);

	if (currententity->scale != 0 && currententity->scale != 100)
	{
		entScale = (float)currententity->scale/100.0;
		switch(currententity->drawflags&SCALE_TYPE_MASKIN)
		{
		case SCALE_TYPE_UNIFORM:
			tmatrix[0][0] = paliashdr->scale[0]*entScale;
			tmatrix[1][1] = paliashdr->scale[1]*entScale;
			tmatrix[2][2] = paliashdr->scale[2]*entScale;
			xyfact = zfact = (entScale-1.0)*127.95;
			break;
		case SCALE_TYPE_XYONLY:
			tmatrix[0][0] = paliashdr->scale[0]*entScale;
			tmatrix[1][1] = paliashdr->scale[1]*entScale;
			tmatrix[2][2] = paliashdr->scale[2];
			xyfact = (entScale-1.0)*127.95;
			zfact = 1.0;
			break;
		case SCALE_TYPE_ZONLY:
			tmatrix[0][0] = paliashdr->scale[0];
			tmatrix[1][1] = paliashdr->scale[1];
			tmatrix[2][2] = paliashdr->scale[2]*entScale;
			xyfact = 1.0;
			zfact = (entScale-1.0)*127.95;
			break;
		}

		switch (currententity->drawflags&SCALE_ORIGIN_MASKIN)
		{
		case SCALE_ORIGIN_CENTER:
			tmatrix[0][3] = paliashdr->scale_origin[0]-paliashdr->scale[0]*xyfact;
			tmatrix[1][3] = paliashdr->scale_origin[1]-paliashdr->scale[1]*xyfact;
			tmatrix[2][3] = paliashdr->scale_origin[2]-paliashdr->scale[2]*zfact;
			break;
		case SCALE_ORIGIN_BOTTOM:
			tmatrix[0][3] = paliashdr->scale_origin[0]-paliashdr->scale[0]*xyfact;
			tmatrix[1][3] = paliashdr->scale_origin[1]-paliashdr->scale[1]*xyfact;
			tmatrix[2][3] = paliashdr->scale_origin[2];
			break;
		case SCALE_ORIGIN_TOP:
			tmatrix[0][3] = paliashdr->scale_origin[0]-paliashdr->scale[0]*xyfact;
			tmatrix[1][3] = paliashdr->scale_origin[1]-paliashdr->scale[1]*xyfact;
			tmatrix[2][3] = paliashdr->scale_origin[2]-paliashdr->scale[2]*zfact*2.0;
			break;
		}
	}
	else
	{
		tmatrix[0][0] = paliashdr->scale[0];
		tmatrix[1][1] = paliashdr->scale[1];
		tmatrix[2][2] = paliashdr->scale[2];
		tmatrix[0][3] = paliashdr->scale_origin[0];
		tmatrix[1][3] = paliashdr->scale_origin[1];
		tmatrix[2][3] = paliashdr->scale_origin[2];
	}

	if (clmodel->flags&EF_ROTATE)
	{
		// Floating motion
		tmatrix[2][3] += sin(currententity->origin[0]
					+ currententity->origin[1]
					+ (cl.time*3)) * 5.5;
	}

// [0][3] [1][3] [2][3]
//	glTranslatef_fp (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	glTranslatef_fp (tmatrix[0][3],tmatrix[1][3],tmatrix[2][3]);
// [0][0] [1][1] [2][2]
//	glScalef_fp (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	glScalef_fp (tmatrix[0][0],tmatrix[1][1],tmatrix[2][2]);

	if ((currententity->model->flags & EF_SPECIAL_TRANS))
	{
		glEnable_fp (GL_BLEND);
		glBlendFunc_fp (GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
	//	glColor3f_fp (1,1,1);
		model_constant_alpha = 1.0f;
		glDisable_fp (GL_CULL_FACE);
	}
	else if (currententity->drawflags & DRF_TRANSLUCENT)
	{
		glEnable_fp (GL_BLEND);
		glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//	glColor4f_fp (1,1,1,r_wateralpha.value);
		model_constant_alpha = r_wateralpha.value;
	}
	else if ((currententity->model->flags & EF_TRANSPARENT))
	{
		glEnable_fp (GL_BLEND);
		glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//	glColor3f_fp (1,1,1);
		model_constant_alpha = 1.0f;
	}
	else if ((currententity->model->flags & EF_HOLEY))
	{
		glEnable_fp (GL_BLEND);
		glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//	glColor3f_fp (1,1,1);
		model_constant_alpha = 1.0f;
	}
	else
	{
		glColor3f_fp (1,1,1);
		model_constant_alpha = 1.0f;
	}

	if (currententity->skinnum >= 100)
	{
		if (currententity->skinnum > 255)
		{
			Sys_Error ("skinnum > 255");
		}

		if (gl_extra_textures[currententity->skinnum-100] == -1) // Need to load it in
		{
			sprintf(temp,"gfx/skin%d.lmp",currententity->skinnum);
			stonepic = Draw_CachePic(temp);
			gl = (glpic_t *)stonepic->data;
			gl_extra_textures[currententity->skinnum-100] = gl->texnum;
		}

		GL_Bind(gl_extra_textures[currententity->skinnum-100]);
	}
	else
	{
		GL_Bind(paliashdr->gl_texturenum[currententity->skinnum]);

		// we can't dynamically colormap textures, so they are cached
		// seperately for the players.  Heads are just uncolored.

		if (currententity->colormap != vid.colormap && !gl_nocolors.value)
		{
			if (currententity->model == player_models[0] ||
			    currententity->model == player_models[1] ||
			    currententity->model == player_models[2] ||
			    currententity->model == player_models[4] ||	/* demoness */
			    currententity->model == player_models[3])
			{
				i = currententity - cl_entities - 1;
				if (i >= 0 && i < cl.maxclients)
				{
					GL_Bind(playertextures[i]);
				}
			}
		}
	}

	if (gl_smoothmodels.value)
		glShadeModel_fp (GL_SMOOTH);
	glTexEnvf_fp(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.value)
		glHint_fp (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	R_SetupAliasFrame (currententity->frame, paliashdr);
	if ((currententity->drawflags & DRF_TRANSLUCENT) ||
			(currententity->model->flags & EF_SPECIAL_TRANS) ||
			(currententity->model->flags & EF_TRANSPARENT) ||
			(currententity->model->flags & EF_HOLEY)	)
		glDisable_fp (GL_BLEND);

	if ((currententity->model->flags & EF_SPECIAL_TRANS))
		glEnable_fp (GL_CULL_FACE);

	glTexEnvf_fp (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glShadeModel_fp (GL_FLAT);
	if (gl_affinemodels.value)
		glHint_fp (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glPopMatrix_fp ();

	if (r_shadows.value)
	{
		glPushMatrix_fp ();
		R_RotateForEntity2 (e);
		glDisable_fp (GL_TEXTURE_2D);
		glEnable_fp (GL_BLEND);
		glColor4f_fp (0,0,0,0.5);
		glDepthMask_fp (0);	// prevent Z fighting
		GL_DrawAliasShadow (paliashdr, lastposenum);
		glDepthMask_fp (1);
		glEnable_fp (GL_TEXTURE_2D);
		glDisable_fp (GL_BLEND);
		glColor4f_fp (1,1,1,1);
		glPopMatrix_fp ();
	}
}

//=============================================================================

typedef struct sortedent_s {
	entity_t	*ent;
	vec_t		len;
} sortedent_t;

static sortedent_t	cl_transvisedicts[MAX_VISEDICTS];
static sortedent_t	cl_transwateredicts[MAX_VISEDICTS];

static int			cl_numtransvisedicts;
static int			cl_numtranswateredicts;

/*
=============
R_DrawEntitiesOnList
=============
*/
static void R_DrawEntitiesOnList (void)
{
	int			i;
	qboolean	item_trans;
	mleaf_t		*pLeaf;

	cl_numtransvisedicts = 0;
	cl_numtranswateredicts = 0;

	if (!r_drawentities.value)
		return;

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		// chase-cam pitch adj. by FrikaC
		if (currententity == &cl_entities[cl.viewentity])
			currententity->angles[0] *= 0.3;

		switch (currententity->model->type)
		{
		case mod_alias:
			item_trans = ((currententity->drawflags & DRF_TRANSLUCENT) ||
						  (currententity->model->flags & (EF_TRANSPARENT|EF_HOLEY|EF_SPECIAL_TRANS))) != 0;
			if (!item_trans)
				R_DrawAliasModel (currententity);
			break;

		case mod_brush:
			item_trans = ((currententity->drawflags & DRF_TRANSLUCENT)) != 0;
			if (!item_trans)
				R_DrawBrushModel (currententity,false);
			break;

		case mod_sprite:
			item_trans = true;
			break;

		default:
			item_trans = false;
			break;
		}

		if (item_trans)
		{
			pLeaf = Mod_PointInLeaf (currententity->origin, cl.worldmodel);
		//	if (pLeaf->contents == CONTENTS_EMPTY)
			if (pLeaf->contents != CONTENTS_WATER)
				cl_transvisedicts[cl_numtransvisedicts++].ent = currententity;
			else
				cl_transwateredicts[cl_numtranswateredicts++].ent = currententity;
		}
	}
}


/*
================
R_DrawTransEntitiesOnList
Implemented by: jack
================
*/

static int transCompare (const void *arg1, const void *arg2)
{
	const sortedent_t *a1, *a2;
	a1 = (sortedent_t *) arg1;
	a2 = (sortedent_t *) arg2;
	return (a2->len - a1->len); // Sorted in reverse order.  Neat, huh?
}

static void R_DrawTransEntitiesOnList (qboolean inwater)
{
	int		i;
	int		numents;
	sortedent_t	*theents;
	int	depthMaskWrite = 0;
	vec3_t	result;

	theents = (inwater) ? cl_transwateredicts : cl_transvisedicts;
	numents = (inwater) ? cl_numtranswateredicts : cl_numtransvisedicts;

	for (i = 0 ; i < numents ; i++)
	{
		VectorSubtract(theents[i].ent->origin, r_origin, result);
	//	theents[i].len = VectorLength(result);
		theents[i].len = (result[0] * result[0]) + (result[1] * result[1]) + (result[2] * result[2]);
	}

	qsort((void *) theents, numents, sizeof(sortedent_t), transCompare);
	// Add in BETTER sorting here

	glDepthMask_fp(0);
	for (i = 0 ; i < numents ; i++)
	{
		currententity = theents[i].ent;

		switch (currententity->model->type)
		{
		case mod_alias:
			if (!depthMaskWrite)
			{
				depthMaskWrite = 1;
				glDepthMask_fp(1);
			}
			R_DrawAliasModel (currententity);
			break;
		case mod_brush:
			if (!depthMaskWrite)
			{
				depthMaskWrite = 1;
				glDepthMask_fp(1);
			}
			R_DrawBrushModel (currententity, true);
			break;
		case mod_sprite:
			if (depthMaskWrite)
			{
				depthMaskWrite = 0;
				glDepthMask_fp(0);
			}
			R_DrawSpriteModel (currententity);
			break;
		}
	}

	if (!depthMaskWrite)
		glDepthMask_fp(1);
}

//=============================================================================


// Glow styles. These rely on unchanged game code!
#define	TORCH_STYLE	1	/* Flicker	*/
#define	MISSILE_STYLE	6	/* Flicker	*/
#define	PULSE_STYLE	11	/* Slow pulse	*/

static void R_DrawGlow (entity_t *e)
{
	model_t		*clmodel;

	clmodel = currententity->model;

	// Torches & Flames
	if ((gl_glows.value && (clmodel->ex_flags & XF_TORCH_GLOW)) ||
	    (gl_missile_glows.value && (clmodel->ex_flags & XF_MISSILE_GLOW)) ||
	    (gl_other_glows.value && (clmodel->ex_flags & XF_GLOW)) )
	{
		// NOTE: It would be better if we batched these up.
		//	 All those state changes are not nice. KH
		vec3_t	lightorigin;		// Origin of torch.
		vec3_t	glow_vect;		// Vector to torch.
		float	radius;			// Radius of torch flare.
		float	distance;		// Vector distance to torch.
		float	intensity;		// Intensity of torch flare.
		int	i, j;
		vec3_t	vp2;

		// NOTE: I don't think this is centered on the model.
		VectorCopy(currententity->origin, lightorigin);

		radius = 20.0f;

		// for mana, make it bit bigger
		if ( !Q_strncasecmp(clmodel->name, "models/i_btmana", 15))
			radius += 5.0f;

		VectorSubtract(lightorigin, r_origin, vp2);

		// See if view is outside the light.
		distance = VectorLength(glow_vect);
		// See if view is outside the light.
		distance = VectorLength(vp2);

		if (distance > radius)
		{
			VectorNormalize(vp2);
			glPushMatrix_fp();

			// Translate the glow to coincide with the flame. KH
			if (clmodel->ex_flags & XF_TORCH_GLOW)
			{
				if (!Q_strncasecmp (clmodel->name, "models/eflmtrch",15))
					// egypt torch fix
					glTranslatef_fp ( cos(e->angles[1]/180*M_PI)*8.0f,
								sin(e->angles[1]/180*M_PI)*8.0f,
								16.0f);
				else
					glTranslatef_fp (0.0f, 0.0f, 8.0f);
			}

			// 'floating' movement
			if(clmodel->flags & EF_ROTATE)
				glTranslatef_fp (0, 0, sin(currententity->origin[0]+currententity->origin[1]+(cl.time*3))*5.5);

			glBegin_fp(GL_TRIANGLE_FAN);
			// Diminish torch flare inversely with distance.
			intensity = (1024.0f - distance) / 1024.0f;

			// Invert (fades as you approach).
			intensity = (1.0f - intensity);

			// Clamp, but don't let the flare disappear.
			if (intensity > 1.0f)
				intensity = 1.0f;
			if (intensity < 0.0f)
				intensity = 0.0f;

			// Now modulate with flicker.
			j = 0;	// avoid compiler warning
			if (clmodel->ex_flags & XF_TORCH_GLOW)
			{
				i = (int)(cl.time*10);
				if (!cl_lightstyle[TORCH_STYLE].length)
				{
					j = 256;
				}
				else
				{
					j = i % cl_lightstyle[TORCH_STYLE].length;
					j = cl_lightstyle[TORCH_STYLE].map[j] - 'a';
					j = j*22;
				}
			}
			else if (clmodel->ex_flags & XF_MISSILE_GLOW)
			{
				i = (int)(cl.time*10);
				if (!cl_lightstyle[MISSILE_STYLE].length)
				{
					j = 256;
				}
				else
				{
					j = i % cl_lightstyle[MISSILE_STYLE].length;
					j = cl_lightstyle[MISSILE_STYLE].map[j] - 'a';
					j = j*22;
				}
			}
			else if (clmodel->ex_flags & XF_GLOW)
			{
				i = (int)(cl.time*10);
				if (!cl_lightstyle[PULSE_STYLE].length)
				{
					j = 256;
				}
				else
				{
					j = i % cl_lightstyle[PULSE_STYLE].length;
					j = cl_lightstyle[PULSE_STYLE].map[j] - 'a';
					j = j*22;
				}
			}

			intensity *= ((float)j / 255.0f);

			if (clmodel->ex_flags & XF_TORCH_GLOW)
				// Set yellow intensity
				glColor4f_fp (0.8f*intensity, 0.4f*intensity, 0.1f*intensity, 1.0f);
			else
				glColor4f_fp (clmodel->glow_color[0]*intensity,
						clmodel->glow_color[1]*intensity,
						clmodel->glow_color[2]*intensity,
						0.5f);

			for (i=0 ; i<3 ; i++)
				glow_vect[i] = lightorigin[i] - vp2[i]*radius;

			glVertex3fv_fp(glow_vect);

			glColor4f_fp(0.0f, 0.0f, 0.0f, 1.0f);

			for (i=16; i>=0; i--)
			{
				float a = i/16.0f * M_PI*2;

				for (j=0; j<3; j++)
					glow_vect[j] = lightorigin[j] + 
							vright[j]*cos(a)*radius +
							vup[j]*sin(a)*radius;

				glVertex3fv_fp(glow_vect);
			}

			glEnd_fp();
			glColor4f_fp (0.0f,0.0f,0.0f,1.0f);
			// Restore previous matrix
			glPopMatrix_fp();
		}
	}
}

static void R_DrawAllGlows (void)
{
	int		i;

	if (!r_drawentities.value)
		return;

	glDepthMask_fp (0);
	glDisable_fp (GL_TEXTURE_2D);
	glShadeModel_fp (GL_SMOOTH);
	glEnable_fp (GL_BLEND);
	glBlendFunc_fp (GL_ONE, GL_ONE);

	for (i=0 ; i < cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_alias:
			R_DrawGlow (currententity);
			break;
		default:
			break;
		}
	}

	glDisable_fp (GL_BLEND);
	glEnable_fp (GL_TEXTURE_2D);
	glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask_fp (1);
	glShadeModel_fp (GL_FLAT);
	glTexEnvf_fp(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

//=============================================================================


/*
=============
R_DrawViewModel
=============
*/
static void R_DrawViewModel (void)
{
	int			lnum;
	vec3_t		dist;
	float		add;
	dlight_t	*dl;
	int		*lpc;

	currententity = &cl.viewent;

	if (!currententity->model)
		return;

	if (gl_lightmap_format == GL_RGBA)
	{
		lpc = R_LightPointColour (currententity->origin);
		// always give some light on gun
		ambientlight = (float) lpc[3] > 24 ? lpc[3] : 24;
		shadelightcolor[0] = (float) lpc[0] > 24 ? lpc[0] : 24;
		shadelightcolor[1] = (float) lpc[1] > 24 ? lpc[1] : 24;
		shadelightcolor[2] = (float) lpc[2] > 24 ? lpc[2] : 24;
		shadelightcolor[3] = (float) lpc[3] > 24 ? lpc[3] : 24;
	}
	else
	{
		ambientlight = R_LightPoint (currententity->origin);
		if (ambientlight < 24)
			ambientlight = 24;	// always give some light on gun
	}

// add dynamic lights
	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		dl = &cl_dlights[lnum];
		if (!dl->radius)
			continue;
		if (dl->die < cl.time)
			continue;

		VectorSubtract (currententity->origin, dl->origin, dist);
		add = dl->radius - VectorLength(dist);
		if (add > 0)
		{
			if (gl_lightmap_format == GL_RGBA)
			{
				shadelightcolor[0] += (float) (dl->color[0] * add);
				shadelightcolor[1] += (float) (dl->color[1] * add);
				shadelightcolor[2] += (float) (dl->color[2] * add);
				shadelightcolor[3] += (float) add;
			}
			else
			{
				shadelight += (float) add;
			}

			ambientlight += add;
		}
	}

	cl.light_level = ambientlight;

	if ((cl.v.health <= 0) ||
	    (chase_active.value) ||
	    (cl.items & IT_INVISIBILITY) ||
	    (!r_drawviewmodel.value) ||
	    (!r_drawentities.value) ||
	    (envmap))
	{
		return;
	}

	// hack the depth range to prevent view model from poking into walls
	glDepthRange_fp (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));
	AlwaysDrawModel = true;
	R_DrawAliasModel (currententity);
	AlwaysDrawModel = false;
	glDepthRange_fp (gldepthmin, gldepthmax);
}

//=============================================================================


/*
===============
R_MarkLeaves
===============
*/
static void R_MarkLeaves (void)
{
	byte	*vis;
	mnode_t	*node;
	int		i;
	byte	solid[4096];

	if (r_oldviewleaf == r_viewleaf && !r_novis.value)
		return;

	if (mirror)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis.value)
	{
		vis = solid;
		memset (solid, 0xff, (cl.worldmodel->numleafs+7)>>3);
	}
	else
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
	{
		if (vis[i>>3] & (1<<(i&7)))
		{
			node = (mnode_t *)&cl.worldmodel->leafs[i+1];
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}

//=============================================================================


/*
=================
GL_DrawBlendPoly

Renders a polygon covering the whole screen. For
fullscreen color blending and approximated gamma
correction. To be called from R_PolyBlend().
=================
*/
static void GL_DrawBlendPoly (void)
{
	glBegin_fp (GL_QUADS);
	glVertex3f_fp (10, 100, 100);
	glVertex3f_fp (10, -100, 100);
	glVertex3f_fp (10, -100, -100);
	glVertex3f_fp (10, 100, -100);
	glEnd_fp ();
}

/*
=================
GL_DoGamma

Uses GL_DrawBlendPoly() for gamma correction.
This trick is useful if normal ways of gamma
adjustment fail: In case of 3dfx Voodoo1/2/Rush,
we can't use 3dfx specific extensions in unix,
so this can be our friend at a cost of 4-5 fps.
To be called from R_PolyBlend().
Idea originally nicked from LordHavoc, re-worked
and extended by muff - 5 Feb 2001.
=================
*/
static void GL_DoGamma (void)
{
	if (v_gamma.value < 0.2)
		v_gamma.value = 0.2;
	if (v_gamma.value >= 1)
	{
		v_gamma.value = 1;
		return;
	}

	// this actually does brighten the picture..
	glBlendFunc_fp (GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f_fp (1, 1, 1, v_gamma.value);

	GL_DrawBlendPoly ();
	// if we do this twice, we double the brightening
	// effect for a wider range of gamma's
	//GL_DrawBlendPoly ();
}

/*
============
R_PolyBlend
============
*/
static void R_PolyBlend (void)
{
	if (!gl_polyblend.value)
		return;

	glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable_fp (GL_BLEND);
	glDisable_fp (GL_DEPTH_TEST);
	glDisable_fp (GL_TEXTURE_2D);

	glLoadIdentity_fp ();

	glRotatef_fp (-90,  1, 0, 0);	// put Z going up
	glRotatef_fp (90,  0, 0, 1);	// put Z going up

	if (v_blend[3])
	{
		glColor4fv_fp (v_blend);
		GL_DrawBlendPoly ();
	}

	if (gl_dogamma)
		GL_DoGamma ();

	glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable_fp (GL_BLEND);
	glEnable_fp (GL_TEXTURE_2D);
	glEnable_fp (GL_ALPHA_TEST);
}

//=============================================================================


static int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}

static void R_SetFrustum (void)
{
	int		i;

	// front side is visible

	VectorAdd (vpn, vright, frustum[0].normal);
	VectorSubtract (vpn, vright, frustum[1].normal);

	VectorAdd (vpn, vup, frustum[2].normal);
	VectorSubtract (vpn, vup, frustum[3].normal);

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}


/*
===============
R_SetupFrame
===============
*/
static void R_SetupFrame (void)
{
// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
		Cvar_SetValue ("r_fullbright", 0);

	R_AnimateLight ();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;
}

static void MYgluPerspective (GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
	GLdouble	xmin, xmax, ymin, ymax;

	ymax = zNear * tan(fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	glFrustum_fp (xmin, xmax, ymin, ymax, zNear, zFar);
}

/*
=============
R_SetupGL
=============
*/
static void R_SetupGL (void)
{
	float	screenaspect;
	float	yfov;
	int	x, x2, y2, y, w, h;

	//
	// set up viewpoint
	//
	glMatrixMode_fp(GL_PROJECTION);
	glLoadIdentity_fp ();

	x  =  r_refdef.vrect.x * glwidth/vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width;
	y  = (vid.height - r_refdef.vrect.y) * glheight/vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < glheight)
		y++;

	w = x2 - x;
	h = y - y2;

	if (envmap)
	{
		x = y2 = 0;
		w = h = 256;
	}

	glViewport_fp (glx + x, gly + y2, w, h);
	screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
	yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;
	MYgluPerspective (yfov, screenaspect, 4, 4096);

	if (mirror)
	{
		if (mirror_plane->normal[2])
			glScalef_fp (1, -1, 1);
		else
			glScalef_fp (-1, 1, 1);
		glCullFace_fp(GL_BACK);
	}
	else
		glCullFace_fp(GL_FRONT);

	glMatrixMode_fp(GL_MODELVIEW);
	glLoadIdentity_fp ();

	glRotatef_fp (-90,  1, 0, 0);	// put Z going up
	glRotatef_fp (90,  0, 0, 1);	// put Z going up
	glRotatef_fp (-r_refdef.viewangles[2],  1, 0, 0);
	glRotatef_fp (-r_refdef.viewangles[0],  0, 1, 0);
	glRotatef_fp (-r_refdef.viewangles[1],  0, 0, 1);
	glTranslatef_fp (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

	glGetFloatv_fp (GL_MODELVIEW_MATRIX, r_world_matrix);

	//
	// set drawing parms
	//
	if (gl_cull.value)
		glEnable_fp(GL_CULL_FACE);
	else
		glDisable_fp(GL_CULL_FACE);

	glDisable_fp(GL_BLEND);
	glDisable_fp(GL_ALPHA_TEST);
	glEnable_fp(GL_DEPTH_TEST);
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
static void R_RenderScene (void)
{
	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();		// adds static entities to the list

	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	R_DrawEntitiesOnList ();

	R_DrawAllGlows();

	R_RenderDlights ();

#ifdef GLTEST
	Test_Draw ();
#endif
}


/*
=============
R_Clear
=============
*/
static void R_Clear (void)
{
	if (r_mirroralpha.value != 1.0)
	{
		if (gl_clear.value)
			glClear_fp (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glClear_fp (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 0.5;
		glDepthFunc_fp (GL_LEQUAL);
	}
	else if (gl_ztrick.value)
	{
		static int trickframe;

		if (gl_clear.value)
			glClear_fp (GL_COLOR_BUFFER_BIT);

		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			glDepthFunc_fp (GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			glDepthFunc_fp (GL_GEQUAL);
		}
	}
	else
	{
		if (gl_clear.value)
			glClear_fp (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glClear_fp (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		glDepthFunc_fp (GL_LEQUAL);
	}

	glDepthRange_fp (gldepthmin, gldepthmax);

	if (have_stencil == true && gl_stencilshadow.value > 0 && r_shadows.value > 0)
	{
		glClearStencil_fp(1);
		glClear_fp(GL_STENCIL_BUFFER_BIT);
	}
}


/*
=============
R_Mirror
=============
*/
static void R_Mirror (void)
{
	float		d;
	msurface_t	*s;
	entity_t	*ent;

	if (!mirror)
		return;

	memcpy (r_base_world_matrix, r_world_matrix, sizeof(r_base_world_matrix));

	d = DotProduct (r_refdef.vieworg, mirror_plane->normal) - mirror_plane->dist;
	VectorMA (r_refdef.vieworg, -2*d, mirror_plane->normal, r_refdef.vieworg);

	d = DotProduct (vpn, mirror_plane->normal);
	VectorMA (vpn, -2*d, mirror_plane->normal, vpn);

	r_refdef.viewangles[0] = -asin (vpn[2])/M_PI*180;
	r_refdef.viewangles[1] = atan2 (vpn[1], vpn[0])/M_PI*180;
	r_refdef.viewangles[2] = -r_refdef.viewangles[2];

	ent = &cl_entities[cl.viewentity];
	if (cl_numvisedicts < MAX_VISEDICTS)
	{
		cl_visedicts[cl_numvisedicts] = ent;
		cl_numvisedicts++;
	}

	gldepthmin = 0.5;
	gldepthmax = 1;
	glDepthRange_fp (gldepthmin, gldepthmax);
	glDepthFunc_fp (GL_LEQUAL);

	R_RenderScene ();

	glDepthMask_fp(0);

	R_DrawParticles ();

// THIS IS THE F*S*D(KCING MIRROR ROUTINE!  Go down!!!
	R_DrawTransEntitiesOnList (true); // This restores the depth mask

	R_DrawWaterSurfaces ();

	R_DrawTransEntitiesOnList (false);

	gldepthmin = 0;
	gldepthmax = 0.5;
	glDepthRange_fp (gldepthmin, gldepthmax);
	glDepthFunc_fp (GL_LEQUAL);

	// blend on top
	glEnable_fp (GL_BLEND);
	glMatrixMode_fp(GL_PROJECTION);
	if (mirror_plane->normal[2])
		glScalef_fp (1,-1,1);
	else
		glScalef_fp (-1,1,1);
	glCullFace_fp(GL_FRONT);
	glMatrixMode_fp(GL_MODELVIEW);

	glLoadMatrixf_fp (r_base_world_matrix);

	glColor4f_fp (1,1,1,r_mirroralpha.value);
	s = cl.worldmodel->textures[mirrortexturenum]->texturechain;
	for ( ; s ; s=s->texturechain)
		R_RenderBrushPoly (s, true);
	cl.worldmodel->textures[mirrortexturenum]->texturechain = NULL;
	glDisable_fp (GL_BLEND);
	glColor4f_fp (1,1,1,1);
}


/*
=============
R_PrintTimes
=============
*/
static void R_PrintTimes (void)
{
	float	r_time2;
	float	ms, fps;

	r_lasttime1 = r_time2 = Sys_DoubleTime();

	ms = 1000*(r_time2-r_time1);
	fps = 1000/ms;

	Con_Printf("%3.1f fps %5.0f ms\n%4i wpoly  %4i epoly\n",
		fps, ms, c_brush_polys, c_alias_polys);
}


/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	if (r_norefresh.value)
		return;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
		glFinish_fp ();
		if (r_wholeframe.value)
			r_time1 = r_lasttime1;
		else
			r_time1 = Sys_DoubleTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	mirror = false;

//	glFinish_fp ();

	R_Clear ();

	// render normal view
	R_RenderScene ();

	glDepthMask_fp(0);

	R_DrawParticles ();

	R_DrawTransEntitiesOnList (r_viewleaf->contents == CONTENTS_EMPTY); // This restores the depth mask

	R_DrawWaterSurfaces ();

	R_DrawTransEntitiesOnList (r_viewleaf->contents != CONTENTS_EMPTY);

	R_DrawViewModel();

	// render mirror view
	R_Mirror ();

	R_PolyBlend ();

	if (r_speeds.value)
		R_PrintTimes ();
}


/*
 * $Log: not supported by cvs2svn $
 * Revision 1.46  2006/07/02 11:45:30  sezero
 * minor optimiziations to mathlib: added VectorNegate and VectorClear macros
 * which stops vec3_origin usage in relevant calculations. renamed the Length
 * macro to VectorLength for consistancy. updated the utilities' mathlib for
 * similar macro usage as in the engine.
 *
 * Revision 1.45  2006/04/05 06:09:23  sezero
 * killed (almost) all H2MP ifdefs: this is the first step in making a single
 * binary which handles both h2 and h2mp properly. the only H2MP ifdefs left
 * are actually the ones for determining the icon and window manager text, so
 * nothing serious. the binary normally will only run the original h2 game.
 * if given a -portals or -missionpack or -h2mp argument, it will look for the
 * mission pack and run it (this is the same logic that quake used.) The only
 * serious side effect is that h2 and h2mp progs being different: This will be
 * solved by the next patch by adding support for the two progs versions into
 * a single binary.
 *
 * Revision 1.44  2006/03/24 15:05:39  sezero
 * killed the archive, server and info members of the cvar structure.
 * the new flags member is now employed for all those purposes. also
 * made all non-globally used cvars static.
 *
 * Revision 1.43  2006/03/23 20:01:33  sezero
 * made the lightmap format configurable via the menu system using a new
 * cvar gl_lightmapfmt. -lm_1 and -lm_4 are still functional as commandline
 * overrides even with commandline options dictating a direct load of a map,
 * such as +playdemo xxx or +load xxx. updated the opengl features menu with
 * new proper notifications about the colored lighting option's level reload
 * necessity and with new entries for the ligthmap format.
 *
 * Revision 1.42  2006/03/14 11:44:18  sezero
 * arghhh... cvs changelogs...
 *
 * Revision 1.41  2006/03/11 22:51:16  sezero
 * continue making static functions and vars static. whitespace and coding style
 * cleanup. part 41: gl_draw.c, gl_rlight.c, gl_rmain.c, gl_rmisc.c, gl_rsurf.c,
 * gl_test.c, gl_warp.c, gl_mesh.c, gl_refrag.c, gl_ngraph.c.
 * a few compiler warnings were fixed and tiny synchronizations between h2 and
 * h2w versions were done along the way. more to come.
 *
 * Revision 1.40  2006/03/10 08:08:45  sezero
 * Added support for colored lights and .lit files;. initially extracted
 * from jshexen2 (thanks Michal Wozniak). Colored lights and lit file
 * support is now added to hexenworld, as well, although the new cvars
 * gl_colored_dynamic_lights and gl_extra_dynamic_light aren't functional
 * for it: hexenworld had some sort of "colored" dynamic lights in it,
 * and they aren't changed by this patch. The cvars mentions are fully
 * functional for hexen2. Colored lights member of the dlight_t in h2
 * is changed to be an array of 4, instead of vec3_t as it used to appear
 * in jshexen2, so that things look more alike with the hw version. The
 * default lightmap format is changed to GL_RGBA, (previously it was
 * GL_LUMINANCE.) Command line arguments can be employed to change the
 * lightmap format:  -lm_1 : GL_LUMINANCE, -lm_4 : GL_RGBA (default).
 * (Note: Would it be a good (AND a feasible) idea to add a menu+cvar
 * control for the lightmap format?)
 *
 * Revision 1.39  2006/03/01 21:24:42  sezero
 * continue making static functions and vars static. whitespace and coding style
 * cleanup. (part 35:  anorm_dots.h, gl_warp_sin.h).
 *
 * Revision 1.38  2005/10/24 23:01:15  sezero
 * fixed "might be used uninitialized" warnings for xyfact and zfact
 *
 * Revision 1.37  2005/10/02 20:02:41  sezero
 * Fix glows of floating models (eg. mana) not floating along with the model.
 * Also a fix intersecting of mana boxes with glow. from Michal Wozniak.
 *
 * Revision 1.36  2005/09/19 19:50:10  sezero
 * fixed those famous spelling errors
 *
 * Revision 1.35  2005/09/12 08:17:46  sezero
 * added stencil buffered shadows (extracted from jshexen2, code by MrG)
 *
 * Revision 1.34  2005/08/17 00:02:57  sezero
 * R_DrawViewModel should not return until it determines the light_level,
 * otherwise the player will be considered invisible to the monsters.
 *
 * Revision 1.33  2005/06/26 11:11:57  sezero
 * Changed those pointless glow style const ints to defines
 *
 * Revision 1.32  2005/06/23 06:29:54  sezero
 * Ummm, spelling fixes (spesific <-> specific)
 *
 * Revision 1.31  2005/06/15 18:40:43  sezero
 * fifth model is for h2mp only and we've been out of bounds for ages here
 *
 * Revision 1.30  2005/06/15 13:18:17  sezero
 * killed the glfunc struct
 *
 * Revision 1.29  2005/06/06 10:19:40  sezero
 * ChaseCam clipping fixes from quakeforge and quakesrc.org (FrikaC).
 * Still not perfect, but much better than what we previously had.
 *
 * Revision 1.28  2005/06/05 16:14:07  sezero
 * patches I've been forgetting 6/6:
 * enable the irritating water-warp only by a cvar
 * and add water-ripple code by Jacques Krige
 *
 * Revision 1.27  2005/06/05 16:09:07  sezero
 * patches I've been forgetting 2/6:
 * prevent Z fighting in shadows
 *
 * Revision 1.26  2005/06/05 16:07:53  sezero
 * patches I've been forgetting 1/6:
 * use GL_LoadTexture for player textures
 *
 * Revision 1.25  2005/06/03 13:21:08  sezero
 * Updated gamma stuff. Enabled gl-gamma trick to work if all else
 * fail (such as in cases of Voodoo1/2).  IMO, the gl-gamma trick
 * looks very ugly and causes a 5 fps slowdown in 1x brughtening and
 * 10-12 fps in 2x brightening on my Voodoo2 box. We'll see..
 *
 * Revision 1.24  2005/05/29 08:53:57  sezero
 * get rid of silly name changes
 *
 * Revision 1.23  2005/05/29 08:38:12  sezero
 * get rid of the silly func name difference
 *
 * Revision 1.22  2005/05/28 12:31:47  sezero
 * added/updated multitexture detection. doesn't do anything yet.
 *
 * Revision 1.21  2005/05/28 12:02:02  sezero
 * Killed gl_texsort (thus, R_DrawSequentailPoly and co) which was
 * fairly broken. This also effectively killed the unfunctional
 * multitextune support in hexenworld: updated the detection but
 * doesn't do anything for now.
 *
 * Revision 1.20  2005/05/26 19:45:37  sezero
 * whitespace...
 *
 * Revision 1.19  2005/05/20 17:30:57  sezero
 * initial slight gl syncing-2 (kill unused c_sky_polys)
 *
 * Revision 1.18  2005/05/17 22:56:19  sezero
 * cleanup the "stricmp, strcmpi, strnicmp, Q_strcasecmp, Q_strncasecmp" mess:
 * Q_strXcasecmp will now be used throughout the code which are implementation
 * dependant defines for __GNUC__ (strXcasecmp) and _WIN32 (strXicmp)
 *
 * Revision 1.17  2005/05/07 08:07:47  sezero
 * Back-out my silly mistake of removing light_level code in R_DrawViewModel
 *
 * Revision 1.16  2005/04/30 11:12:08  sezero
 * commented-out the externs in gl_rmain.c which are to
 * serve not-yet-enabled GL_DoGamma()
 *
 * Revision 1.15  2005/04/30 08:19:35  sezero
 * R_DrawAliasModel does all these by itself. As a bonus, fixes two warnings
 * about ambientlight and shadelight global declerations being shadowed
 *
 * Revision 1.14  2005/04/08 18:16:35  sezero
 * - Merged a new R_PolyBlend code (from jshexen2)
 * - Added the GL_DoGamma function (to be called from R_PolyBlend)
 *   which brightens the display. Commented out for now. This little
 *   gem can be used to adjust gamma for Voodoo1/2/Rush.
 *
 * Revision 1.13  2005/01/24 20:32:56  sezero
 * add sky alpha
 *
 * Revision 1.12  2005/01/12 11:57:40  sezero
 * glfunc stuff (nitpicking)
 *
 * Revision 1.11  2005/01/10 14:31:19  sezero
 * comment "beautification"
 *
 * Revision 1.10  2005/01/10 14:30:06  sezero
 * glows indentation/whitespace fix. should be more readable now.
 *
 * Revision 1.9  2004/12/21 16:20:17  sezero
 * revert two commits (obsolete experimentals)
 *
 * Revision 1.8  2004/12/18 14:08:07  sezero
 * Clean-up and kill warnings 9:
 * Kill many unused vars.
 *
 * Revision 1.7  2004/12/18 13:24:37  sezero
 * fix fire attacks of the Demoness and Praevus which were drawing lines
 * across the screen. courtesy of Pa3Pyx.
 *
 * Revision 1.6  2004/12/12 23:16:44  sezero
 * two minor cvar annoyances
 *
 * Revision 1.5  2004/12/12 14:40:56  sezero
 * sync with steven
 *
 * Revision 1.4  2004/12/12 14:14:42  sezero
 * style changes to our liking
 *
 * Revision 1.3  2004/11/28 00:58:08  sezero
 *
 * Commit Steven's changes as of 2004.11.24:
 *
 * * Rewritten Help/Version message(s)
 * * Proper fullscreen mode(s) for OpenGL.
 * * Screen sizes are selectable with "-width" and "-height" options.
 * * Mouse grab in window modes , which is released when menus appear.
 * * Interactive video modes in software game disabled.
 * * Replaced Video Mode menu with a helpful message.
 * * New menu items for GL Glow, Chase mode, Draw Shadows.
 * * Changes to initial cvar_t variables:
 *      r_shadows, gl_other_glows, _windowed_mouse,
 *
 * Revision 1.2  2004/11/28 00:37:43  sezero
 * add gl-glow. code borrowed from the js sources
 *
 * Revision 1.1.1.1  2004/11/28 00:03:04  sezero
 * Initial import of AoT 1.2.0 code
 *
 * Revision 1.3  2002/01/06 02:49:24  theoddone33
 * Fix accidental checkin
 *
 * Revision 1.2  2002/01/06 02:46:37  theoddone33
 * Ensure screenshots and other files are written to ~/.aot/
 *
 * Revision 1.1  2002/01/02 15:14:44  phneutre
 * dlsym'ed all calls to GL functions with SDL_GL_GetProcAddress
 *
 * 
 * 4     3/30/98 10:57a Jmonroe
 * 
 * 3     2/26/98 9:19p Jmonroe
 * shortened memory struct for sprites, added sprite orientation code in
 * gl (need to test)
 * 
 * 2     1/18/98 8:06p Jmonroe
 * all of rick's patch code is in now
 * 
 * 38    10/28/97 6:11p Jheitzman
 * 
 * 36    9/25/97 2:10p Rjohnson
 * Smaller status bar
 * 
 * 35    9/23/97 9:47p Rjohnson
 * Fix for dedicated gl server and color maps for sheeps
 * 
 * 34    9/15/97 1:26p Rjohnson
 * Fixed sprites
 * 
 * 33    9/09/97 5:24p Rjohnson
 * Play skin updates
 * 
 * 32    9/03/97 9:10a Rjohnson
 * Update
 * 
 * 31    8/31/97 9:27p Rjohnson
 * GL Updates
 * 
 * 30    8/21/97 11:31a Rjohnson
 * Fix for color map
 * 
 * 29    8/20/97 10:38p Rjohnson
 * Fix for view model drawing
 * 
 * 28    8/17/97 4:14p Rjohnson
 * Fix for model lighting
 * 
 * 27    8/15/97 3:10p Rjohnson
 * Precache Update
 * 
 * 26    8/06/97 2:59p Rjohnson
 * Fix for getting the light on the gl version
 * 
 * 25    7/24/97 5:31p Rjohnson
 * Updates to the gl version for color tinting
 * 
 * 24    6/17/97 10:03a Rjohnson
 * GL Updates
 * 
 * 23    6/16/97 5:47p Bgokey
 * 
 * 22    6/16/97 4:20p Rjohnson
 * Added a sky poly count
 * 
 * 21    6/16/97 5:28a Rjohnson
 * Minor fixes
 * 
 * 20    6/14/97 2:30p Rjohnson
 * Changed the defaults of some of the console variables
 * 
 * 19    6/14/97 1:59p Rjohnson
 * Updated the fps display in the gl size, as well as meshing directories
 * 
 * 18    5/31/97 3:43p Rjohnson
 * Fix for translucent water
 * 
 * 17    6/02/97 3:42p Gmctaggart
 * GL Catchup
 * 
 * 16    5/31/97 3:43p Rjohnson
 * Drawing items in the right order
 * 
 * 15    5/31/97 11:12a Rjohnson
 * GL Updates
 * 
 * 14    5/28/97 3:54p Rjohnson
 * Effect to make a model always face you
 * 
 * 13    5/15/97 6:34p Rjohnson
 * Code Cleanup
 * 
 * 12    4/17/97 2:38p Bgokey
 * 
 * 11    4/17/97 2:30p Bgokey
 * 
 * 10    4/17/97 12:15p Rjohnson
 * Fixed some compiling problems
 * 
 * 9     4/15/97 9:02p Bgokey
 * 
 * 8     3/25/97 12:48a Bgokey
 * 
 * 7     3/22/97 5:19p Rjohnson
 * Changed the stone drawing flag to just a generic way based upon the
 * skin number
 * 
 * 6     3/22/97 3:23p Rjohnson
 * Reversed a debugging test
 * 
 * 5     3/22/97 3:22p Rjohnson
 * Added the stone texture ability for models
 * 
 * 4     3/13/97 10:54p Rjohnson
 * Added support for transparent sprites
 * 
 * 3     3/13/97 12:24p Rjohnson
 * Added the dynamic scaling of models to the gl version
 */
