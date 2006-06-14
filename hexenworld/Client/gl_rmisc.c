// r_misc.c

#include "quakedef.h"

byte			*playerTranslation;

cvar_t			gl_purge_maptex = {"gl_purge_maptex", "1", CVAR_ARCHIVE};
				// whether or not map-specific OGL textures
				// are purged on map change. default == yes

int			gl_texlevel;
extern qboolean		plyrtex[MAX_PLAYER_CLASS][16][16];
extern gltexture_t	gltextures[MAX_GLTEXTURES];
extern int		menu_numcachepics;
extern cachepic_t	menu_cachepics[MAX_CACHED_PICS];

extern void R_InitBubble (void);

/*
==================
R_InitTextures
==================
*/
void	R_InitTextures (void)
{
	int		x, y, m;
	byte	*dest;

// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName (sizeof(texture_t) + 16*16+8*8+4*4+2*2, "notexture");

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;

	for (m=0 ; m<4 ; m++)
	{
		dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}
}

#define	TEXSIZE		8

static byte dottexture[TEXSIZE][TEXSIZE] =
{
	{0,1,1,0,0,0,0,0},
	{1,1,1,1,0,0,0,0},
	{1,1,1,1,0,0,0,0},
	{0,1,1,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
};

void R_InitParticleTexture (void)
{
	int		x, y;
	byte	data[TEXSIZE][TEXSIZE][4];

	//
	// particle texture
	//
	for (x = 0 ; x < TEXSIZE ; x++)
	{
		for (y = 0 ; y < TEXSIZE ; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y]*255;
		}
	}

	particletexture = GL_LoadTexture("", TEXSIZE, TEXSIZE, (byte *)data, false, true, 0, true);
	glTexEnvf_fp(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
static void R_Envmap_f (void)
{
	byte	buffer[256*256*4];

	glDrawBuffer_fp (GL_FRONT);
	glReadBuffer_fp (GL_FRONT);
	envmap = true;

	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = 0;
	r_refdef.vrect.width = 256;
	r_refdef.vrect.height = 256;

	r_refdef.viewangles[0] = 0;
	r_refdef.viewangles[1] = 0;
	r_refdef.viewangles[2] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels_fp (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env0.rgb", buffer, sizeof(buffer));

	r_refdef.viewangles[1] = 90;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels_fp (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env1.rgb", buffer, sizeof(buffer));

	r_refdef.viewangles[1] = 180;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels_fp (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env2.rgb", buffer, sizeof(buffer));

	r_refdef.viewangles[1] = 270;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels_fp (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env3.rgb", buffer, sizeof(buffer));

	r_refdef.viewangles[0] = -90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels_fp (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env4.rgb", buffer, sizeof(buffer));

	r_refdef.viewangles[0] = 90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels_fp (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env5.rgb", buffer, sizeof(buffer));

	envmap = false;
	glDrawBuffer_fp (GL_BACK);
	glReadBuffer_fp (GL_BACK);
	GL_EndRendering ();
}

/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
static void R_TimeRefresh_f (void)
{
	int		i;
	float		start, stop, time;

	if (cls.state != ca_active)
	{
		Con_Printf("Not connected to a server\n");
		return;
	}

	start = Sys_DoubleTime ();
	for (i=0 ; i<128 ; i++)
	{
		GL_BeginRendering(&glx, &gly, &glwidth, &glheight);
		r_refdef.viewangles[1] = i/128.0*360.0;
		R_RenderView ();
		GL_EndRendering ();
	}

	glFinish_fp ();
	stop = Sys_DoubleTime ();
	time = stop-start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128/time);
}

/*
===============
R_Init
===============
*/
void R_Init (void)
{
	int	counter;

	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
	Cmd_AddCommand ("envmap", R_Envmap_f);
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);

	Cvar_RegisterVariable (&r_norefresh);
	Cvar_RegisterVariable (&r_lightmap);
	Cvar_RegisterVariable (&r_fullbright);
	Cvar_RegisterVariable (&r_drawentities);
	Cvar_RegisterVariable (&r_drawviewmodel);
	Cvar_RegisterVariable (&r_shadows);
	Cvar_RegisterVariable (&r_mirroralpha);
	Cvar_RegisterVariable (&r_wateralpha);
	Cvar_RegisterVariable (&r_skyalpha);
	Cvar_RegisterVariable (&r_dynamic);
	Cvar_RegisterVariable (&r_novis);
	Cvar_RegisterVariable (&r_speeds);
	Cvar_RegisterVariable (&r_wholeframe);

	Cvar_RegisterVariable (&r_netgraph);
	Cvar_RegisterVariable (&r_entdistance);
	Cvar_RegisterVariable (&r_teamcolor);

	Cvar_RegisterVariable (&gl_clear);
	Cvar_RegisterVariable (&gl_cull);
	Cvar_RegisterVariable (&gl_smoothmodels);
	Cvar_RegisterVariable (&gl_affinemodels);
	Cvar_RegisterVariable (&gl_polyblend);
	Cvar_RegisterVariable (&gl_flashblend);
	Cvar_RegisterVariable (&gl_playermip);
	Cvar_RegisterVariable (&gl_nocolors);
	Cvar_RegisterVariable (&gl_waterripple);
	Cvar_RegisterVariable (&gl_waterwarp);

	Cvar_RegisterVariable (&gl_ztrick);
	Cvar_RegisterVariable (&gl_purge_maptex);

	Cvar_RegisterVariable (&gl_keeptjunctions);
	Cvar_RegisterVariable (&gl_reporttjunctions);
	Cvar_RegisterVariable (&gl_multitexture);

	Cvar_RegisterVariable (&gl_glows);
	Cvar_RegisterVariable (&gl_missile_glows);
	Cvar_RegisterVariable (&gl_other_glows);
	Cvar_RegisterVariable (&gl_stencilshadow);

	Cvar_RegisterVariable (&gl_coloredlight);
	Cvar_RegisterVariable (&gl_colored_dynamic_lights);
	Cvar_RegisterVariable (&gl_extra_dynamic_lights);

	R_InitBubble();

	R_InitParticles ();
	R_InitParticleTexture ();

#ifdef GLTEST
	Test_Init ();
#endif

	netgraphtexture = texture_extension_number;
	texture_extension_number++;

	for (counter = 0 ; counter < MAX_EXTRA_TEXTURES ; counter++)
		gl_extra_textures[counter] = -1;

	playerTranslation = (byte *)COM_LoadHunkFile ("gfx/player.lmp");
	if (!playerTranslation)
		Sys_Error ("Couldn't load gfx/player.lmp");
}

const int color_offsets[MAX_PLAYER_CLASS] =
{
	2*14*256,
	0,
	1*14*256,
	2*14*256,
	2*14*256,
#if defined(H2W)
	2*14*256
#endif
};

/*
===============
R_TranslatePlayerSkin

Translates a skin texture by the per-player color lookup
===============
*/
extern	model_t	*player_models[MAX_PLAYER_CLASS];
extern	byte	player_8bit_texels[MAX_PLAYER_CLASS][620*245];

void R_TranslatePlayerSkin (int playernum)
{
	int		top, bottom;
	byte		translate[256];
	unsigned	translate32[256];
	int		i, j, s;
	model_t		*model;
	aliashdr_t	*paliashdr;
	byte		*original;
	unsigned	pixels[512*256], *out;
	unsigned	scaled_width, scaled_height;
	int		inwidth, inheight;
	byte		*inrow;
	unsigned	frac, fracstep;
	byte		*sourceA, *sourceB, *colorA, *colorB;
	player_info_t	*player;
	char		texname[20];

	for (i=0 ; i<256 ; i++)
		translate[i] = i;

	player = &cl.players[playernum];
	if (!player->name[0])
		return;
	if (!player->playerclass)
		return;

	top = player->topcolor;
	bottom = player->bottomcolor;

	if (top > 10)
		top = 0;
	if (bottom > 10)
		bottom = 0;

	top -= 1;
	bottom -= 1;

	colorA = playerTranslation + 256 + color_offsets[(int)player->playerclass-1];
	colorB = colorA + 256;
	sourceA = colorB + 256 + (top * 256);
	sourceB = colorB + 256 + (bottom * 256);
	for (i = 0 ; i < 256 ; i++, colorA++, colorB++, sourceA++, sourceB++)
	{
		if (top >= 0 && (*colorA != 255))
			translate[i] = *sourceA;
		if (bottom >= 0 && (*colorB != 255))
			translate[i] = *sourceB;
	}

	//
	// locate the original skin pixels
	//
	if (cl.players[playernum].modelindex <= 0)
		return;

	model = player_models[cl.players[playernum].playerclass-1];
	if (!model)
		return;		// player doesn't have a model yet
	paliashdr = (aliashdr_t *)Mod_Extradata (model);
	s = paliashdr->skinwidth * paliashdr->skinheight;

	if (cl.players[playernum].playerclass >= 1 && cl.players[playernum].playerclass <= MAX_PLAYER_CLASS)
	{
		original = player_8bit_texels[(int)cl.players[playernum].playerclass-1];
		cl.players[playernum].Translated = true;
	}
	else
		original = player_8bit_texels[0];

//	if (s & 3)
//		Sys_Error ("R_TranslateSkin: s&3");

#if 0
	byte	translated[320*200];

	for (i=0 ; i<s ; i+=4)
	{
		translated[i] = translate[original[i]];
		translated[i+1] = translate[original[i+1]];
		translated[i+2] = translate[original[i+2]];
		translated[i+3] = translate[original[i+3]];
	}

	// don't mipmap these, because it takes too long
	GL_Upload8 (translated, paliashdr->skinwidth, paliashdr->skinheight, false, false, true);
#else
	for (i=0 ; i<256 ; i++)
		translate32[i] = d_8to24table[translate[i]];
	scaled_width  = gl_max_size < 512 ? gl_max_size : 512;
	scaled_height = gl_max_size < 256 ? gl_max_size : 256;

	// allow users to crunch sizes down even more if they want
	scaled_width >>= (int)gl_playermip.value;
	scaled_height >>= (int)gl_playermip.value;

	inwidth = paliashdr->skinwidth;
	inheight = paliashdr->skinheight;
	out = pixels;
	fracstep = inwidth*0x10000/scaled_width;
	for (i=0 ; i<scaled_height ; i++, out += scaled_width)
	{
		inrow = original + inwidth*(i*inheight/scaled_height);
		frac = fracstep >> 1;
		for (j=0 ; j<scaled_width ; j+=4)
		{
			out[j] = translate32[inrow[frac>>16]];
			frac += fracstep;
			out[j+1] = translate32[inrow[frac>>16]];
			frac += fracstep;
			out[j+2] = translate32[inrow[frac>>16]];
			frac += fracstep;
			out[j+3] = translate32[inrow[frac>>16]];
			frac += fracstep;
		}
	}
	snprintf(texname, 19, "player%i", playernum);
	playertextures[playernum] = GL_LoadTexture(texname, scaled_width, scaled_height, (byte *)pixels, false, false, 0, true);
#endif
}

/*
===============
R_NewMap
===============
*/
void R_NewMap (void)
{
	int		i;

	for (i=0 ; i<256 ; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	r_worldentity.model = cl.worldmodel;

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
		cl.worldmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	R_ClearParticles ();

	GL_BuildLightmaps ();

	// identify sky texture
	skytexturenum = -1;
	mirrortexturenum = -1;
	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		if (!cl.worldmodel->textures[i])
			continue;
		if (!strncmp(cl.worldmodel->textures[i]->name,"sky",3) )
			skytexturenum = i;
		if (!strncmp(cl.worldmodel->textures[i]->name,"window02_1",10) )
			mirrortexturenum = i;
		cl.worldmodel->textures[i]->texturechain = NULL;
	}
#ifdef QUAKE2
	R_LoadSkys ();
#endif
}


/* D_ClearOpenGLTexture
   this procedure (called by Host_ClearMemory/SV_SpawnServer in hexen2 on new
   map, or by CL_ClearState/CL_ParseServerData in HW on new connection) will
   purge all OpenGL textures beyond static ones (console, menu, etc, whatever
   was loaded at initialization time). This will save a lot of video memory,
   because the textures won't keep accumulating from map to map, thus bloating
   more and more the more time the client is running, which gets pretty nasty
   on 8-16-32M machines with OpenGL drivers like nVidia, which cache all
   textures in system memory. (Pa3PyX)
*/
void D_ClearOpenGLTextures (int last_tex)
{
	int		i;

	// Delete OpenGL textures
	for (i = last_tex; i < numgltextures; i++)
		glDeleteTextures_fp(1, &(gltextures[i].texnum));

	memset(&(gltextures[last_tex]), 0, (numgltextures - last_tex) * sizeof(gltexture_t));
	numgltextures = last_tex;

	if (currenttexture >= last_tex)
		currenttexture = -1;

	// Clear menu pic cache
	memset(menu_cachepics, 0, menu_numcachepics * sizeof(cachepic_t));
	menu_numcachepics = 0;

	// Clear player pic cache
	memset(plyrtex, 0, MAX_PLAYER_CLASS * 16 * 16 * sizeof(qboolean));
	Con_Printf ("Purged OpenGL textures\n");
}

void D_FlushCaches (void)
{
	if (numgltextures - gl_texlevel > 0 && flush_textures && gl_purge_maptex.value)
		D_ClearOpenGLTextures (gl_texlevel);
}

