/*
	gl_main.c

	$Id: gl_rmain.c,v 1.48 2007-05-13 11:59:00 sezero Exp $
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
GLuint			playertextures[MAX_CLIENTS];	// up to MAX_CLIENTS color translated skins
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
//static float	r_base_world_matrix[16];	// for R_Mirror()

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
cvar_t	r_texture_external = {"r_texture_external", "0", CVAR_ARCHIVE};

cvar_t	r_entdistance = {"r_entdistance", "0", CVAR_ARCHIVE};
cvar_t	r_netgraph = {"r_netgraph", "0", CVAR_NONE};
cvar_t	r_teamcolor = {"r_teamcolor", "187", CVAR_ARCHIVE};

extern	cvar_t	scr_fov;

cvar_t	gl_clear = {"gl_clear", "0", CVAR_NONE};
cvar_t	gl_cull = {"gl_cull", "1", CVAR_NONE};
cvar_t	gl_ztrick = {"gl_ztrick", "0", CVAR_ARCHIVE};
cvar_t	gl_multitexture = {"gl_multitexture", "0", CVAR_ARCHIVE};
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

	for (i = 0; i < 4; i++)
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
	glRotatef_fp (e->angles[2], 1, 0, 0);	//RDM: switched sign so it matches software
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

		// For clientside rotation stuff
		glRotatef_fp (e->angleAdd[0], 0, 1, 0);
		glRotatef_fp (e->angleAdd[1], 0, 0, 1);
		glRotatef_fp (e->angleAdd[2], 1, 0, 0);
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
static mspriteframe_t *R_GetSpriteFrame (entity_t *curr_ent)
{
	msprite_t	*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int			i, numframes, frame;
	float		*pintervals, fullinterval, targettime, time;

	psprite = curr_ent->model->cache.data;
	frame = curr_ent->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_Printf ("%s: no such frame %d\n", __thisfunc__, frame);
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

		time = cl.time + curr_ent->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i = 0; i < (numframes-1); i++)
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
static void R_DrawSpriteModel (entity_t *e)
{
	vec3_t		point;
	mspriteframe_t	*frame;
	msprite_t	*psprite;
	float		*up, *right;
	vec3_t		v_forward, v_right, v_up;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache

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

	frame = R_GetSpriteFrame (e);
	psprite = currententity->model->cache.data;

	if (psprite->type == SPR_ORIENTED)
	{	// bullet marks on walls
		AngleVectors (currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else
	{	// normal sprite
		up = vup;
		right = vright;
	}

	GL_Bind(frame->gl_texturenum);

	glBegin_fp (GL_QUADS);

	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glTexCoord2f_fp (0, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv_fp (point);

	glTexCoord2f_fp (0, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv_fp (point);

	glTexCoord2f_fp (1, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv_fp (point);

	glTexCoord2f_fp (1, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv_fp (point);

	glEnd_fp ();

// restore tex parms
//	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//	glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	/* replaced GL_REPEAT with GL_CLAMP below (courtesy of Pa3PyX)
	   fixing the demoness and praevus flame's "lines" bug.  also
	   see gl_vidsdl.c and gl_vidnt.c in func: GL_Init()	- S.A */
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
	char		client_team[16], this_team[16];
	qboolean	OnTeam = false;
	int			i, my_team, ve_team;

	lastposenum = posenum;

	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	ColorShade = currententity->colorshade;

	i = currententity->scoreboard - cl.players;
	if (i >= 0 && i < MAX_CLIENTS)
	{
		my_team = cl.players[cl.playernum].siege_team;
		ve_team = cl.players[i].siege_team;
		if ((ambientlight+shadelight) > 50 || (cl_siege && my_team == ve_team))
			cl.players[i].shownames_off = false;
		else
			cl.players[i].shownames_off = true;
		if (cl_siege)
		{
			if (cl.players[cl.playernum].playerclass == CLASS_DWARF && currententity->skinnum == 101)
			{
				ColorShade = 133;
				if (ambientlight < 128)
					shadelight += (128 - ambientlight);
				cl.players[i].shownames_off = false;
			}
			else if (cl.players[cl.playernum].playerclass == CLASS_DWARF && (ambientlight+shadelight) < 51)
			{
				// OOps, use darkmaps in GL
				ColorShade = 128 + (int)((ambientlight+shadelight)/5);
				shadelight += (51 - ambientlight);
				cl.players[i].shownames_off = false;
			}
			else if (ve_team == ST_DEFENDER)
			{
				// tint gold since we can't have seperate skins
				OnTeam = true;
				ColorShade = 165;
			}
		}
		else
		{
			strncpy(client_team, Info_ValueForKey(cl.players[cl.playernum].userinfo, "team"), 16);
			client_team[15] = 0;
			if (client_team[0])
			{
				strncpy(this_team, Info_ValueForKey(cl.players[i].userinfo, "team"), 16);
				this_team[15] = 0;
				if (Q_strcasecmp(client_team, this_team) == 0)
				{
					OnTeam = true;
					ColorShade = r_teamcolor.value;
				}
			}
		}
	}

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
				glColor4f_fp (l * lightcolor[0], l * lightcolor[1], l * lightcolor[2], model_constant_alpha);
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

	if (have_stencil && gl_stencilshadow.integer)
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

	if (have_stencil && gl_stencilshadow.integer)
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
		Con_DPrintf ("%s: no such frame %d\n", __thisfunc__, frame);
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
	int		anim;
	static float	tmatrix[3][4];
	float		entScale;
	float		xyfact = 1.0, zfact = 1.0; // avoid compiler warning
	qpic_t		*stonepic;
	glpic_t		*gl;
	char		temp[80];
	int		mls;
	vec3_t		adjust_origin;

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

	if (!AlwaysDrawModel && R_CullBox (mins, maxs))
		return;

	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	mls = currententity->drawflags & MLS_MASKIN;
	if (currententity->model->flags & EF_ROTATE)
	{
		ambientlight = shadelight =
			lightcolor[0] =
			lightcolor[1] =
			lightcolor[2] =
					60+34+sin(currententity->origin[0]
						+ currententity->origin[1]
						+ (cl.time*3.8))*34;
	}
	else if (mls == MLS_ABSLIGHT)
	{
		lightcolor[0] =
		lightcolor[1] =
		lightcolor[2] =
		ambientlight =
		shadelight =
				currententity->abslight;
	}
	else if (mls != MLS_NONE)
	{
		// Use a model light style (25-30)
		lightcolor[0] =
		lightcolor[1] =
		lightcolor[2] =
		ambientlight =
		shadelight =
				d_lightstylevalue[24+mls]/2;
	}
	else if (e != &cl.viewent)	// R_DrawViewModel() already does viewmodel lighting.
	{
		// get lighting information
		VectorCopy(currententity->origin, adjust_origin);
		adjust_origin[2] += (currententity->model->mins[2] + currententity->model->maxs[2]) / 2;
		if (gl_lightmap_format == GL_RGBA)
			ambientlight = R_LightPointColor (adjust_origin);
		else
			ambientlight = shadelight = R_LightPoint (adjust_origin);

		for (lnum = 0; lnum < MAX_DLIGHTS; lnum++)
		{
			if (cl_dlights[lnum].die >= cl.time)
			{
				VectorSubtract (currententity->origin, cl_dlights[lnum].origin, dist);
				add = cl_dlights[lnum].radius - VectorLength(dist);
				if (add > 0)
				{
					ambientlight += add;
					//ZOID models should be affected by dlights as well
					shadelight += add;
					lightcolor[0] += (cl_dlights[lnum].color[0] * add);
					lightcolor[1] += (cl_dlights[lnum].color[1] * add);
					lightcolor[2] += (cl_dlights[lnum].color[2] * add);
				}
			}
		}

		// clamp lighting so it doesn't overbright as much
		if (ambientlight > 128)
			ambientlight = 128;
		if (ambientlight + shadelight > 192)
			shadelight = 192 - ambientlight;
	}

	shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	shadelight = shadelight / 200.0;
	VectorScale(lightcolor, 1.0f / 200.0f, lightcolor);

	an = e->angles[1] / 180 * M_PI;
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
		entScale = (float)currententity->scale / 100.0;
		switch (currententity->drawflags & SCALE_TYPE_MASKIN)
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

		switch (currententity->drawflags & SCALE_ORIGIN_MASKIN)
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

//	if (cl.players[currententity->scoreboard - cl.players].siege_team == ST_DEFENDER)
//		currententity->skinnum = cl.players[currententity->scoreboard - cl.players].playerclass + 110;

	if (currententity->skinnum >= 100)
	{
		if (currententity->skinnum > 255)
		{
			Sys_Error ("skinnum > 255");
		}

		if (gl_extra_textures[currententity->skinnum-100] == GL_UNUSED_TEXTURE) // Need to load it in
		{
			snprintf (temp, sizeof(temp), "gfx/skin%d.lmp", currententity->skinnum);
			stonepic = Draw_CachePic(temp);
			gl = (glpic_t *)stonepic->data;
			gl_extra_textures[currententity->skinnum-100] = gl->texnum;
		}

		GL_Bind(gl_extra_textures[currententity->skinnum-100]);
	}
	else
	{
		anim = (int)(cl.time*10) & 3;
		GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]);

		// we can't dynamically colormap textures, so they are cached
		// seperately for the players.  Heads are just uncolored.

		if (currententity->colormap != vid.colormap && !gl_nocolors.integer)
		{
		// FIXME? What about Demoness and Dwarf?
			if (currententity->model == player_models[0] ||
			    currententity->model == player_models[1] ||
			    currententity->model == player_models[2] ||
			    currententity->model == player_models[3])
			{
				i = currententity->scoreboard - cl.players;
				if (i >= 0 && i < MAX_CLIENTS)
				{
					if (!cl.players[i].Translated)
						R_TranslatePlayerSkin(i);

					GL_Bind(playertextures[i]);
				}
			}
		}
	}

	if (gl_smoothmodels.integer)
		glShadeModel_fp (GL_SMOOTH);
	glTexEnvf_fp(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.integer)
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
	if (gl_affinemodels.integer)
		glHint_fp (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glPopMatrix_fp ();

	if (r_shadows.integer)
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
	vec3_t		diff;
	int			test_length, calc_length;

	cl_numtransvisedicts = 0;
	cl_numtranswateredicts = 0;

	if (!r_drawentities.integer)
		return;

	if (r_entdistance.value <= 0)
	{
		test_length = 9999*9999;
	}
	else
	{
		test_length = r_entdistance.value * r_entdistance.value;
	}

	// draw sprites seperately, because of alpha blending
	for (i = 0; i < cl_numvisedicts; i++)
	{
		currententity = &cl_visedicts[i];

//		if (currententity->drawflags & 5) // MLS_INVIS - but dwarf can see
//			if (cl.v.playerclass != CLASS_DWARF)
//				continue;

		switch (currententity->model->type)
		{
		case mod_alias:
			VectorSubtract(currententity->origin, r_origin, diff);
			calc_length = (diff[0]*diff[0]) + (diff[1]*diff[1]) + (diff[2]*diff[2]);
			if (calc_length > test_length)
				continue;

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
			VectorSubtract(currententity->origin, r_origin, diff);
			calc_length = (diff[0]*diff[0]) + (diff[1]*diff[1]) + (diff[2]*diff[2]);
			if (calc_length > test_length)
				continue;

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

	for (i = 0; i < numents; i++)
	{
		VectorSubtract(theents[i].ent->origin, r_origin, result);
	//	theents[i].len = VectorLength(result);
		theents[i].len = (result[0] * result[0]) + (result[1] * result[1]) + (result[2] * result[2]);
	}

	qsort((void *) theents, numents, sizeof(sortedent_t), transCompare);
	// Add in BETTER sorting here

	glDepthMask_fp(0);
	for (i = 0; i < numents; i++)
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
	if ((gl_glows.integer && (clmodel->ex_flags & XF_TORCH_GLOW)) ||
	    (gl_missile_glows.integer && (clmodel->ex_flags & XF_MISSILE_GLOW)) ||
	    (gl_other_glows.integer && (clmodel->ex_flags & XF_GLOW)) )
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
			if (clmodel->flags & EF_ROTATE)
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
					j = j * 22;
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
					j = j * 22;
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
					j = j * 22;
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

			for (i = 0; i < 3; i++)
				glow_vect[i] = lightorigin[i] - vp2[i]*radius;

			glVertex3fv_fp(glow_vect);

			glColor4f_fp(0.0f, 0.0f, 0.0f, 1.0f);

			for (i = 16; i >= 0; i--)
			{
				float a = i / 16.0f * M_PI * 2;

				for (j = 0; j < 3; j++)
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

	if (!r_drawentities.integer)
		return;

	glDepthMask_fp (0);
	glDisable_fp (GL_TEXTURE_2D);
	glShadeModel_fp (GL_SMOOTH);
	glEnable_fp (GL_BLEND);
	glBlendFunc_fp (GL_ONE, GL_ONE);

	for (i = 0; i < cl_numvisedicts; i++)
	{
		currententity = &cl_visedicts[i];

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

	if (cl.spectator)
		return;

	currententity = &cl.viewent;

	if (!currententity->model)
		return;

	if (gl_lightmap_format == GL_RGBA)
	{
		ambientlight = R_LightPointColor (currententity->origin);
		if (lightcolor[0] < 24)
			lightcolor[0] = 24;
		if (lightcolor[1] < 24)
			lightcolor[1] = 24;
		if (lightcolor[2] < 24)
			lightcolor[2] = 24;
		if (ambientlight < 24)
			ambientlight = 24;		// always give some light on gun
	}
	else
	{
		ambientlight = shadelight = R_LightPoint (currententity->origin);
		if (ambientlight < 24)
			ambientlight = shadelight = 24;	// always give some light on gun
	}

// add dynamic lights
	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++)
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
				lightcolor[0] += (float) (dl->color[0] * add);
				lightcolor[1] += (float) (dl->color[1] * add);
				lightcolor[2] += (float) (dl->color[2] * add);
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
//rjr	    (cl.items & IT_INVISIBILITY) ||
	    (!r_drawviewmodel.integer) ||
	    (!r_drawentities.integer) ||
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

	if (r_oldviewleaf == r_viewleaf && !r_novis.integer)
		return;

	if (mirror)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis.integer)
	{
		vis = solid;
		memset (solid, 0xff, (cl.worldmodel->numleafs+7)>>3);
	}
	else
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

	for (i = 0; i < cl.worldmodel->numleafs; i++)
	{
		if ( vis[i>>3] & (1<<(i&7)) )
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
	if (!gl_polyblend.integer)
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
	for (j = 0; j < 3; j++)
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

	for (i = 0; i < 4; i++)
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

typedef struct _MATRIX {
	GLfloat	M[4][4];
} MATRIX;

typedef struct _point3d {
	GLfloat	x;
	GLfloat	y;
	GLfloat	z;
} POINT3D;

static MATRIX	ModelviewMatrix, ProjectionMatrix, FinalMatrix;

static void MultiplyMatrix (MATRIX *m1, MATRIX *m2, MATRIX *m3)
{
	int		i, j;

	for (j = 0; j < 4; j ++)
	{
		for (i = 0; i < 4; i ++)
		{
			m1->M[j][i] = m2->M[j][0] * m3->M[0][i] +
					m2->M[j][1] * m3->M[1][i] +
					m2->M[j][2] * m3->M[2][i] +
					m2->M[j][3] * m3->M[3][i];
		}
	}
}

static void TransformPoint (POINT3D *ptOut, POINT3D *ptIn, MATRIX *mat)
{
	double	x, y, z;

	x = (ptIn->x * mat->M[0][0]) + (ptIn->y * mat->M[0][1]) +
			(ptIn->z * mat->M[0][2]) + mat->M[0][3];

	y = (ptIn->x * mat->M[1][0]) + (ptIn->y * mat->M[1][1]) +
			(ptIn->z * mat->M[1][2]) + mat->M[1][3];

	z = (ptIn->x * mat->M[2][0]) + (ptIn->y * mat->M[2][1]) +
			(ptIn->z * mat->M[2][2]) + mat->M[2][3];

	ptOut->x = (float) x;
	ptOut->y = (float) y;
	ptOut->z = (float) z;
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
//	yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;
//	yfov = (2.0 * tan (scr_fov.value/360*M_PI)) / screenaspect;
	yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*(scr_fov.value*2)/M_PI;
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
	if (gl_cull.integer)
		glEnable_fp(GL_CULL_FACE);
	else
		glDisable_fp(GL_CULL_FACE);

	glDisable_fp(GL_BLEND);
	glDisable_fp(GL_ALPHA_TEST);
	glEnable_fp(GL_DEPTH_TEST);

	glGetFloatv_fp(GL_MODELVIEW_MATRIX, (float *)ModelviewMatrix.M);
//	ModelviewMatrix.M[0][3] = 0;
//	ModelviewMatrix.M[1][3] = 0;
//	ModelviewMatrix.M[2][3] = 0;
//	ModelviewMatrix.M[3][3] = 0;
	glGetFloatv_fp(GL_PROJECTION_MATRIX, (float *)ProjectionMatrix.M);
//	MultiplyMatrix(&FinalMatrix, &ModelviewMatrix, &ProjectionMatrix);
	MultiplyMatrix(&FinalMatrix, &ProjectionMatrix, &ModelviewMatrix);
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
		if (gl_clear.integer)
			glClear_fp (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glClear_fp (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 0.5;
		glDepthFunc_fp (GL_LEQUAL);
	}
	else if (gl_ztrick.integer)
	{
		static int trickframe;

		if (gl_clear.integer)
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
		if (gl_clear.integer)
			glClear_fp (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glClear_fp (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		glDepthFunc_fp (GL_LEQUAL);
	}

	glDepthRange_fp (gldepthmin, gldepthmax);

	if (have_stencil && gl_stencilshadow.integer && r_shadows.integer)
	{
		glClearStencil_fp(1);
		glClear_fp(GL_STENCIL_BUFFER_BIT);
	}
}


#if 0 //!!! FIXME, Zoid, mirror is disabled for now
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
	for ( ; s ; s = s->texturechain)
		R_RenderBrushPoly (s, true);
	cl.worldmodel->textures[mirrortexturenum]->texturechain = NULL;
	glDisable_fp (GL_BLEND);
	glColor4f_fp (1,1,1,1);
}
#endif


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

	ms = 1000 * (r_time2 - r_time1);
	fps = 1000 / ms;

	Con_Printf("%3.1f fps %5.0f ms\n%4i wpoly  %4i epoly  %4i(%i) edicts\n",
			fps, ms, c_brush_polys, c_alias_polys, cl_numvisedicts, cl_numtransvisedicts+cl_numtranswateredicts);
}


/*
=============
R_DrawName
=============
*/
void R_DrawName (vec3_t origin, char *Name, int Red)
{
	float	zi;
	int		u, v;
	POINT3D	In, Out;

	if (!Name)
		return;

	In.x = origin[0];
	In.y = origin[1];
	In.z = origin[2];
	TransformPoint(&Out, &In, &FinalMatrix);

	if (Out.z < 0)
		return;

	zi = 1.0 / (Out.z + 8);
	u = (int)(r_refdef.vrect.width / 2 * (zi * Out.x + 1) ) + r_refdef.vrect.x;
	v = (int)(r_refdef.vrect.height / 2 * (zi * (-Out.y) + 1) ) + r_refdef.vrect.y;

	u -= strlen(Name) * 4;

	if (cl_siege)
	{
		if (Red > 10)
		{
			Red -= 10;
			Draw_Character (u, v, 145);	//key
			u += 8;
		}
		if (Red > 0 && Red < 3)		//def
		{
			if (Red == true)
				Draw_Character (u, v, 143);	//shield
			else
				Draw_Character (u, v, 130);	//crown
			Draw_RedString (u+8, v, Name);
		}
		else if (!Red)
		{
			Draw_Character (u, v, 144);	//sword
			Draw_String (u+8, v, Name);
		}
		else
			Draw_String (u+8, v, Name);
	}
	else
	{
		Draw_String (u, v, Name);
	}
}


/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	if (r_norefresh.integer)
		return;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("%s: NULL worldmodel", __thisfunc__);

	if (r_speeds.integer)
	{
		glFinish_fp ();
		if (r_wholeframe.integer)
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
//	R_Mirror ();

	R_PolyBlend ();

	if (r_speeds.integer)
		R_PrintTimes ();
}

