/*
	cl_effect.c
	Client side effects.

	$Header: /home/ozzie/Download/0000/uhexen2/hexen2/cl_effect.c,v 1.12 2006-10-20 20:32:29 sezero Exp $
*/

// HEADER FILES ------------------------------------------------------------

#include "quakedef.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

#define MAX_EFFECT_ENTITIES		256

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void R_RunQuakeEffect (vec3_t org, float distance);
extern void RiderParticle (int count, vec3_t origin);
extern void GravityWellParticle (int count, vec3_t origin, int color);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int NewEffectEntity (void);
static void FreeEffectEntity (int idx);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static entity_t EffectEntities[MAX_EFFECT_ENTITIES];
static qboolean EntityUsed[MAX_EFFECT_ENTITIES];
static int EffectEntityCount;

// CODE --------------------------------------------------------------------


//==========================================================================
//
// CL_InitTEnts
//
//==========================================================================

void CL_InitEffects (void)
{
}

void CL_ClearEffects (void)
{
	memset(cl.Effects,0,sizeof(cl.Effects));
	memset(EntityUsed,0,sizeof(EntityUsed));
	EffectEntityCount = 0;
}

static void CL_FreeEffect (int idx)
{
	int		i;

	switch (cl.Effects[idx].type)
	{
		case CE_RAIN:
			break;

		case CE_SNOW:
			break;

		case CE_FOUNTAIN:
			break;

		case CE_QUAKE:
			break;

		case CE_WHITE_SMOKE:
		case CE_GREEN_SMOKE:
		case CE_GREY_SMOKE:
		case CE_RED_SMOKE:
		case CE_SLOW_WHITE_SMOKE:
		case CE_TELESMK1:
		case CE_TELESMK2:
		case CE_GHOST:
		case CE_REDCLOUD:
		case CE_ACID_MUZZFL:
		case CE_FLAMESTREAM:
		case CE_FLAMEWALL:
		case CE_FLAMEWALL2:
		case CE_ONFIRE:
			FreeEffectEntity(cl.Effects[idx].ef.Smoke.entity_index);
			break;

		// Just go through animation and then remove
		case CE_SM_WHITE_FLASH:
		case CE_YELLOWRED_FLASH:
		case CE_BLUESPARK:
		case CE_YELLOWSPARK:
		case CE_SM_CIRCLE_EXP:
		case CE_BG_CIRCLE_EXP:
		case CE_SM_EXPLOSION:
		case CE_LG_EXPLOSION:
		case CE_FLOOR_EXPLOSION:
		case CE_FLOOR_EXPLOSION3:
		case CE_BLUE_EXPLOSION:
		case CE_REDSPARK:
		case CE_GREENSPARK:
		case CE_ICEHIT:
		case CE_MEDUSA_HIT:
		case CE_MEZZO_REFLECT:
		case CE_FLOOR_EXPLOSION2:
		case CE_XBOW_EXPLOSION:
		case CE_NEW_EXPLOSION:
		case CE_MAGIC_MISSILE_EXPLOSION:
		case CE_BONE_EXPLOSION:
		case CE_BLDRN_EXPL:
		case CE_BRN_BOUNCE:
		case CE_LSHOCK:
		case CE_ACID_HIT:
		case CE_ACID_SPLAT:
		case CE_ACID_EXPL:
		case CE_LBALL_EXPL:
		case CE_FBOOM:
		case CE_BOMB:
		case CE_FIREWALL_SMALL:
		case CE_FIREWALL_MEDIUM:
		case CE_FIREWALL_LARGE:
			FreeEffectEntity(cl.Effects[idx].ef.Smoke.entity_index);
			break;

		// Go forward then backward through animation then remove
		case CE_WHITE_FLASH:
		case CE_BLUE_FLASH:
		case CE_SM_BLUE_FLASH:
		case CE_RED_FLASH:
			FreeEffectEntity(cl.Effects[idx].ef.Flash.entity_index);
			break;

		case CE_RIDER_DEATH:
			break;

		case CE_GRAVITYWELL:
			break;

		case CE_TELEPORTERPUFFS:
			for (i = 0 ; i < 8 ; ++i)
				FreeEffectEntity(cl.Effects[idx].ef.Teleporter.entity_index[i]);
			break;

		case CE_TELEPORTERBODY:
			FreeEffectEntity(cl.Effects[idx].ef.Teleporter.entity_index[0]);
			break;

		case CE_BONESHARD:
		case CE_BONESHRAPNEL:
			FreeEffectEntity(cl.Effects[idx].ef.Missile.entity_index);
			break;

		case CE_CHUNK:
			//Con_Printf("Freeing a chunk here\n");
			for (i = 0 ; i < cl.Effects[idx].ef.Chunk.numChunks ; i++)
			{
				if (cl.Effects[idx].ef.Chunk.entity_index[i] != -1)
					FreeEffectEntity(cl.Effects[idx].ef.Chunk.entity_index[i]);
			}
			break;
	}

	memset(&cl.Effects[idx],0,sizeof(struct EffectT));
}

//==========================================================================
//
// CL_ParseEffect
//
//==========================================================================

// All changes need to be in SV_SendEffect(), SV_ParseEffect(),
// SV_SaveEffects(), SV_LoadEffects(), CL_ParseEffect()
void CL_ParseEffect (void)
{
	int		i, idx;
	qboolean	ImmediateFree;
	entity_t	*ent;
	int		dir;
	float		angleval, sinval, cosval;
	float		skinnum;
	float		final;

	ImmediateFree = false;

	idx = MSG_ReadByte();
	if (cl.Effects[idx].type)
		CL_FreeEffect(idx);

	memset(&cl.Effects[idx],0,sizeof(struct EffectT));

	cl.Effects[idx].type = MSG_ReadByte();

	switch (cl.Effects[idx].type)
	{
		case CE_RAIN:
			cl.Effects[idx].ef.Rain.min_org[0] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.min_org[1] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.min_org[2] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.max_org[0] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.max_org[1] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.max_org[2] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.e_size[0] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.e_size[1] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.e_size[2] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.dir[0] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.dir[1] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.dir[2] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.color = MSG_ReadShort();
			cl.Effects[idx].ef.Rain.count = MSG_ReadShort();
			cl.Effects[idx].ef.Rain.wait = MSG_ReadFloat();
			break;

		case CE_SNOW:
			cl.Effects[idx].ef.Rain.min_org[0] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.min_org[1] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.min_org[2] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.max_org[0] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.max_org[1] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.max_org[2] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.flags = MSG_ReadByte();
			cl.Effects[idx].ef.Rain.dir[0] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.dir[1] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.dir[2] = MSG_ReadCoord();
			cl.Effects[idx].ef.Rain.count = MSG_ReadByte();
			//cl.Effects[idx].ef.Rain.veer = MSG_ReadShort();
			break;

		case CE_FOUNTAIN:
			cl.Effects[idx].ef.Fountain.pos[0] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Fountain.pos[1] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Fountain.pos[2] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Fountain.angle[0] = MSG_ReadAngle ();
			cl.Effects[idx].ef.Fountain.angle[1] = MSG_ReadAngle ();
			cl.Effects[idx].ef.Fountain.angle[2] = MSG_ReadAngle ();
			cl.Effects[idx].ef.Fountain.movedir[0] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Fountain.movedir[1] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Fountain.movedir[2] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Fountain.color = MSG_ReadShort ();
			cl.Effects[idx].ef.Fountain.cnt = MSG_ReadByte ();
			AngleVectors (cl.Effects[idx].ef.Fountain.angle,
					cl.Effects[idx].ef.Fountain.vforward,
					cl.Effects[idx].ef.Fountain.vright,
					cl.Effects[idx].ef.Fountain.vup);
			break;

		case CE_QUAKE:
			cl.Effects[idx].ef.Quake.origin[0] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Quake.origin[1] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Quake.origin[2] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Quake.radius = MSG_ReadFloat ();
			break;

		case CE_WHITE_SMOKE:
		case CE_GREEN_SMOKE:
		case CE_GREY_SMOKE:
		case CE_RED_SMOKE:
		case CE_SLOW_WHITE_SMOKE:
		case CE_TELESMK1:
		case CE_TELESMK2:
		case CE_GHOST:
		case CE_REDCLOUD:
		case CE_ACID_MUZZFL:
		case CE_FLAMESTREAM:
		case CE_FLAMEWALL:
		case CE_FLAMEWALL2:
		case CE_ONFIRE:
			cl.Effects[idx].ef.Smoke.origin[0] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Smoke.origin[1] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Smoke.origin[2] = MSG_ReadCoord ();

			cl.Effects[idx].ef.Smoke.velocity[0] = MSG_ReadFloat ();
			cl.Effects[idx].ef.Smoke.velocity[1] = MSG_ReadFloat ();
			cl.Effects[idx].ef.Smoke.velocity[2] = MSG_ReadFloat ();

			cl.Effects[idx].ef.Smoke.framelength = MSG_ReadFloat ();
			cl.Effects[idx].ef.Smoke.frame = MSG_ReadFloat ();

			if ((cl.Effects[idx].ef.Smoke.entity_index = NewEffectEntity()) != -1)
			{
				ent = &EffectEntities[cl.Effects[idx].ef.Smoke.entity_index];
				VectorCopy(cl.Effects[idx].ef.Smoke.origin, ent->origin);

				if ((cl.Effects[idx].type == CE_WHITE_SMOKE) ||
						(cl.Effects[idx].type == CE_SLOW_WHITE_SMOKE))
					ent->model = Mod_ForName("models/whtsmk1.spr", true);
				else if (cl.Effects[idx].type == CE_GREEN_SMOKE)
					ent->model = Mod_ForName("models/grnsmk1.spr", true);
				else if (cl.Effects[idx].type == CE_GREY_SMOKE)
					ent->model = Mod_ForName("models/grysmk1.spr", true);
				else if (cl.Effects[idx].type == CE_RED_SMOKE)
					ent->model = Mod_ForName("models/redsmk1.spr", true);
				else if (cl.Effects[idx].type == CE_TELESMK1)
					ent->model = Mod_ForName("models/telesmk1.spr", true);
				else if (cl.Effects[idx].type == CE_TELESMK2)
					ent->model = Mod_ForName("models/telesmk2.spr", true);
				else if (cl.Effects[idx].type == CE_REDCLOUD)
					ent->model = Mod_ForName("models/rcloud.spr", true);
				else if (cl.Effects[idx].type == CE_FLAMESTREAM)
					ent->model = Mod_ForName("models/flamestr.spr", true);
				else if (cl.Effects[idx].type == CE_ACID_MUZZFL)
				{
					ent->model = Mod_ForName("models/muzzle1.spr", true);
					ent->drawflags = DRF_TRANSLUCENT | MLS_ABSLIGHT;
					ent->abslight = 0.2;
				}
				else if (cl.Effects[idx].type == CE_FLAMEWALL)
					ent->model = Mod_ForName("models/firewal1.spr", true);
				else if (cl.Effects[idx].type == CE_FLAMEWALL2)
					ent->model = Mod_ForName("models/firewal2.spr", true);
				else if (cl.Effects[idx].type == CE_ONFIRE)
				{
					float rdm = rand() & 3;

					if (rdm < 1)
						ent->model = Mod_ForName("models/firewal1.spr", true);
					else if (rdm < 2)
						ent->model = Mod_ForName("models/firewal2.spr", true);
					else
						ent->model = Mod_ForName("models/firewal3.spr", true);

					ent->drawflags = DRF_TRANSLUCENT;
					ent->abslight = 1;
					ent->frame = cl.Effects[idx].ef.Smoke.frame;
				}

				if (cl.Effects[idx].type != CE_REDCLOUD &&
						cl.Effects[idx].type != CE_ACID_MUZZFL &&
						cl.Effects[idx].type != CE_FLAMEWALL)
					ent->drawflags = DRF_TRANSLUCENT;

				if (cl.Effects[idx].type == CE_FLAMESTREAM)
				{
					ent->drawflags = DRF_TRANSLUCENT | MLS_ABSLIGHT;
					ent->abslight = 1;
					ent->frame = cl.Effects[idx].ef.Smoke.frame;
				}

				if (cl.Effects[idx].type == CE_GHOST)
				{
					ent->model = Mod_ForName("models/ghost.spr", true);
					ent->drawflags = DRF_TRANSLUCENT | MLS_ABSLIGHT;
					ent->abslight = .5;
				}
			}
			else
				ImmediateFree = true;
			break;

		case CE_SM_WHITE_FLASH:
		case CE_YELLOWRED_FLASH:
		case CE_BLUESPARK:
		case CE_YELLOWSPARK:
		case CE_SM_CIRCLE_EXP:
		case CE_BG_CIRCLE_EXP:
		case CE_SM_EXPLOSION:
		case CE_LG_EXPLOSION:
		case CE_FLOOR_EXPLOSION:
		case CE_FLOOR_EXPLOSION3:
		case CE_BLUE_EXPLOSION:
		case CE_REDSPARK:
		case CE_GREENSPARK:
		case CE_ICEHIT:
		case CE_MEDUSA_HIT:
		case CE_MEZZO_REFLECT:
		case CE_FLOOR_EXPLOSION2:
		case CE_XBOW_EXPLOSION:
		case CE_NEW_EXPLOSION:
		case CE_MAGIC_MISSILE_EXPLOSION:
		case CE_BONE_EXPLOSION:
		case CE_BLDRN_EXPL:
		case CE_BRN_BOUNCE:
		case CE_LSHOCK:
		case CE_ACID_HIT:
		case CE_ACID_SPLAT:
		case CE_ACID_EXPL:
		case CE_LBALL_EXPL:
		case CE_FBOOM:
		case CE_BOMB:
		case CE_FIREWALL_SMALL:
		case CE_FIREWALL_MEDIUM:
		case CE_FIREWALL_LARGE:
			cl.Effects[idx].ef.Smoke.origin[0] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Smoke.origin[1] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Smoke.origin[2] = MSG_ReadCoord ();
			if ((cl.Effects[idx].ef.Smoke.entity_index = NewEffectEntity()) != -1)
			{
				ent = &EffectEntities[cl.Effects[idx].ef.Smoke.entity_index];
				VectorCopy(cl.Effects[idx].ef.Smoke.origin, ent->origin);

				if (cl.Effects[idx].type == CE_BLUESPARK)
					ent->model = Mod_ForName("models/bspark.spr", true);
				else if (cl.Effects[idx].type == CE_YELLOWSPARK)
					ent->model = Mod_ForName("models/spark.spr", true);
				else if (cl.Effects[idx].type == CE_SM_CIRCLE_EXP)
					ent->model = Mod_ForName("models/fcircle.spr", true);
				else if (cl.Effects[idx].type == CE_BG_CIRCLE_EXP)
					ent->model = Mod_ForName("models/xplod29.spr", true);
				else if (cl.Effects[idx].type == CE_SM_WHITE_FLASH)
					ent->model = Mod_ForName("models/sm_white.spr", true);
				else if (cl.Effects[idx].type == CE_YELLOWRED_FLASH)
				{
					ent->model = Mod_ForName("models/yr_flsh.spr", true);
					ent->drawflags = DRF_TRANSLUCENT;
				}
				else if (cl.Effects[idx].type == CE_SM_EXPLOSION)
					ent->model = Mod_ForName("models/sm_expld.spr", true);
				else if (cl.Effects[idx].type == CE_LG_EXPLOSION)
					ent->model = Mod_ForName("models/bg_expld.spr", true);
				else if (cl.Effects[idx].type == CE_FLOOR_EXPLOSION)
					ent->model = Mod_ForName("models/fl_expld.spr", true);
				else if (cl.Effects[idx].type == CE_FLOOR_EXPLOSION3)
					ent->model = Mod_ForName("models/biggy.spr", true);
				else if (cl.Effects[idx].type == CE_BLUE_EXPLOSION)
					ent->model = Mod_ForName("models/xpspblue.spr", true);
				else if (cl.Effects[idx].type == CE_REDSPARK)
					ent->model = Mod_ForName("models/rspark.spr", true);
				else if (cl.Effects[idx].type == CE_GREENSPARK)
					ent->model = Mod_ForName("models/gspark.spr", true);
				else if (cl.Effects[idx].type == CE_ICEHIT)
					ent->model = Mod_ForName("models/icehit.spr", true);
				else if (cl.Effects[idx].type == CE_MEDUSA_HIT)
					ent->model = Mod_ForName("models/medhit.spr", true);
				else if (cl.Effects[idx].type == CE_MEZZO_REFLECT)
					ent->model = Mod_ForName("models/mezzoref.spr", true);
				else if (cl.Effects[idx].type == CE_FLOOR_EXPLOSION2)
					ent->model = Mod_ForName("models/flrexpl2.spr", true);
				else if (cl.Effects[idx].type == CE_XBOW_EXPLOSION)
					ent->model = Mod_ForName("models/xbowexpl.spr", true);
				else if (cl.Effects[idx].type == CE_NEW_EXPLOSION)
					ent->model = Mod_ForName("models/gen_expl.spr", true);
				else if (cl.Effects[idx].type == CE_MAGIC_MISSILE_EXPLOSION)
					ent->model = Mod_ForName("models/mm_expld.spr", true);
				else if (cl.Effects[idx].type == CE_BONE_EXPLOSION)
					ent->model = Mod_ForName("models/bonexpld.spr", true);
				else if (cl.Effects[idx].type == CE_BLDRN_EXPL)
					ent->model = Mod_ForName("models/xplsn_1.spr", true);
				else if (cl.Effects[idx].type == CE_ACID_HIT)
					ent->model = Mod_ForName("models/axplsn_2.spr", true);
				else if (cl.Effects[idx].type == CE_ACID_SPLAT)
					ent->model = Mod_ForName("models/axplsn_1.spr", true);
				else if (cl.Effects[idx].type == CE_ACID_EXPL)
				{
					ent->model = Mod_ForName("models/axplsn_5.spr", true);
					ent->drawflags = MLS_ABSLIGHT;
					ent->abslight = 1;
				}
				else if (cl.Effects[idx].type == CE_FBOOM)
					ent->model = Mod_ForName("models/fboom.spr", true);
				else if (cl.Effects[idx].type == CE_BOMB)
					ent->model = Mod_ForName("models/pow.spr", true);
				else if (cl.Effects[idx].type == CE_LBALL_EXPL)
					ent->model = Mod_ForName("models/Bluexp3.spr", true);
				else if (cl.Effects[idx].type == CE_FIREWALL_SMALL)
					ent->model = Mod_ForName("models/firewal1.spr", true);
				else if (cl.Effects[idx].type == CE_FIREWALL_MEDIUM)
					ent->model = Mod_ForName("models/firewal5.spr", true);
				else if (cl.Effects[idx].type == CE_FIREWALL_LARGE)
					ent->model = Mod_ForName("models/firewal4.spr", true);
				else if (cl.Effects[idx].type == CE_BRN_BOUNCE)
					ent->model = Mod_ForName("models/spark.spr", true);
				else if (cl.Effects[idx].type == CE_LSHOCK)
				{
					ent->model = Mod_ForName("models/vorpshok.mdl", true);
					ent->drawflags = MLS_TORCH;
					ent->angles[2] = 90;
					ent->scale = 255;
				}
			}
			else
			{
				ImmediateFree = true;
			}
			break;

		case CE_WHITE_FLASH:
		case CE_BLUE_FLASH:
		case CE_SM_BLUE_FLASH:
		case CE_RED_FLASH:
			cl.Effects[idx].ef.Flash.origin[0] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Flash.origin[1] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Flash.origin[2] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Flash.reverse = 0;
			if ((cl.Effects[idx].ef.Flash.entity_index = NewEffectEntity()) != -1)
			{
				ent = &EffectEntities[cl.Effects[idx].ef.Flash.entity_index];
				VectorCopy(cl.Effects[idx].ef.Flash.origin, ent->origin);

				if (cl.Effects[idx].type == CE_WHITE_FLASH)
					ent->model = Mod_ForName("models/gryspt.spr", true);
				else if (cl.Effects[idx].type == CE_BLUE_FLASH)
					ent->model = Mod_ForName("models/bluflash.spr", true);
				else if (cl.Effects[idx].type == CE_SM_BLUE_FLASH)
					ent->model = Mod_ForName("models/sm_blue.spr", true);
				else if (cl.Effects[idx].type == CE_RED_FLASH)
					ent->model = Mod_ForName("models/redspt.spr", true);

				ent->drawflags = DRF_TRANSLUCENT;
			}
			else
			{
				ImmediateFree = true;
			}
			break;

		case CE_RIDER_DEATH:
			cl.Effects[idx].ef.RD.origin[0] = MSG_ReadCoord ();
			cl.Effects[idx].ef.RD.origin[1] = MSG_ReadCoord ();
			cl.Effects[idx].ef.RD.origin[2] = MSG_ReadCoord ();
			break;

		case CE_GRAVITYWELL:
			cl.Effects[idx].ef.RD.origin[0] = MSG_ReadCoord ();
			cl.Effects[idx].ef.RD.origin[1] = MSG_ReadCoord ();
			cl.Effects[idx].ef.RD.origin[2] = MSG_ReadCoord ();
			cl.Effects[idx].ef.RD.color = MSG_ReadShort ();
			cl.Effects[idx].ef.RD.lifetime = MSG_ReadFloat ();
			break;

		case CE_TELEPORTERPUFFS:
			cl.Effects[idx].ef.Teleporter.origin[0] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Teleporter.origin[1] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Teleporter.origin[2] = MSG_ReadCoord ();

			cl.Effects[idx].ef.Teleporter.framelength = .05;
			dir = 0;
			for (i = 0 ; i < 8 ; ++i)
			{
				if ((cl.Effects[idx].ef.Teleporter.entity_index[i] = NewEffectEntity()) != -1)
				{
					ent = &EffectEntities[cl.Effects[idx].ef.Teleporter.entity_index[i]];
					VectorCopy(cl.Effects[idx].ef.Teleporter.origin, ent->origin);

					angleval = dir * M_PI*2 / 360;

					sinval = sin(angleval);
					cosval = cos(angleval);

					cl.Effects[idx].ef.Teleporter.velocity[i][0] = 10*cosval;
					cl.Effects[idx].ef.Teleporter.velocity[i][1] = 10*sinval;
					cl.Effects[idx].ef.Teleporter.velocity[i][2] = 0;
					dir += 45;

					ent->model = Mod_ForName("models/telesmk2.spr", true);
					ent->drawflags = DRF_TRANSLUCENT;
				}
			}
			break;

		case CE_TELEPORTERBODY:
			cl.Effects[idx].ef.Teleporter.origin[0] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Teleporter.origin[1] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Teleporter.origin[2] = MSG_ReadCoord ();

			cl.Effects[idx].ef.Teleporter.velocity[0][0] = MSG_ReadFloat ();
			cl.Effects[idx].ef.Teleporter.velocity[0][1] = MSG_ReadFloat ();
			cl.Effects[idx].ef.Teleporter.velocity[0][2] = MSG_ReadFloat ();

			skinnum = MSG_ReadFloat ();

			cl.Effects[idx].ef.Teleporter.framelength = .05;
			dir = 0;
			if ((cl.Effects[idx].ef.Teleporter.entity_index[0] = NewEffectEntity()) != -1)
			{
				ent = &EffectEntities[cl.Effects[idx].ef.Teleporter.entity_index[0]];
				VectorCopy(cl.Effects[idx].ef.Teleporter.origin, ent->origin);

				ent->model = Mod_ForName("models/teleport.mdl", true);
				ent->drawflags = SCALE_TYPE_XYONLY | DRF_TRANSLUCENT;
				ent->scale = 100;
				ent->skinnum = skinnum;
			}
			break;

		case CE_BONESHARD:
		case CE_BONESHRAPNEL:
			cl.Effects[idx].ef.Missile.origin[0] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Missile.origin[1] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Missile.origin[2] = MSG_ReadCoord ();

			cl.Effects[idx].ef.Missile.velocity[0] = MSG_ReadFloat ();
			cl.Effects[idx].ef.Missile.velocity[1] = MSG_ReadFloat ();
			cl.Effects[idx].ef.Missile.velocity[2] = MSG_ReadFloat ();

			cl.Effects[idx].ef.Missile.angle[0] = MSG_ReadFloat ();
			cl.Effects[idx].ef.Missile.angle[1] = MSG_ReadFloat ();
			cl.Effects[idx].ef.Missile.angle[2] = MSG_ReadFloat ();

			cl.Effects[idx].ef.Missile.avelocity[0] = MSG_ReadFloat ();
			cl.Effects[idx].ef.Missile.avelocity[1] = MSG_ReadFloat ();
			cl.Effects[idx].ef.Missile.avelocity[2] = MSG_ReadFloat ();

			if ((cl.Effects[idx].ef.Missile.entity_index = NewEffectEntity()) != -1)
			{
				ent = &EffectEntities[cl.Effects[idx].ef.Missile.entity_index];
				VectorCopy(cl.Effects[idx].ef.Missile.origin, ent->origin);
				if (cl.Effects[idx].type == CE_BONESHARD)
					ent->model = Mod_ForName("models/boneshot.mdl", true);
				else if (cl.Effects[idx].type == CE_BONESHRAPNEL)
					ent->model = Mod_ForName("models/boneshrd.mdl", true);
			}
			else
				ImmediateFree = true;
			break;

		case CE_CHUNK:
			cl.Effects[idx].ef.Chunk.origin[0] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Chunk.origin[1] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Chunk.origin[2] = MSG_ReadCoord ();

			cl.Effects[idx].ef.Chunk.type = MSG_ReadByte ();

			cl.Effects[idx].ef.Chunk.srcVel[0] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Chunk.srcVel[1] = MSG_ReadCoord ();
			cl.Effects[idx].ef.Chunk.srcVel[2] = MSG_ReadCoord ();

			cl.Effects[idx].ef.Chunk.numChunks = MSG_ReadByte ();

			cl.Effects[idx].ef.Chunk.time_amount = 4.0;

			cl.Effects[idx].ef.Chunk.aveScale = 30 + 100 * (cl.Effects[idx].ef.Chunk.numChunks / 40.0);

			if (cl.Effects[idx].ef.Chunk.numChunks > 16)
				cl.Effects[idx].ef.Chunk.numChunks = 16;

			for (i = 0 ; i < cl.Effects[idx].ef.Chunk.numChunks ; i++)
			{
				if ((cl.Effects[idx].ef.Chunk.entity_index[i] = NewEffectEntity()) != -1)
				{
					ent = &EffectEntities[cl.Effects[idx].ef.Chunk.entity_index[i]];
					VectorCopy(cl.Effects[idx].ef.Chunk.origin, ent->origin);

					VectorCopy(cl.Effects[idx].ef.Chunk.srcVel, cl.Effects[idx].ef.Chunk.velocity[i]);
					VectorScale(cl.Effects[idx].ef.Chunk.velocity[i], .80 + ((rand() % 4) / 10.0), cl.Effects[idx].ef.Chunk.velocity[i]);
					// temp modify them...
					cl.Effects[idx].ef.Chunk.velocity[i][0] += (rand() % 140) - 70;
					cl.Effects[idx].ef.Chunk.velocity[i][1] += (rand() % 140) - 70;
					cl.Effects[idx].ef.Chunk.velocity[i][2] += (rand() % 140) - 70;

					// are these in degrees or radians?
					ent->angles[0] = rand() % 360;
					ent->angles[1] = rand() % 360;
					ent->angles[2] = rand() % 360;

					ent->scale = cl.Effects[idx].ef.Chunk.aveScale + (rand() % 40);

					// make this overcomplicated
					final = (rand() % 100) * .01;
					if ((cl.Effects[idx].ef.Chunk.type == THINGTYPE_GLASS) ||
						(cl.Effects[idx].ef.Chunk.type == THINGTYPE_REDGLASS) ||
						(cl.Effects[idx].ef.Chunk.type == THINGTYPE_CLEARGLASS) ||
						(cl.Effects[idx].ef.Chunk.type == THINGTYPE_WEBS))
					{
						if (final < 0.20)
							ent->model = Mod_ForName ("models/shard1.mdl", true);
						else if (final < 0.40)
							ent->model = Mod_ForName ("models/shard2.mdl", true);
						else if (final < 0.60)
							ent->model = Mod_ForName ("models/shard3.mdl", true);
						else if (final < 0.80)
							ent->model = Mod_ForName ("models/shard4.mdl", true);
						else
							ent->model = Mod_ForName ("models/shard5.mdl", true);

						if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_CLEARGLASS)
						{
							ent->skinnum = 1;
							ent->drawflags |= DRF_TRANSLUCENT;
						}
						else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_REDGLASS)
						{
							ent->skinnum = 2;
						}
						else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_WEBS)
						{
							ent->skinnum = 3;
							ent->drawflags |= DRF_TRANSLUCENT;
						}
					}
					else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_WOOD)
					{
						if (final < 0.25)
							ent->model = Mod_ForName ("models/splnter1.mdl", true);
						else if (final < 0.50)
							ent->model = Mod_ForName ("models/splnter2.mdl", true);
						else if (final < 0.75)
							ent->model = Mod_ForName ("models/splnter3.mdl", true);
						else
							ent->model = Mod_ForName ("models/splnter4.mdl", true);
					}
					else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_METAL)
					{
						if (final < 0.25)
							ent->model = Mod_ForName ("models/metlchk1.mdl", true);
						else if (final < 0.50)
							ent->model = Mod_ForName ("models/metlchk2.mdl", true);
						else if (final < 0.75)
							ent->model = Mod_ForName ("models/metlchk3.mdl", true);
						else
							ent->model = Mod_ForName ("models/metlchk4.mdl", true);
					}
					else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_FLESH)
					{
						if (final < 0.33)
							ent->model = Mod_ForName ("models/flesh1.mdl", true);
						else if (final < 0.66)
							ent->model = Mod_ForName ("models/flesh2.mdl", true);
						else
							ent->model = Mod_ForName ("models/flesh3.mdl", true);
					}
					else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_BROWNSTONE)
					{
						if (final < 0.25)
							ent->model = Mod_ForName ("models/schunk1.mdl", true);
						else if (final < 0.50)
							ent->model = Mod_ForName ("models/schunk2.mdl", true);
						else if (final < 0.75)
							ent->model = Mod_ForName ("models/schunk3.mdl", true);
						else
							ent->model = Mod_ForName ("models/schunk4.mdl", true);
						ent->skinnum = 1;
					}
					else if ((cl.Effects[idx].ef.Chunk.type == THINGTYPE_CLAY) ||
							(cl.Effects[idx].ef.Chunk.type == THINGTYPE_BONE))
					{
						if (final < 0.25)
							ent->model = Mod_ForName ("models/clshard1.mdl", true);
						else if (final < 0.50)
							ent->model = Mod_ForName ("models/clshard2.mdl", true);
						else if (final < 0.75)
							ent->model = Mod_ForName ("models/clshard3.mdl", true);
						else
							ent->model = Mod_ForName ("models/clshard4.mdl", true);
						if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_BONE)
						{
							ent->skinnum = 1;	// bone skin is second
						}
					}
					else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_LEAVES)
					{
						if (final < 0.33)
							ent->model = Mod_ForName ("models/leafchk1.mdl", true);
						else if (final < 0.66)
							ent->model = Mod_ForName ("models/leafchk2.mdl", true);
						else
							ent->model = Mod_ForName ("models/leafchk3.mdl", true);
					}
					else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_HAY)
					{
						if (final < 0.33)
							ent->model = Mod_ForName ("models/hay1.mdl", true);
						else if (final < 0.66)
							ent->model = Mod_ForName ("models/hay2.mdl", true);
						else
							ent->model = Mod_ForName ("models/hay3.mdl", true);
					}
					else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_CLOTH)
					{
						if (final < 0.33)
							ent->model = Mod_ForName ("models/clthchk1.mdl", true);
						else if (final < 0.66)
							ent->model = Mod_ForName ("models/clthchk2.mdl", true);
						else
							ent->model = Mod_ForName ("models/clthchk3.mdl", true);
					}
					else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_WOOD_LEAF)
					{
						if (final < 0.14)
							ent->model = Mod_ForName ("models/splnter1.mdl", true);
						else if (final < 0.28)
							ent->model = Mod_ForName ("models/leafchk1.mdl", true);
						else if (final < 0.42)
							ent->model = Mod_ForName ("models/splnter2.mdl", true);
						else if (final < 0.56)
							ent->model = Mod_ForName ("models/leafchk2.mdl", true);
						else if (final < 0.70)
							ent->model = Mod_ForName ("models/splnter3.mdl", true);
						else if (final < 0.84)
							ent->model = Mod_ForName ("models/leafchk3.mdl", true);
						else
							ent->model = Mod_ForName ("models/splnter4.mdl", true);
					}
					else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_WOOD_METAL)
					{
						if (final < 0.125)
							ent->model = Mod_ForName ("models/splnter1.mdl", true);
						else if (final < 0.25)
							ent->model = Mod_ForName ("models/metlchk1.mdl", true);
						else if (final < 0.375)
							ent->model = Mod_ForName ("models/splnter2.mdl", true);
						else if (final < 0.50)
							ent->model = Mod_ForName ("models/metlchk2.mdl", true);
						else if (final < 0.625)
							ent->model = Mod_ForName ("models/splnter3.mdl", true);
						else if (final < 0.75)
							ent->model = Mod_ForName ("models/metlchk3.mdl", true);
						else if (final < 0.875)
							ent->model = Mod_ForName ("models/splnter4.mdl", true);
						else
							ent->model = Mod_ForName ("models/metlchk4.mdl", true);
					}
					else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_WOOD_STONE)
					{
						if (final < 0.125)
							ent->model = Mod_ForName ("models/splnter1.mdl", true);
						else if (final < 0.25)
							ent->model = Mod_ForName ("models/schunk1.mdl", true);
						else if (final < 0.375)
							ent->model = Mod_ForName ("models/splnter2.mdl", true);
						else if (final < 0.50)
							ent->model = Mod_ForName ("models/schunk2.mdl", true);
						else if (final < 0.625)
							ent->model = Mod_ForName ("models/splnter3.mdl", true);
						else if (final < 0.75)
							ent->model = Mod_ForName ("models/schunk3.mdl", true);
						else if (final < 0.875)
							ent->model = Mod_ForName ("models/splnter4.mdl", true);
						else
							ent->model = Mod_ForName ("models/schunk4.mdl", true);
					}
					else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_METAL_STONE)
					{
						if (final < 0.125)
							ent->model = Mod_ForName ("models/metlchk1.mdl", true);
						else if (final < 0.25)
							ent->model = Mod_ForName ("models/schunk1.mdl", true);
						else if (final < 0.375)
							ent->model = Mod_ForName ("models/metlchk2.mdl", true);
						else if (final < 0.50)
							ent->model = Mod_ForName ("models/schunk2.mdl", true);
						else if (final < 0.625)
							ent->model = Mod_ForName ("models/metlchk3.mdl", true);
						else if (final < 0.75)
							ent->model = Mod_ForName ("models/schunk3.mdl", true);
						else if (final < 0.875)
							ent->model = Mod_ForName ("models/metlchk4.mdl", true);
						else
							ent->model = Mod_ForName ("models/schunk4.mdl", true);
					}
					else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_METAL_CLOTH)
					{
						if (final < 0.14)
							ent->model = Mod_ForName ("models/metlchk1.mdl", true);
						else if (final < 0.28)
							ent->model = Mod_ForName ("models/clthchk1.mdl", true);
						else if (final < 0.42)
							ent->model = Mod_ForName ("models/metlchk2.mdl", true);
						else if (final < 0.56)
							ent->model = Mod_ForName ("models/clthchk2.mdl", true);
						else if (final < 0.70)
							ent->model = Mod_ForName ("models/metlchk3.mdl", true);
						else if (final < 0.84)
							ent->model = Mod_ForName ("models/clthchk3.mdl", true);
						else
							ent->model = Mod_ForName ("models/metlchk4.mdl", true);
					}
					else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_ICE)
					{
						ent->model = Mod_ForName("models/shard.mdl", true);
						ent->skinnum = 0;
						ent->frame = rand() % 2;
						ent->drawflags |= DRF_TRANSLUCENT|MLS_ABSLIGHT;
						ent->abslight = 0.5;
					}
					else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_METEOR)
					{
						ent->model = Mod_ForName("models/tempmetr.mdl", true);
						ent->skinnum = 0;
					}
					else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_ACID)
					{	// no spinning if possible...
						ent->model = Mod_ForName("models/sucwp2p.mdl", true);
						ent->skinnum = 0;
					}
					else if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_GREENFLESH)
					{	// spider guts
						if (final < 0.33)
							ent->model = Mod_ForName ("models/sflesh1.mdl", true);
						else if (final < 0.66)
							ent->model = Mod_ForName ("models/sflesh2.mdl", true);
						else
							ent->model = Mod_ForName ("models/sflesh3.mdl", true);

						ent->skinnum = 0;
					}
					else// if (cl.Effects[idx].ef.Chunk.type == THINGTYPE_GREYSTONE)
					{
						if (final < 0.25)
							ent->model = Mod_ForName ("models/schunk1.mdl", true);
						else if (final < 0.50)
							ent->model = Mod_ForName ("models/schunk2.mdl", true);
						else if (final < 0.75)
							ent->model = Mod_ForName ("models/schunk3.mdl", true);
						else
							ent->model = Mod_ForName ("models/schunk4.mdl", true);
						ent->skinnum = 0;
					}
				}
			}

			for (i = 0 ; i < 3 ; i++)
			{
				cl.Effects[idx].ef.Chunk.avel[i][0] = (rand() % 850) - 425;
				cl.Effects[idx].ef.Chunk.avel[i][1] = (rand() % 850) - 425;
				cl.Effects[idx].ef.Chunk.avel[i][2] = (rand() % 850) - 425;
			}

			break;

		default:
			Sys_Error ("CL_ParseEffect: bad type");
	}

	if (ImmediateFree)
	{
		cl.Effects[idx].type = CE_NONE;
	}
}

void CL_EndEffect (void)
{
	int		idx;

	idx = MSG_ReadByte();

	CL_FreeEffect(idx);
}

static void CL_LinkEntity (entity_t *ent)
{
	if (cl_numvisedicts < MAX_VISEDICTS)
	{
		cl_visedicts[cl_numvisedicts++] = ent;
	}
}

void CL_UpdateEffects (void)
{
	int		i;
	int		idx, cur_frame;
	vec3_t		mymin, mymax;
	float		frametime;
//	edict_t		test;
//	trace_t		trace;
	vec3_t		org, org2, alldir, snow_org;
	int		x_dir, y_dir;
	entity_t	*ent;
	float		distance, smoketime;

	if (cls.state == ca_disconnected)
		return;

	frametime = cl.time - cl.oldtime;
	if (!frametime)
		return;
//	Con_Printf("Here at %f\n",cl.time);

	for (idx = 0 ; idx < MAX_EFFECTS ; idx++)
	{
		if (!cl.Effects[idx].type)
			continue;

		switch (cl.Effects[idx].type)
		{
			case CE_RAIN:
				org[0] = cl.Effects[idx].ef.Rain.min_org[0];
				org[1] = cl.Effects[idx].ef.Rain.min_org[1];
				org[2] = cl.Effects[idx].ef.Rain.max_org[2];

				org2[0] = cl.Effects[idx].ef.Rain.e_size[0];
				org2[1] = cl.Effects[idx].ef.Rain.e_size[1];
				org2[2] = cl.Effects[idx].ef.Rain.e_size[2];

				x_dir = cl.Effects[idx].ef.Rain.dir[0];
				y_dir = cl.Effects[idx].ef.Rain.dir[1];

				cl.Effects[idx].ef.Rain.next_time += frametime;
				if (cl.Effects[idx].ef.Rain.next_time >= cl.Effects[idx].ef.Rain.wait)
				{
					R_RainEffect(org, org2, x_dir, y_dir, cl.Effects[idx].ef.Rain.color,
								cl.Effects[idx].ef.Rain.count);
					cl.Effects[idx].ef.Rain.next_time = 0;
				}
				break;

			case CE_SNOW:
				VectorCopy(cl.Effects[idx].ef.Rain.min_org,org);
				VectorCopy(cl.Effects[idx].ef.Rain.max_org,org2);
				VectorCopy(cl.Effects[idx].ef.Rain.dir,alldir);

				VectorAdd(org, org2, snow_org);

				snow_org[0] *= 0.5;
				snow_org[1] *= 0.5;
				snow_org[2] *= 0.5;

				snow_org[2] = r_origin[2];

				VectorSubtract(snow_org, r_origin, snow_org);

				distance = VectorNormalize(snow_org);

				cl.Effects[idx].ef.Rain.next_time += frametime;
				//jfm:  fixme, check distance to player first
				if (cl.Effects[idx].ef.Rain.next_time >= 0.10 && distance < 1024)
				{
					R_SnowEffect(org, org2, cl.Effects[idx].ef.Rain.flags, alldir,
								cl.Effects[idx].ef.Rain.count);

					cl.Effects[idx].ef.Rain.next_time = 0;
				}
				break;

			case CE_FOUNTAIN:
				mymin[0] = (-3 * cl.Effects[idx].ef.Fountain.vright[0] * cl.Effects[idx].ef.Fountain.movedir[0]) +
						(-3 * cl.Effects[idx].ef.Fountain.vforward[0] * cl.Effects[idx].ef.Fountain.movedir[1]) +
						(2 * cl.Effects[idx].ef.Fountain.vup[0] * cl.Effects[idx].ef.Fountain.movedir[2]);
				mymin[1] = (-3 * cl.Effects[idx].ef.Fountain.vright[1] * cl.Effects[idx].ef.Fountain.movedir[0]) +
						(-3 * cl.Effects[idx].ef.Fountain.vforward[1] * cl.Effects[idx].ef.Fountain.movedir[1]) +
						(2 * cl.Effects[idx].ef.Fountain.vup[1] * cl.Effects[idx].ef.Fountain.movedir[2]);
				mymin[2] = (-3 * cl.Effects[idx].ef.Fountain.vright[2] * cl.Effects[idx].ef.Fountain.movedir[0]) +
						(-3 * cl.Effects[idx].ef.Fountain.vforward[2] * cl.Effects[idx].ef.Fountain.movedir[1]) +
						(2 * cl.Effects[idx].ef.Fountain.vup[2] * cl.Effects[idx].ef.Fountain.movedir[2]);
				mymin[0] *= 15;
				mymin[1] *= 15;
				mymin[2] *= 15;

				mymax[0] = (3 * cl.Effects[idx].ef.Fountain.vright[0] * cl.Effects[idx].ef.Fountain.movedir[0]) +
						(3 * cl.Effects[idx].ef.Fountain.vforward[0] * cl.Effects[idx].ef.Fountain.movedir[1]) +
						(10 * cl.Effects[idx].ef.Fountain.vup[0] * cl.Effects[idx].ef.Fountain.movedir[2]);
				mymax[1] = (3 * cl.Effects[idx].ef.Fountain.vright[1] * cl.Effects[idx].ef.Fountain.movedir[0]) +
						(3 * cl.Effects[idx].ef.Fountain.vforward[1] * cl.Effects[idx].ef.Fountain.movedir[1]) +
						(10 * cl.Effects[idx].ef.Fountain.vup[1] * cl.Effects[idx].ef.Fountain.movedir[2]);
				mymax[2] = (3 * cl.Effects[idx].ef.Fountain.vright[2] * cl.Effects[idx].ef.Fountain.movedir[0]) +
						(3 * cl.Effects[idx].ef.Fountain.vforward[2] * cl.Effects[idx].ef.Fountain.movedir[1]) +
						(10 * cl.Effects[idx].ef.Fountain.vup[2] * cl.Effects[idx].ef.Fountain.movedir[2]);
				mymax[0] *= 15;
				mymax[1] *= 15;
				mymax[2] *= 15;

				R_RunParticleEffect2 (cl.Effects[idx].ef.Fountain.pos, mymin, mymax,
							cl.Effects[idx].ef.Fountain.color, 2, cl.Effects[idx].ef.Fountain.cnt);

			/*	memset(&test,0,sizeof(test));
				trace = SV_Move (cl.Effects[idx].ef.Fountain.pos, mymin, mymax, mymin, false, &test);
				Con_Printf("Fraction is %f\n",trace.fraction);
			*/
				break;

			case CE_QUAKE:
				R_RunQuakeEffect (cl.Effects[idx].ef.Quake.origin, cl.Effects[idx].ef.Quake.radius);
				break;

			case CE_WHITE_SMOKE:
			case CE_GREEN_SMOKE:
			case CE_GREY_SMOKE:
			case CE_RED_SMOKE:
			case CE_SLOW_WHITE_SMOKE:
			case CE_TELESMK1:
			case CE_TELESMK2:
			case CE_GHOST:
			case CE_REDCLOUD:
			case CE_FLAMESTREAM:
			case CE_ACID_MUZZFL:
			case CE_FLAMEWALL:
			case CE_FLAMEWALL2:
			case CE_ONFIRE:
				cl.Effects[idx].ef.Smoke.time_amount += frametime;
				ent = &EffectEntities[cl.Effects[idx].ef.Smoke.entity_index];

				smoketime = cl.Effects[idx].ef.Smoke.framelength;
				if (!smoketime)
					smoketime = HX_FRAME_TIME;

				ent->origin[0] += (frametime/smoketime) * cl.Effects[idx].ef.Smoke.velocity[0];
				ent->origin[1] += (frametime/smoketime) * cl.Effects[idx].ef.Smoke.velocity[1];
				ent->origin[2] += (frametime/smoketime) * cl.Effects[idx].ef.Smoke.velocity[2];

				while (cl.Effects[idx].ef.Smoke.time_amount >= smoketime)
				{
					ent->frame++;
					cl.Effects[idx].ef.Smoke.time_amount -= smoketime;
				}

				if (ent->frame >= ent->model->numframes)
				{
					CL_FreeEffect(idx);
				}
				else
					CL_LinkEntity(ent);

				break;

			// Just go through animation and then remove
			case CE_SM_WHITE_FLASH:
			case CE_YELLOWRED_FLASH:
			case CE_BLUESPARK:
			case CE_YELLOWSPARK:
			case CE_SM_CIRCLE_EXP:
			case CE_BG_CIRCLE_EXP:
			case CE_SM_EXPLOSION:
			case CE_LG_EXPLOSION:
			case CE_FLOOR_EXPLOSION:
			case CE_FLOOR_EXPLOSION3:
			case CE_BLUE_EXPLOSION:
			case CE_REDSPARK:
			case CE_GREENSPARK:
			case CE_ICEHIT:
			case CE_MEDUSA_HIT:
			case CE_MEZZO_REFLECT:
			case CE_FLOOR_EXPLOSION2:
			case CE_XBOW_EXPLOSION:
			case CE_NEW_EXPLOSION:
			case CE_MAGIC_MISSILE_EXPLOSION:
			case CE_BONE_EXPLOSION:
			case CE_BLDRN_EXPL:
			case CE_BRN_BOUNCE:
			case CE_ACID_HIT:
			case CE_ACID_SPLAT:
			case CE_ACID_EXPL:
			case CE_LBALL_EXPL:
			case CE_FBOOM:
			case CE_BOMB:
			case CE_FIREWALL_SMALL:
			case CE_FIREWALL_MEDIUM:
			case CE_FIREWALL_LARGE:
				cl.Effects[idx].ef.Smoke.time_amount += frametime;
				ent = &EffectEntities[cl.Effects[idx].ef.Smoke.entity_index];

				if (cl.Effects[idx].type != CE_BG_CIRCLE_EXP)
				{
					while (cl.Effects[idx].ef.Smoke.time_amount >= HX_FRAME_TIME)
					{
						ent->frame++;
						cl.Effects[idx].ef.Smoke.time_amount -= HX_FRAME_TIME;
					}
				}
				else
				{
					while (cl.Effects[idx].ef.Smoke.time_amount >= HX_FRAME_TIME * 2)
					{
						ent->frame++;
						cl.Effects[idx].ef.Smoke.time_amount -= HX_FRAME_TIME * 2;
					}
				}
				if (ent->frame >= ent->model->numframes)
				{
					CL_FreeEffect(idx);
				}
				else
					CL_LinkEntity(ent);
				break;

			case CE_LSHOCK:
				ent = &EffectEntities[cl.Effects[idx].ef.Smoke.entity_index];
				if (ent->skinnum == 0)
					ent->skinnum = 1;
				else if (ent->skinnum == 1)
					ent->skinnum = 0;
				ent->scale -= 10;
				if (ent->scale <= 10)
				{
					CL_FreeEffect(idx);
				}
				else
					CL_LinkEntity(ent);
				break;

			// Go forward then backward through animation then remove
			case CE_WHITE_FLASH:
			case CE_BLUE_FLASH:
			case CE_SM_BLUE_FLASH:
			case CE_RED_FLASH:
				cl.Effects[idx].ef.Flash.time_amount += frametime;
				ent = &EffectEntities[cl.Effects[idx].ef.Flash.entity_index];

				while (cl.Effects[idx].ef.Flash.time_amount >= HX_FRAME_TIME)
				{
					if (!cl.Effects[idx].ef.Flash.reverse)
					{
						if (ent->frame >= ent->model->numframes-1)
						{
						// Ran through forward animation
							cl.Effects[idx].ef.Flash.reverse = 1;
							ent->frame--;
						}
						else
							ent->frame++;
					}
					else
						ent->frame--;

					cl.Effects[idx].ef.Flash.time_amount -= HX_FRAME_TIME;
				}

				if ((ent->frame <= 0) && (cl.Effects[idx].ef.Flash.reverse))
				{
					CL_FreeEffect(idx);
				}
				else
					CL_LinkEntity(ent);
				break;

			case CE_RIDER_DEATH:
				cl.Effects[idx].ef.RD.time_amount += frametime;
				if (cl.Effects[idx].ef.RD.time_amount >= 1)
				{
					cl.Effects[idx].ef.RD.stage++;
					cl.Effects[idx].ef.RD.time_amount -= 1;
				}

				VectorCopy(cl.Effects[idx].ef.RD.origin,org);
				org[0] += sin(cl.Effects[idx].ef.RD.time_amount * 2 * M_PI) * 30;
				org[1] += cos(cl.Effects[idx].ef.RD.time_amount * 2 * M_PI) * 30;

				if (cl.Effects[idx].ef.RD.stage <= 6)
				//	RiderParticle(cl.Effects[idx].ef.RD.stage+1, cl.Effects[idx].ef.RD.origin);
					RiderParticle(cl.Effects[idx].ef.RD.stage+1, org);
				else
				{
					// To set the rider's origin point for the particles
					RiderParticle(0, org);
					if (cl.Effects[idx].ef.RD.stage == 7)
					{
						cl.cshifts[CSHIFT_BONUS].destcolor[0] = 255;
						cl.cshifts[CSHIFT_BONUS].destcolor[1] = 255;
						cl.cshifts[CSHIFT_BONUS].destcolor[2] = 255;
						cl.cshifts[CSHIFT_BONUS].percent = 256;
					}
					else if (cl.Effects[idx].ef.RD.stage > 13)
					{
					//	cl.Effects[idx].ef.RD.stage = 0;
						CL_FreeEffect(idx);
					}
				}
				break;

			case CE_GRAVITYWELL:
				cl.Effects[idx].ef.RD.time_amount += frametime*2;
				if (cl.Effects[idx].ef.RD.time_amount >= 1)
					cl.Effects[idx].ef.RD.time_amount -= 1;

				VectorCopy(cl.Effects[idx].ef.RD.origin,org);
				org[0] += sin(cl.Effects[idx].ef.RD.time_amount * 2 * M_PI) * 30;
				org[1] += cos(cl.Effects[idx].ef.RD.time_amount * 2 * M_PI) * 30;

				if (cl.Effects[idx].ef.RD.lifetime < cl.time)
				{
					CL_FreeEffect(idx);
				}
				else
					GravityWellParticle(rand() % 8, org, cl.Effects[idx].ef.RD.color);

				break;

			case CE_TELEPORTERPUFFS:
				cl.Effects[idx].ef.Teleporter.time_amount += frametime;
				smoketime = cl.Effects[idx].ef.Teleporter.framelength;

				ent = &EffectEntities[cl.Effects[idx].ef.Teleporter.entity_index[0]];
				while (cl.Effects[idx].ef.Teleporter.time_amount >= HX_FRAME_TIME)
				{
					ent->frame++;
					cl.Effects[idx].ef.Teleporter.time_amount -= HX_FRAME_TIME;
				}
				cur_frame = ent->frame;

				if (cur_frame >= ent->model->numframes)
				{
					CL_FreeEffect(idx);
					break;
				}

				for (i = 0 ; i < 8 ; ++i)
				{
					ent = &EffectEntities[cl.Effects[idx].ef.Teleporter.entity_index[i]];

					ent->origin[0] += (frametime/smoketime) * cl.Effects[idx].ef.Teleporter.velocity[i][0];
					ent->origin[1] += (frametime/smoketime) * cl.Effects[idx].ef.Teleporter.velocity[i][1];
					ent->origin[2] += (frametime/smoketime) * cl.Effects[idx].ef.Teleporter.velocity[i][2];
					ent->frame = cur_frame;

					CL_LinkEntity(ent);
				}
				break;

			case CE_TELEPORTERBODY:
				cl.Effects[idx].ef.Teleporter.time_amount += frametime;
				smoketime = cl.Effects[idx].ef.Teleporter.framelength;

				ent = &EffectEntities[cl.Effects[idx].ef.Teleporter.entity_index[0]];
				while (cl.Effects[idx].ef.Teleporter.time_amount >= HX_FRAME_TIME)
				{
					ent->scale -= 15;
					cl.Effects[idx].ef.Teleporter.time_amount -= HX_FRAME_TIME;
				}

				ent = &EffectEntities[cl.Effects[idx].ef.Teleporter.entity_index[0]];
				ent->angles[1] += 45;

				if (ent->scale <= 10)
				{
					CL_FreeEffect(idx);
				}
				else
				{
					CL_LinkEntity(ent);
				}
				break;

			case CE_BONESHARD:
			case CE_BONESHRAPNEL:
				cl.Effects[idx].ef.Missile.time_amount += frametime;
				ent = &EffectEntities[cl.Effects[idx].ef.Missile.entity_index];

			//	ent->angles[0] = cl.Effects[idx].ef.Missile.angle[0];
			//	ent->angles[1] = cl.Effects[idx].ef.Missile.angle[1];
			//	ent->angles[2] = cl.Effects[idx].ef.Missile.angle[2];

				ent->angles[0] += frametime * cl.Effects[idx].ef.Missile.avelocity[0];
				ent->angles[1] += frametime * cl.Effects[idx].ef.Missile.avelocity[1];
				ent->angles[2] += frametime * cl.Effects[idx].ef.Missile.avelocity[2];

				ent->origin[0] += frametime * cl.Effects[idx].ef.Missile.velocity[0];
				ent->origin[1] += frametime * cl.Effects[idx].ef.Missile.velocity[1];
				ent->origin[2] += frametime * cl.Effects[idx].ef.Missile.velocity[2];

				CL_LinkEntity(ent);
				break;

			case CE_CHUNK:
				cl.Effects[idx].ef.Chunk.time_amount -= frametime;
				if (cl.Effects[idx].ef.Chunk.time_amount < 0)
				{
					CL_FreeEffect(idx);
				}
				else
				{
					for (i = 0 ; i < cl.Effects[idx].ef.Chunk.numChunks ; i++)
					{
						vec3_t		oldorg;
						mleaf_t		*l;
						int		moving = 1;

						ent = &EffectEntities[cl.Effects[idx].ef.Chunk.entity_index[i]];

						VectorCopy(ent->origin, oldorg);

						ent->origin[0] += frametime * cl.Effects[idx].ef.Chunk.velocity[i][0];
						ent->origin[1] += frametime * cl.Effects[idx].ef.Chunk.velocity[i][1];
						ent->origin[2] += frametime * cl.Effects[idx].ef.Chunk.velocity[i][2];

						l = Mod_PointInLeaf (ent->origin, cl.worldmodel);
						if (l->contents != CONTENTS_EMPTY) //|| in_solid == true
						{	// bouncing prolly won't work...
							VectorCopy(oldorg, ent->origin);

							cl.Effects[idx].ef.Chunk.velocity[i][0] = 0;
							cl.Effects[idx].ef.Chunk.velocity[i][1] = 0;
							cl.Effects[idx].ef.Chunk.velocity[i][2] = 0;

							moving = 0;
						}
						else
						{
							ent->angles[0] += frametime * cl.Effects[idx].ef.Chunk.avel[i%3][0];
							ent->angles[1] += frametime * cl.Effects[idx].ef.Chunk.avel[i%3][1];
							ent->angles[2] += frametime * cl.Effects[idx].ef.Chunk.avel[i%3][2];
						}

						if (cl.Effects[idx].ef.Chunk.time_amount < frametime * 3)
						{	// chunk leaves in 3 frames
							ent->scale *= .7;
						}

						CL_LinkEntity(ent);

						cl.Effects[idx].ef.Chunk.velocity[i][2] -= frametime * 500; // apply gravity

						switch (cl.Effects[idx].ef.Chunk.type)
						{
						case THINGTYPE_GREYSTONE:
							break;
						case THINGTYPE_WOOD:
							break;
						case THINGTYPE_METAL:
							break;
						case THINGTYPE_FLESH:
							if (moving)
								R_RocketTrail (oldorg, ent->origin, 17);
							break;
						case THINGTYPE_FIRE:
							break;
						case THINGTYPE_CLAY:
						case THINGTYPE_BONE:
							break;
						case THINGTYPE_LEAVES:
							break;
						case THINGTYPE_HAY:
							break;
						case THINGTYPE_BROWNSTONE:
							break;
						case THINGTYPE_CLOTH:
							break;
						case THINGTYPE_WOOD_LEAF:
							break;
						case THINGTYPE_WOOD_METAL:
							break;
						case THINGTYPE_WOOD_STONE:
							break;
						case THINGTYPE_METAL_STONE:
							break;
						case THINGTYPE_METAL_CLOTH:
							break;
						case THINGTYPE_WEBS:
							break;
						case THINGTYPE_GLASS:
							break;
						case THINGTYPE_ICE:
							if (moving)
								R_RocketTrail (oldorg, ent->origin, rt_ice);
							break;
						case THINGTYPE_CLEARGLASS:
							break;
						case THINGTYPE_REDGLASS:
							break;
						case THINGTYPE_ACID:
							if (moving)
								R_RocketTrail (oldorg, ent->origin, rt_acidball);
							break;
						case THINGTYPE_METEOR:
							R_RocketTrail (oldorg, ent->origin, 1);
							break;
						case THINGTYPE_GREENFLESH:
							if (moving)
								R_RocketTrail (oldorg, ent->origin, rt_acidball);
							break;
						}
					}
				}
				break;
		}
	}
}

//==========================================================================
//
// NewEffectEntity
//
//==========================================================================

static int NewEffectEntity (void)
{
	entity_t	*ent;
	int		counter;

	if (cl_numvisedicts == MAX_VISEDICTS)
		return -1;

	if (EffectEntityCount == MAX_EFFECT_ENTITIES)
		return -1;

	for (counter = 0 ; counter < MAX_EFFECT_ENTITIES ; counter++)
		if (!EntityUsed[counter])
			break;

	EntityUsed[counter] = true;
	EffectEntityCount++;
	ent = &EffectEntities[counter];
	memset(ent, 0, sizeof(*ent));
	ent->colormap = vid.colormap;

	return counter;
}

static void FreeEffectEntity (int idx)
{
	EntityUsed[idx] = false;
	EffectEntityCount--;
}


/*
 * $Log: not supported by cvs2svn $
 * Revision 1.11  2006/06/24 14:12:59  sezero
 * separated cl_effect.c and sv_effect.c for easier maintainance
 *
 * Revision 1.10  2006/04/06 08:36:23  sezero
 * more tidy-ups (cl_effect.h, cl_effect.c, sv_effect.c)
 *
 * Revision 1.9  2005/11/05 20:22:10  sezero
 * fixed some gcc4 warnings
 *
 * Revision 1.8  2005/10/27 06:47:12  sezero
 * coding style and whitespace cleanup.
 *
 * Revision 1.7  2005/10/25 20:04:17  sezero
 * static functions part-1: started making local functions static,
 * killing nested externs, const vars clean-up.
 *
 * Revision 1.6  2005/10/02 15:43:08  sezero
 * killed -Wshadow warnings
 *
 * Revision 1.5  2005/01/01 21:43:47  sezero
 * prototypes clean-up
 *
 * Revision 1.4  2004/12/18 14:08:07  sezero
 * Clean-up and kill warnings 9:
 * Kill many unused vars.
 *
 * Revision 1.3  2004/12/18 13:59:25  sezero
 * Clean-up and kill warnings 8:
 * Missing prototypes.
 *
 * Revision 1.2  2004/12/12 14:14:42  sezero
 * style changes to our liking
 *
 * Revision 1.1.1.1  2004/11/28 00:00:56  sezero
 * Initial import of AoT 1.2.0 code
 *
 * 41    3/20/98 12:55p Jmonroe
 * made sbar arti safe, made linkedict safe, removed pent hull for NON MP
 * maps.
 * 
 * 40    3/19/98 12:23a Mgummelt
 * 
 * 39    3/13/98 4:00p Jmonroe
 * 
 * 38    3/13/98 3:35p Jweier
 * 
 * 37    3/05/98 7:54p Jmonroe
 * fixed startRain, optimized particle struct
 * 
 * 36    3/02/98 12:41p Mgummelt
 * 
 * 35    2/23/98 4:48p Jmonroe
 * fixed snow to only spew at 10 fps instead of infinity
 * 
 * 34    2/23/98 2:00p Jweier
 * 
 * 33    2/20/98 1:00p Jmonroe
 * added thingtype_bone
 * 
 * 32    2/12/98 4:35p Jweier
 * 
 * 31    2/12/98 10:38a Jweier
 * 
 * 30    2/10/98 10:51a Jweier
 * 
 * 29    2/08/98 6:47p Jweier
 * 
 * 28    2/07/98 2:11p Mgummelt
 * 
 * 27    2/07/98 1:58p Mgummelt
 * 
 * 26    2/07/98 1:28p Jweier
 * 
 * 25    2/06/98 5:06p Nalbury
 * Added more chunk stuff.
 * 
 * 24    2/06/98 4:18p Mgummelt
 * 
 * 23    2/06/98 3:47p Mgummelt
 * 
 * 22    2/06/98 3:38p Nalbury
 * Added particles
 * 
 * 21    2/06/98 12:15p Jweier
 * 
 * 20    2/06/98 11:30a Mgummelt
 * 
 * 19    2/06/98 5:03a Nalbury
 * Put in preliminary chunks for network games...
 * 
 * 18    2/05/98 11:25p Jweier
 * 
 * 17    2/05/98 6:43p Mgummelt
 * 
 * 16    2/05/98 3:35p Jweier
 * 
 * 15    2/05/98 11:08a Jweier
 * 
 * 14    2/04/98 6:31p Mgummelt
 * 
 * 13    2/04/98 4:58p Mgummelt
 * spawnflags on monsters cleared out
 * 
 * 12    2/04/98 1:45p Jweier
 * 
 * 11    2/04/98 1:36p Mgummelt
 * 
 * 10    2/03/98 4:17p Mgummelt
 * 
 * 9     2/03/98 1:44p Mgummelt
 * 
 * 8     1/26/98 4:38p Plipo
 * added pt_gravwell client effect
 * 
 * 7     1/20/98 11:09a Jmonroe
 * fix for bad model shit because of the remaining particles in restarts
 * 
 * 6     1/18/98 8:05p Jmonroe
 * all of rick's patch code is in now
 * 
 * 32    10/13/97 10:28a Rlove
 * Made the bone shard powerup more network friendly.
 * 
 * 31    10/10/97 9:13a Rlove
 * Moved bone shard shrapnel to client side effect
 * 
 * 30    10/08/97 11:14a Rlove
 * 
 * 29    10/06/97 12:19p Rjohnson
 * Fixed a name
 * 
 * 28    10/06/97 12:14p Rlove
 * 
 * 27    10/06/97 11:41a Rjohnson
 * Partically fixed teleporter effect
 * 
 * 26    9/25/97 2:31p Rlove
 * 
 * 25    9/04/97 4:44p Rjohnson
 * Updates
 * 
 * 24    8/22/97 7:25a Rlove
 * Added red cloud temp effect
 * 
 * 23    8/07/97 9:00a Rlove
 * 
 * 22    8/06/97 3:46p Rjohnson
 * Rider death update
 * 
 * 21    8/04/97 4:44p Rjohnson
 * Rider's Death doesn't repeat
 * 
 * 20    7/31/97 2:49p Rlove
 * Added a few new effect entities
 * 
 * 19    7/25/97 3:12p Rlove
 * 
 * 18    7/25/97 3:05p Rlove
 * 
 * 17    7/24/97 3:31p Rlove
 * 
 * 16    7/24/97 3:28p Rlove
 * 
 * 15    7/01/97 4:09p Rjohnson
 * Saving / Loading of client effects
 * 
 * 14    6/30/97 10:29a Rlove
 * Added wait to rain entity
 * 
 * 13    6/13/97 11:42a Rlove
 * 
 * 12    6/12/97 12:13p Rlove
 * Added red and green sparks
 * 
 * 11    6/12/97 6:42a Rlove
 * Smoke entities are being deleted (before they weren't)
 * 
 * 10    6/04/97 9:59a Rjohnson
 * Added flash effect for rider's death
 * 
 * 9     6/03/97 10:26a Rlove
 * 
 * 8     6/03/97 9:00a Rlove
 * Added fx_smoke_generator entity
 * 
 * 7     5/30/97 12:01p Rlove
 * New blue explosion
 * 
 * 6     5/30/97 11:42a Rjohnson
 * Removed the message field of the effects
 * 
 * 5     5/28/97 10:45a Rlove
 * Moved sprite effects to client side - smoke, explosions, and flashes.
 * 
 * 4     5/27/97 4:46p Rjohnson
 * Added the smoke puff effect
 * 
 * 3     5/23/97 3:05p Rjohnson
 * Update to effects / particle types
 * 
 * 2     5/20/97 11:32a Rjohnson
 * Revised Effects
 * 
 * 1     5/19/97 2:54p Rjohnson
 * Initial Version
 */
