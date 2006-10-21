/*
	sv_effect.c
	Client side effects.

	$Header: /home/ozzie/Download/0000/uhexen2/hexen2/sv_effect.c,v 1.3 2006-10-21 18:21:28 sezero Exp $
*/

// HEADER FILES ------------------------------------------------------------

#include "quakedef.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

#define MAX_EFFECT_ENTITIES		256

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern cvar_t sv_ce_scale;
extern cvar_t sv_ce_max_size;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------


//==========================================================================
//
// CL_InitTEnts
//
//==========================================================================

static void SV_ClearEffects (void)
{
	memset(sv.Effects,0,sizeof(sv.Effects));
}

// All changes need to be in SV_SendEffect(), SV_ParseEffect(),
// SV_SaveEffects(), SV_LoadEffects(), CL_ParseEffect()
static void SV_SendEffect (sizebuf_t *sb, int idx)
{
	qboolean	DoTest;
	vec3_t		TestO1, Diff;
	float		Size, TestDistance;
	int		i, count;

	if (sb == &sv.reliable_datagram && sv_ce_scale.value > 0)
		DoTest = true;
	else
		DoTest = false;

	VectorClear(TestO1);
	TestDistance = 0;

	switch (sv.Effects[idx].type)
	{
		case CE_RAIN:
		case CE_SNOW:
			DoTest = false;
			break;

		case CE_FOUNTAIN:
			DoTest = false;
			break;

		case CE_QUAKE:
			VectorCopy(sv.Effects[idx].ef.Quake.origin, TestO1);
			TestDistance = 700;
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
			VectorCopy(sv.Effects[idx].ef.Smoke.origin, TestO1);
			TestDistance = 250;
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
		case CE_ACID_HIT:
		case CE_LBALL_EXPL:
		case CE_FIREWALL_SMALL:
		case CE_FIREWALL_MEDIUM:
		case CE_FIREWALL_LARGE:
		case CE_ACID_SPLAT:
		case CE_ACID_EXPL:
		case CE_FBOOM:
		case CE_BRN_BOUNCE:
		case CE_LSHOCK:
		case CE_BOMB:
		case CE_FLOOR_EXPLOSION3:
			VectorCopy(sv.Effects[idx].ef.Smoke.origin, TestO1);
			TestDistance = 250;
			break;

		case CE_WHITE_FLASH:
		case CE_BLUE_FLASH:
		case CE_SM_BLUE_FLASH:
		case CE_RED_FLASH:
			VectorCopy(sv.Effects[idx].ef.Smoke.origin, TestO1);
			TestDistance = 250;
			break;

		case CE_RIDER_DEATH:
			DoTest = false;
			break;

		case CE_GRAVITYWELL:
			DoTest = false;
			break;

		case CE_TELEPORTERPUFFS:
			VectorCopy(sv.Effects[idx].ef.Teleporter.origin, TestO1);
			TestDistance = 350;
			break;

		case CE_TELEPORTERBODY:
			VectorCopy(sv.Effects[idx].ef.Teleporter.origin, TestO1);
			TestDistance = 350;
			break;

		case CE_BONESHARD:
		case CE_BONESHRAPNEL:
			VectorCopy(sv.Effects[idx].ef.Missile.origin, TestO1);
			TestDistance = 600;
			break;

		case CE_CHUNK:
			VectorCopy(sv.Effects[idx].ef.Chunk.origin, TestO1);
			TestDistance = 600;
			break;

		default:
		//	Sys_Error ("SV_SendEffect: bad type");
			PR_RunError ("SV_SendEffect: bad type");
			break;
	}

	if (!DoTest)
		count = 1;
	else
	{
		count = svs.maxclients;
		TestDistance = (float)TestDistance * sv_ce_scale.value;
		TestDistance *= TestDistance;
	}

	for (i = 0 ; i < count ; i++)
	{
		if (DoTest)
		{
			if (svs.clients[i].active)
			{
				sb = &svs.clients[i].datagram;
				VectorSubtract(svs.clients[i].edict->v.origin,TestO1,Diff);
				Size = (Diff[0]*Diff[0]) + (Diff[1]*Diff[1]) + (Diff[2]*Diff[2]);

				if (Size > TestDistance)
					continue;

				if (sv_ce_max_size.value > 0 && sb->cursize > sv_ce_max_size.value)
					continue;
			}
			else
				continue;
		}

		MSG_WriteByte (sb, svc_start_effect);
		MSG_WriteByte (sb, idx);
		MSG_WriteByte (sb, sv.Effects[idx].type);

		switch (sv.Effects[idx].type)
		{
		case CE_RAIN:
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.min_org[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.min_org[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.min_org[2]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.max_org[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.max_org[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.max_org[2]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.e_size[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.e_size[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.e_size[2]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.dir[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.dir[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.dir[2]);
			MSG_WriteShort(sb, sv.Effects[idx].ef.Rain.color);
			MSG_WriteShort(sb, sv.Effects[idx].ef.Rain.count);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Rain.wait);
			break;

		case CE_SNOW:
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.min_org[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.min_org[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.min_org[2]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.max_org[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.max_org[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.max_org[2]);
			MSG_WriteByte(sb, sv.Effects[idx].ef.Rain.flags);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.dir[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.dir[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Rain.dir[2]);
			MSG_WriteByte(sb, sv.Effects[idx].ef.Rain.count);
			//MSG_WriteShort(sb, sv.Effects[idx].ef.Rain.veer);
			break;

		case CE_FOUNTAIN:
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Fountain.pos[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Fountain.pos[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Fountain.pos[2]);
			MSG_WriteAngle(sb, sv.Effects[idx].ef.Fountain.angle[0]);
			MSG_WriteAngle(sb, sv.Effects[idx].ef.Fountain.angle[1]);
			MSG_WriteAngle(sb, sv.Effects[idx].ef.Fountain.angle[2]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Fountain.movedir[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Fountain.movedir[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Fountain.movedir[2]);
			MSG_WriteShort(sb, sv.Effects[idx].ef.Fountain.color);
			MSG_WriteByte(sb, sv.Effects[idx].ef.Fountain.cnt);
			break;

		case CE_QUAKE:
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Quake.origin[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Quake.origin[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Quake.origin[2]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Quake.radius);
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
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Smoke.origin[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Smoke.origin[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Smoke.origin[2]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Smoke.velocity[0]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Smoke.velocity[1]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Smoke.velocity[2]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Smoke.framelength);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Smoke.frame);
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
		case CE_ACID_HIT:
		case CE_ACID_SPLAT:
		case CE_ACID_EXPL:
		case CE_LBALL_EXPL:
		case CE_FIREWALL_SMALL:
		case CE_FIREWALL_MEDIUM:
		case CE_FIREWALL_LARGE:
		case CE_FBOOM:
		case CE_BOMB:
		case CE_BRN_BOUNCE:
		case CE_LSHOCK:
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Smoke.origin[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Smoke.origin[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Smoke.origin[2]);
			break;

		case CE_WHITE_FLASH:
		case CE_BLUE_FLASH:
		case CE_SM_BLUE_FLASH:
		case CE_RED_FLASH:
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Smoke.origin[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Smoke.origin[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Smoke.origin[2]);
			break;

		case CE_RIDER_DEATH:
			MSG_WriteCoord(sb, sv.Effects[idx].ef.RD.origin[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.RD.origin[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.RD.origin[2]);
			break;

		case CE_TELEPORTERPUFFS:
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Teleporter.origin[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Teleporter.origin[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Teleporter.origin[2]);
			break;

		case CE_TELEPORTERBODY:
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Teleporter.origin[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Teleporter.origin[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Teleporter.origin[2]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Teleporter.velocity[0][0]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Teleporter.velocity[0][1]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Teleporter.velocity[0][2]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Teleporter.skinnum);
			break;

		case CE_BONESHARD:
		case CE_BONESHRAPNEL:
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Missile.origin[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Missile.origin[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Missile.origin[2]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Missile.velocity[0]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Missile.velocity[1]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Missile.velocity[2]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Missile.angle[0]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Missile.angle[1]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Missile.angle[2]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Missile.avelocity[0]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Missile.avelocity[1]);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.Missile.avelocity[2]);
			break;

		case CE_GRAVITYWELL:
			MSG_WriteCoord(sb, sv.Effects[idx].ef.RD.origin[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.RD.origin[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.RD.origin[2]);
			MSG_WriteShort(sb, sv.Effects[idx].ef.RD.color);
			MSG_WriteFloat(sb, sv.Effects[idx].ef.RD.lifetime);
			break;

		case CE_CHUNK:
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Chunk.origin[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Chunk.origin[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Chunk.origin[2]);
			MSG_WriteByte (sb, sv.Effects[idx].ef.Chunk.type);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Chunk.srcVel[0]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Chunk.srcVel[1]);
			MSG_WriteCoord(sb, sv.Effects[idx].ef.Chunk.srcVel[2]);
			MSG_WriteByte (sb, sv.Effects[idx].ef.Chunk.numChunks);
		//	Con_Printf ("Adding %d chunks on server...\n", sv.Effects[idx].Chunk.numChunks);
			break;

		default:
		//	Sys_Error ("SV_SendEffect: bad type");
			PR_RunError ("SV_SendEffect: bad type");
			break;
		}
	}
}

void SV_UpdateEffects (sizebuf_t *sb)
{
	int		idx;

	for (idx = 0 ; idx < MAX_EFFECTS ; idx++)
	{
		if (sv.Effects[idx].type)
			SV_SendEffect(sb,idx);
	}
}

// All changes need to be in SV_SendEffect(), SV_ParseEffect(),
// SV_SaveEffects(), SV_LoadEffects(), CL_ParseEffect()
void SV_ParseEffect (sizebuf_t *sb)
{
	int		idx;
	byte		effect;

	effect = G_FLOAT(OFS_PARM0);

	for (idx = 0 ; idx < MAX_EFFECTS ; idx++)
	{
		if (!sv.Effects[idx].type || 
				(sv.Effects[idx].expire_time && sv.Effects[idx].expire_time <= sv.time))
			break;
	}

	if (idx >= MAX_EFFECTS)
	{
		PR_RunError ("MAX_EFFECTS reached");
		return;
	}

//	Con_Printf("Effect #%d\n", idx);

	memset(&sv.Effects[idx],0,sizeof(struct EffectT));

	sv.Effects[idx].type = effect;
	G_FLOAT(OFS_RETURN) = idx;

	switch (effect)
	{
		case CE_RAIN:
			VectorCopy(G_VECTOR(OFS_PARM1),sv.Effects[idx].ef.Rain.min_org);
			VectorCopy(G_VECTOR(OFS_PARM2),sv.Effects[idx].ef.Rain.max_org);
			VectorCopy(G_VECTOR(OFS_PARM3),sv.Effects[idx].ef.Rain.e_size);
			VectorCopy(G_VECTOR(OFS_PARM4),sv.Effects[idx].ef.Rain.dir);
			sv.Effects[idx].ef.Rain.color = G_FLOAT(OFS_PARM5);
			sv.Effects[idx].ef.Rain.count = G_FLOAT(OFS_PARM6);
			sv.Effects[idx].ef.Rain.wait = G_FLOAT(OFS_PARM7);

			sv.Effects[idx].ef.Rain.next_time = 0;
			break;

		case CE_SNOW:
			VectorCopy(G_VECTOR(OFS_PARM1),sv.Effects[idx].ef.Rain.min_org);
			VectorCopy(G_VECTOR(OFS_PARM2),sv.Effects[idx].ef.Rain.max_org);
			sv.Effects[idx].ef.Rain.flags = G_FLOAT(OFS_PARM3);
			VectorCopy(G_VECTOR(OFS_PARM4),sv.Effects[idx].ef.Rain.dir);
			sv.Effects[idx].ef.Rain.count = G_FLOAT(OFS_PARM5);
			//sv.Effects[idx].Rain.veer = G_FLOAT(OFS_PARM6);
			//sv.Effects[idx].Rain.wait = 0.10;

			sv.Effects[idx].ef.Rain.next_time = 0;
			break;

		case CE_FOUNTAIN:
			VectorCopy(G_VECTOR(OFS_PARM1),sv.Effects[idx].ef.Fountain.pos);
			VectorCopy(G_VECTOR(OFS_PARM2),sv.Effects[idx].ef.Fountain.angle);
			VectorCopy(G_VECTOR(OFS_PARM3),sv.Effects[idx].ef.Fountain.movedir);
			sv.Effects[idx].ef.Fountain.color = G_FLOAT(OFS_PARM4);
			sv.Effects[idx].ef.Fountain.cnt = G_FLOAT(OFS_PARM5);
			break;

		case CE_QUAKE:
			VectorCopy(G_VECTOR(OFS_PARM1), sv.Effects[idx].ef.Quake.origin);
			sv.Effects[idx].ef.Quake.radius = G_FLOAT(OFS_PARM2);
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
			VectorCopy(G_VECTOR(OFS_PARM1), sv.Effects[idx].ef.Smoke.origin);
			VectorCopy(G_VECTOR(OFS_PARM2), sv.Effects[idx].ef.Smoke.velocity);
			sv.Effects[idx].ef.Smoke.framelength = G_FLOAT(OFS_PARM3);
			sv.Effects[idx].ef.Smoke.frame = 0;
			sv.Effects[idx].expire_time = sv.time + 1;
			break;

		case CE_ACID_MUZZFL:
		case CE_FLAMESTREAM:
		case CE_FLAMEWALL:
		case CE_FLAMEWALL2:
		case CE_ONFIRE:
			VectorCopy(G_VECTOR(OFS_PARM1), sv.Effects[idx].ef.Smoke.origin);
			VectorCopy(G_VECTOR(OFS_PARM2), sv.Effects[idx].ef.Smoke.velocity);
			sv.Effects[idx].ef.Smoke.framelength = 0.05;
			sv.Effects[idx].ef.Smoke.frame = G_FLOAT(OFS_PARM3);
			sv.Effects[idx].expire_time = sv.time + 1;
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
		case CE_ACID_HIT:
		case CE_ACID_SPLAT:
		case CE_ACID_EXPL:
		case CE_LBALL_EXPL:
		case CE_FIREWALL_SMALL:
		case CE_FIREWALL_MEDIUM:
		case CE_FIREWALL_LARGE:
		case CE_FBOOM:
		case CE_BOMB:
		case CE_BRN_BOUNCE:
		case CE_LSHOCK:
			VectorCopy(G_VECTOR(OFS_PARM1), sv.Effects[idx].ef.Smoke.origin);
			sv.Effects[idx].expire_time = sv.time + 1;
			break;

		case CE_WHITE_FLASH:
		case CE_BLUE_FLASH:
		case CE_SM_BLUE_FLASH:
		case CE_RED_FLASH:
			VectorCopy(G_VECTOR(OFS_PARM1), sv.Effects[idx].ef.Flash.origin);
			sv.Effects[idx].expire_time = sv.time + 1;
			break;

		case CE_RIDER_DEATH:
			VectorCopy(G_VECTOR(OFS_PARM1), sv.Effects[idx].ef.RD.origin);
			break;

		case CE_GRAVITYWELL:
			VectorCopy(G_VECTOR(OFS_PARM1), sv.Effects[idx].ef.RD.origin);
			sv.Effects[idx].ef.RD.color = G_FLOAT(OFS_PARM2);
			sv.Effects[idx].ef.RD.lifetime = G_FLOAT(OFS_PARM3);
			break;

		case CE_TELEPORTERPUFFS:
			VectorCopy(G_VECTOR(OFS_PARM1), sv.Effects[idx].ef.Teleporter.origin);
			sv.Effects[idx].expire_time = sv.time + 1;
			break;

		case CE_TELEPORTERBODY:
			VectorCopy(G_VECTOR(OFS_PARM1), sv.Effects[idx].ef.Teleporter.origin);
			VectorCopy(G_VECTOR(OFS_PARM2),sv.Effects[idx].ef.Teleporter.velocity[0]);
			sv.Effects[idx].ef.Teleporter.skinnum = G_FLOAT(OFS_PARM3);
			sv.Effects[idx].expire_time = sv.time + 1;
			break;

		case CE_BONESHARD:
		case CE_BONESHRAPNEL:
			VectorCopy(G_VECTOR(OFS_PARM1), sv.Effects[idx].ef.Missile.origin);
			VectorCopy(G_VECTOR(OFS_PARM2),sv.Effects[idx].ef.Missile.velocity);
			VectorCopy(G_VECTOR(OFS_PARM3),sv.Effects[idx].ef.Missile.angle);
			VectorCopy(G_VECTOR(OFS_PARM2),sv.Effects[idx].ef.Missile.avelocity);

			sv.Effects[idx].expire_time = sv.time + 10;
			break;

		case CE_CHUNK:
			VectorCopy(G_VECTOR(OFS_PARM1),sv.Effects[idx].ef.Chunk.origin);
			sv.Effects[idx].ef.Chunk.type = G_FLOAT(OFS_PARM2);
			VectorCopy(G_VECTOR(OFS_PARM3),sv.Effects[idx].ef.Chunk.srcVel);
			sv.Effects[idx].ef.Chunk.numChunks = G_FLOAT(OFS_PARM4);

			sv.Effects[idx].expire_time = sv.time + 3;
			break;

		default:
		//	Sys_Error ("SV_ParseEffect: bad type");
			PR_RunError ("SV_SendEffect: bad type");
	}

	SV_SendEffect(sb,idx);
}

// All changes need to be in SV_SendEffect(), SV_ParseEffect(),
// SV_SaveEffects(), SV_LoadEffects(), CL_ParseEffect()
void SV_SaveEffects (FILE *FH)
{
	int	idx, count;

	for (idx = count = 0 ; idx < MAX_EFFECTS ; idx++)
	{
		if (sv.Effects[idx].type)
			count++;
	}

	fprintf(FH, "Effects: %d\n", count);

	for (idx = count = 0 ; idx < MAX_EFFECTS ; idx++)
	{
		if ( ! sv.Effects[idx].type )
			continue;

		fprintf(FH, "Effect: %d %d %f: ", idx, sv.Effects[idx].type, sv.Effects[idx].expire_time);

		switch (sv.Effects[idx].type)
		{
			case CE_RAIN:
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.min_org[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.min_org[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.min_org[2]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.max_org[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.max_org[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.max_org[2]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.e_size[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.e_size[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.e_size[2]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.dir[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.dir[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.dir[2]);
				fprintf(FH, "%d ", sv.Effects[idx].ef.Rain.color);
				fprintf(FH, "%d ", sv.Effects[idx].ef.Rain.count);
				fprintf(FH, "%f\n", sv.Effects[idx].ef.Rain.wait);
				break;

			case CE_SNOW:
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.min_org[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.min_org[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.min_org[2]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.max_org[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.max_org[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.max_org[2]);
				fprintf(FH, "%d ", sv.Effects[idx].ef.Rain.flags);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.dir[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.dir[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Rain.dir[2]);
				fprintf(FH, "%d ", sv.Effects[idx].ef.Rain.count);
				//fprintf(FH, "%d ", sv.Effects[idx].ef.Rain.veer);

				// O.S:	a linefeed is missing here. not adding
				//	it so as not to break existing saves.
				//	also see in: SV_LoadEffects().
				break;

			case CE_FOUNTAIN:
				fprintf(FH, "%f ", sv.Effects[idx].ef.Fountain.pos[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Fountain.pos[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Fountain.pos[2]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Fountain.angle[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Fountain.angle[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Fountain.angle[2]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Fountain.movedir[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Fountain.movedir[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Fountain.movedir[2]);
				fprintf(FH, "%d ", sv.Effects[idx].ef.Fountain.color);
				fprintf(FH, "%d\n", sv.Effects[idx].ef.Fountain.cnt);
				break;

			case CE_QUAKE:
				fprintf(FH, "%f ", sv.Effects[idx].ef.Quake.origin[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Quake.origin[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Quake.origin[2]);
				fprintf(FH, "%f\n", sv.Effects[idx].ef.Quake.radius);
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
				fprintf(FH, "%f ", sv.Effects[idx].ef.Smoke.origin[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Smoke.origin[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Smoke.origin[2]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Smoke.velocity[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Smoke.velocity[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Smoke.velocity[2]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Smoke.framelength);
				fprintf(FH, "%f\n", sv.Effects[idx].ef.Smoke.frame);
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
			case CE_FIREWALL_SMALL:
			case CE_FIREWALL_MEDIUM:
			case CE_FIREWALL_LARGE:
			case CE_FBOOM:
			case CE_BOMB:
				fprintf(FH, "%f ", sv.Effects[idx].ef.Smoke.origin[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Smoke.origin[1]);
				fprintf(FH, "%f\n", sv.Effects[idx].ef.Smoke.origin[2]);
				break;

			case CE_WHITE_FLASH:
			case CE_BLUE_FLASH:
			case CE_SM_BLUE_FLASH:
			case CE_RED_FLASH:
				fprintf(FH, "%f ", sv.Effects[idx].ef.Flash.origin[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Flash.origin[1]);
				fprintf(FH, "%f\n", sv.Effects[idx].ef.Flash.origin[2]);
				break;

			case CE_RIDER_DEATH:
				fprintf(FH, "%f ", sv.Effects[idx].ef.RD.origin[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.RD.origin[1]);
				fprintf(FH, "%f\n", sv.Effects[idx].ef.RD.origin[2]);
				break;

			case CE_GRAVITYWELL:
				fprintf(FH, "%f ", sv.Effects[idx].ef.RD.origin[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.RD.origin[1]);
				fprintf(FH, "%f", sv.Effects[idx].ef.RD.origin[2]);
				fprintf(FH, "%d", sv.Effects[idx].ef.RD.color);
				fprintf(FH, "%f\n", sv.Effects[idx].ef.RD.lifetime);
				break;

			case CE_TELEPORTERPUFFS:
				fprintf(FH, "%f ", sv.Effects[idx].ef.Teleporter.origin[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Teleporter.origin[1]);
				fprintf(FH, "%f\n", sv.Effects[idx].ef.Teleporter.origin[2]);
				break;

			case CE_TELEPORTERBODY:
				fprintf(FH, "%f ", sv.Effects[idx].ef.Teleporter.origin[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Teleporter.origin[1]);
				fprintf(FH, "%f\n", sv.Effects[idx].ef.Teleporter.origin[2]);
				break;

			case CE_BONESHARD:
			case CE_BONESHRAPNEL:
				fprintf(FH, "%f ", sv.Effects[idx].ef.Missile.origin[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Missile.origin[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Missile.origin[2]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Missile.velocity[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Missile.velocity[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Missile.velocity[2]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Missile.angle[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Missile.angle[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Missile.angle[2]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Missile.avelocity[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Missile.avelocity[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Missile.avelocity[2]);

				// O.S:	a linefeed is missing here. not adding
				//	it so as not to break existing saves.
				//	also see in: SV_LoadEffects().
				break;

			case CE_CHUNK:
				fprintf(FH, "%f ", sv.Effects[idx].ef.Chunk.origin[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Chunk.origin[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Chunk.origin[2]);
				fprintf(FH, "%d ", sv.Effects[idx].ef.Chunk.type);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Chunk.srcVel[0]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Chunk.srcVel[1]);
				fprintf(FH, "%f ", sv.Effects[idx].ef.Chunk.srcVel[2]);
				fprintf(FH, "%d ", sv.Effects[idx].ef.Chunk.numChunks);

				// O.S:	a linefeed is missing here. not adding
				//	it so as not to break existing saves.
				//	also see in: SV_LoadEffects().
				break;

			default:
				PR_RunError ("SV_SaveEffect: bad type");
				break;
		}
	}
}

// All changes need to be in SV_SendEffect(), SV_ParseEffect(),
// SV_SaveEffects(), SV_LoadEffects(), CL_ParseEffect()
void SV_LoadEffects (FILE *FH)
{
	int		idx, Total, count;

	// Since the map is freshly loaded, clear out any effects as a result of
	// the loading
	SV_ClearEffects();

	fscanf(FH, "Effects: %d\n", &Total);

	for (count = 0 ; count < Total ; count++)
	{
		fscanf(FH, "Effect: %d ", &idx);
		fscanf(FH, "%d %f: ", &sv.Effects[idx].type, &sv.Effects[idx].expire_time);

		switch (sv.Effects[idx].type)
		{
			case CE_RAIN:
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.min_org[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.min_org[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.min_org[2]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.max_org[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.max_org[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.max_org[2]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.e_size[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.e_size[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.e_size[2]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.dir[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.dir[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.dir[2]);
				fscanf(FH, "%d ", &sv.Effects[idx].ef.Rain.color);
				fscanf(FH, "%d ", &sv.Effects[idx].ef.Rain.count);
				fscanf(FH, "%f\n", &sv.Effects[idx].ef.Rain.wait);
				break;

			case CE_SNOW:
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.min_org[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.min_org[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.min_org[2]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.max_org[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.max_org[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.max_org[2]);
				fscanf(FH, "%d ", &sv.Effects[idx].ef.Rain.flags);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.dir[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.dir[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Rain.dir[2]);
				fscanf(FH, "%d ", &sv.Effects[idx].ef.Rain.count);
				//fscanf(FH, "%d ", &sv.Effects[idx].ef.Rain.veer);

				// O.S:	a linefeed is missing here. not adding
				//	it so as not to break existing saves.
				//	also see in: SV_SaveEffects().
				break;

			case CE_FOUNTAIN:
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Fountain.pos[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Fountain.pos[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Fountain.pos[2]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Fountain.angle[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Fountain.angle[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Fountain.angle[2]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Fountain.movedir[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Fountain.movedir[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Fountain.movedir[2]);
				fscanf(FH, "%d ", &sv.Effects[idx].ef.Fountain.color);
				fscanf(FH, "%d\n", &sv.Effects[idx].ef.Fountain.cnt);
				break;

			case CE_QUAKE:
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Quake.origin[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Quake.origin[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Quake.origin[2]);
				fscanf(FH, "%f\n", &sv.Effects[idx].ef.Quake.radius);
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
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Smoke.origin[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Smoke.origin[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Smoke.origin[2]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Smoke.velocity[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Smoke.velocity[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Smoke.velocity[2]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Smoke.framelength);
				fscanf(FH, "%f\n", &sv.Effects[idx].ef.Smoke.frame);
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
			case CE_FIREWALL_SMALL:
			case CE_FIREWALL_MEDIUM:
			case CE_FIREWALL_LARGE:
			case CE_BOMB:
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Smoke.origin[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Smoke.origin[1]);
				fscanf(FH, "%f\n", &sv.Effects[idx].ef.Smoke.origin[2]);
				break;

			case CE_WHITE_FLASH:
			case CE_BLUE_FLASH:
			case CE_SM_BLUE_FLASH:
			case CE_RED_FLASH:
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Flash.origin[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Flash.origin[1]);
				fscanf(FH, "%f\n", &sv.Effects[idx].ef.Flash.origin[2]);
				break;

			case CE_RIDER_DEATH:
				fscanf(FH, "%f ", &sv.Effects[idx].ef.RD.origin[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.RD.origin[1]);
				fscanf(FH, "%f\n", &sv.Effects[idx].ef.RD.origin[2]);
				break;

			case CE_GRAVITYWELL:
				fscanf(FH, "%f ", &sv.Effects[idx].ef.RD.origin[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.RD.origin[1]);
				fscanf(FH, "%f", &sv.Effects[idx].ef.RD.origin[2]);
				fscanf(FH, "%d", &sv.Effects[idx].ef.RD.color);
				fscanf(FH, "%f\n", &sv.Effects[idx].ef.RD.lifetime);
				break;

			case CE_TELEPORTERPUFFS:
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Teleporter.origin[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Teleporter.origin[1]);
				fscanf(FH, "%f\n", &sv.Effects[idx].ef.Teleporter.origin[2]);
				break;

			case CE_TELEPORTERBODY:
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Teleporter.origin[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Teleporter.origin[1]);
				fscanf(FH, "%f\n", &sv.Effects[idx].ef.Teleporter.origin[2]);
				break;

			case CE_BONESHARD:
			case CE_BONESHRAPNEL:
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Missile.origin[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Missile.origin[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Missile.origin[2]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Missile.velocity[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Missile.velocity[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Missile.velocity[2]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Missile.angle[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Missile.angle[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Missile.angle[2]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Missile.avelocity[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Missile.avelocity[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Missile.avelocity[2]);

				// O.S:	a linefeed is missing here. not adding
				//	it so as not to break existing saves.
				//	also see in: SV_SaveEffects().
				break;

			case CE_CHUNK:
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Chunk.origin[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Chunk.origin[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Chunk.origin[2]);
				fscanf(FH, "%u ", (unsigned int *)&sv.Effects[idx].ef.Chunk.type);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Chunk.srcVel[0]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Chunk.srcVel[1]);
				fscanf(FH, "%f ", &sv.Effects[idx].ef.Chunk.srcVel[2]);
				fscanf(FH, "%u ", (unsigned int *)&sv.Effects[idx].ef.Chunk.numChunks);

				// O.S:	a linefeed is missing here. not adding
				//	it so as not to break existing saves.
				//	also see in: SV_SaveEffects().
				break;

			default:
				PR_RunError ("SV_SaveEffect: bad type");
				break;
		}
	}
}


/*
 * $Log: not supported by cvs2svn $
 * Revision 1.2  2006/07/02 11:45:34  sezero
 * minor optimiziations to mathlib: added VectorNegate and VectorClear macros
 * which stops vec3_origin usage in relevant calculations. renamed the Length
 * macro to VectorLength for consistancy. updated the utilities' mathlib for
 * similar macro usage as in the engine.
 *
 * Revision 1.1  2006/06/24 14:12:59  sezero
 * separated cl_effect.c and sv_effect.c for easier maintainance
 *
 */
