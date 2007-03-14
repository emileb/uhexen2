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
	entities.c
	$Id: entities.c,v 1.11 2007-03-14 21:04:35 sezero Exp $
*/

#include "util_inc.h"
#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "tyrlite.h"
#include "entities.h"


entity_t	entities[MAX_MAP_ENTITIES];
int			num_entities;
int			num_lights;
int			num_clights;


#if 0	// no users.
static int		numtexlights;
static entity_t	texlights[MAX_MAP_FACES];

// find textures that could emit lights
void FindTexLights (void)
{
	int		i;
	int		hit;
	char	texname[16];

	numtexlights = 0;

#   if 0
	for (i = 0; i < numfaces; i++)
	{
		// make it more legible... and easier (less typing)
		sprintf (texname, "%s", miptex[texinfo[dfaces[i].texinfo].miptex].name);
		hit = 1; // assume success until we find out otherwise

		if (!strncmp (texname, "*lava", 5))
		{
			// specific to each texture
			texlights[numtexlights].lightcolour[0] = 255;
			texlights[numtexlights].lightcolour[1] = 64;
			texlights[numtexlights].lightcolour[2] = 64;
		}
		else
			hit = 0;

		if (hit)
		{
			// common to all
			texlights[numtexlights].light = 300;
			strcpy (texlights[numtexlights].classname, "light");
			texlights[numtexlights].origin[0] = dplanes[dfaces[i].planenum].normal[0];
			texlights[numtexlights].origin[1] = dplanes[dfaces[i].planenum].normal[1];
			texlights[numtexlights].origin[2] = dplanes[dfaces[i].planenum].normal[2];
			numtexlights++;
		}
	}
#   endif
}
#endif

/*
==============================================================================

ENTITY FILE PARSING

If a light has a targetname, generate a unique style in the 32-63 range
==============================================================================
*/

static int		numlighttargets;
static char	lighttargets[32][64];

static int LightStyleForTargetname (char *targetname, qboolean alloc)
{
	int		i;

	for (i = 0 ; i < numlighttargets ; i++)
		if (!strcmp (lighttargets[i], targetname))
			return 32 + i;
	if (!alloc)
		return -1;
	strcpy (lighttargets[i], targetname);
	numlighttargets++;

	// DEBUG
	// printf("Generating lightstyle %i for targetname \"%s\"\n",
	//         numlighttargets-1+32, targetname);

	return numlighttargets-1 + 32;
}


/*
==================
MatchTargets
==================
*/
static void MatchTargets (void)
{
	int		i, j;

	for (i = 0 ; i < num_entities ; i++)
	{
		if (!entities[i].target[0])
			continue;

		for (j = 0 ; j < num_entities ; j++)
			if (!strcmp(entities[j].targetname, entities[i].target))
			{
				entities[i].targetent = &entities[j];
				break;
			}
		if (j == num_entities)
		{
			printf ("WARNING: entity at (%i,%i,%i) (%s) has unmatched target\n",
					(int)entities[i].origin[0], (int)entities[i].origin[1],
					(int)entities[i].origin[2], entities[i].classname);
			continue;
		}

	// set the style on the source ent for switchable lights
		if (entities[j].style)
		{
			char	s[16];

			entities[i].style = entities[j].style;
			sprintf (s,"%i", entities[i].style);
			SetKeyValue (&entities[i], "style", s);
		}
	}
}


/*
==================
LoadEntities
==================
*/
extern int	bsp_ver;
void LoadEntities (void)
{
	char		*data;
	entity_t	*entity;
	char		key[64];
	epair_t		*epair;
	vec_t		vec[3];
	int		i;

	data = dentdata;
//
// start parsing
//
	num_entities = 0;
	num_lights = 0;
// go through all the entities
	while (1)
	{
	// parse the opening brace
		data = COM_Parse (data);
		if (!data)
			break;
		if (com_token[0] != '{')
			Error ("%s: found %s when expecting {", __FUNCTION__, com_token);

		if (num_entities == MAX_MAP_ENTITIES)
			Error ("%s: MAX_MAP_ENTITIES", __FUNCTION__);
		entity = &entities[num_entities];
		num_entities++;

	// flag to indicate use of mangle key
		entity->use_mangle = false;

	// go through all the keys in this entity
		while (1)
		{
			int		c;

		// parse key
			data = COM_Parse (data);
			if (!data)
				Error ("%s: EOF without closing brace", __FUNCTION__);
			if (!strcmp(com_token,"}"))
				break;
			strcpy (key, com_token);

		// parse value
			data = COM_Parse (data);
			if (!data)
				Error ("%s: EOF without closing brace", __FUNCTION__);
			c = com_token[0];
			if (c == '}')
				Error ("%s: closing brace without data", __FUNCTION__);

			epair = malloc (sizeof(epair_t));
			memset (epair, 0, sizeof(epair));
			strcpy (epair->key, key);
			strcpy (epair->value, com_token);
			epair->next = entity->epairs;
			entity->epairs = epair;

			if (!strcmp(key, "classname"))
				strcpy (entity->classname, com_token);
			else if (!strcmp (key, "message"))
				strcpy (entity->message, com_token);
			else if (!strcmp (key, "netname"))
				strcpy (entity->netname, com_token);
			else if (!strcmp(key, "target"))
				strcpy (entity->target, com_token);
			else if (!strcmp(key, "targetname"))
				strcpy (entity->targetname, com_token);
			else if (!strcmp(key, "origin"))
			{
#ifdef DOUBLEVEC_T
				if (sscanf(com_token, "%lf %lf %lf",
#else
				if (sscanf(com_token, "%f %f %f",
#endif
						&entity->origin[0],
						&entity->origin[1],
						&entity->origin[2]) != 3)
					Error ("%s: not 3 values for origin", __FUNCTION__);
			}
			else if (!strncmp(key, "light", 5))
			{
				//entity->light = atoi(com_token);
				entity->light = atof(com_token);
			}
			else if (!strncmp (key, "_light", 6))
			{
				entity->light = atof(com_token);
			}
			else if (!strcmp(key, "style"))
			{
				entity->style = atoi(com_token);
				if ((unsigned)entity->style > 254)
					Error ("Bad light style %i (must be 0-254)", entity->style);
			}
			else if (!strcmp(key, "angle"))
				entity->angle = atof(com_token);
			else if (!strcmp(key, "wait"))
				entity->atten = atof(com_token);
			else if (!strcmp(key, "delay"))
				entity->formula = atoi(com_token);
			else if (!strcmp(key, "mangle"))
			{
#ifdef DOUBLEVEC_T
				if (sscanf(com_token, "%lf %lf %lf", &vec[0], &vec[1], &vec[2]) != 3)
#else
				if (sscanf(com_token, "%f %f %f", &vec[0], &vec[1], &vec[2]) != 3)
#endif
					Error ("%s: not 3 values for mangle", __FUNCTION__);

				/* Precalculate the direction vector		*/
				entity->use_mangle = true;
				entity->mangle[0] = cos(vec[0]*Q_PI/180)*cos(vec[1]*Q_PI/180);
				entity->mangle[1] = sin(vec[0]*Q_PI/180)*cos(vec[1]*Q_PI/180);
				entity->mangle[2] = sin(vec[1]*Q_PI/180);
			}
			else if (!strcmp(key, "_color")/* don't work with hipnotic particle fields || !strcmp(key, "color")*/)
			{
#ifdef DOUBLEVEC_T
				if (sscanf(com_token, "%lf %lf %lf", &vec[0], &vec[1], &vec[2]) != 3)
#else
				if (sscanf(com_token, "%f %f %f", &vec[0], &vec[1], &vec[2]) != 3)
#endif
					Error ("%s: not 3 values for colour", __FUNCTION__);
				for (i=0 ; i<3 ; i++)
					entity->lightcolour[i] = vec[i];
			}
			else if (!strcmp(key, "_sunlight"))
				sunlight = atof(com_token);
			else if (!strcmp(key, "_sun_mangle"))
			{
#ifdef DOUBLEVEC_T
				if (sscanf(com_token, "%lf %lf %lf", &vec[0], &vec[1], &vec[2]) != 3)
#else
				if (sscanf(com_token, "%f %f %f", &vec[0], &vec[1], &vec[2]) != 3)
#endif
					Error ("%s: not 3 values for _sun_mangle", __FUNCTION__);

				/* Precalculate sun vector and			*/
				/* make it too large to fit into the map	*/
				sunmangle[0] = cos(vec[0]*Q_PI/180)*cos(vec[1]*Q_PI/180);
				sunmangle[1] = sin(vec[0]*Q_PI/180)*cos(vec[1]*Q_PI/180);
				sunmangle[2] = sin(vec[1]*Q_PI/180);
				VectorNormalize(sunmangle);
				VectorScale(sunmangle, -16384, sunmangle);
			}
			else if (!strcmp(key, "_sunlight_color"))
			{
#ifdef DOUBLEVEC_T
				if (sscanf(com_token, "%lf %lf %lf",
#else
				if (sscanf(com_token, "%f %f %f",
#endif
						&sunlight_color[0],
						&sunlight_color[1],
						&sunlight_color[2]) != 3)
					Error ("%s: not 3 values for _sunlight_color", __FUNCTION__);
			}
			else if (!strcmp(key, "_minlight_color"))
			{
#ifdef DOUBLEVEC_T
				if (sscanf(com_token, "%lf %lf %lf",
#else
				if (sscanf(com_token, "%f %f %f",
#endif
						&minlight_color[0],
						&minlight_color[1],
						&minlight_color[2]) != 3)
					Error ("%s: not 3 values for _minlight_color", __FUNCTION__);
			}
			/*
			else if (!strcmp(key, "angle"))
			{
				entity->angle = (float)atof(com_token);
			}
			*/
		}

	// all fields have been parsed
	// check default settings and check for light value in worldspawn
	/*
		if (!strncmp (entity->classname, "light", 5) && !entity->light)
			entity->light = DEFAULTLIGHTLEVEL;

		if (!strncmp (entity->classname, "light", 5))
		{
			if (entity->targetname[0] && !entity->style)
			{
				char	s[16];

				entity->style = LightStyleForTargetname (entity->targetname, true);
				sprintf (s,"%i", entity->style);
				SetKeyValue (entity, "style", s);
			}
		}
	*/

		if (!strncmp (entity->classname, "light", 5))
		{
			num_clights++;

			if (!entity->light)
				entity->light = DEFAULTLIGHTLEVEL;
			if (entity->atten <= 0.0)
				entity->atten = 1.0;
			if ((entity->formula < 0) || (entity->formula > 3))
				entity->formula = 0;

			// Convert to Hexen2 Entities
			// since the map has no colour info anyway, we don't need to test this
			if (!Q_strncasecmp(entity->classname, "light_torch", 11))
			{
			/*
				In Hexen 2 :
				light_torch_castle
				light_torch_rome
				light_torch_meso
				light_torch_egypt
				light_torch_small_walltorch
			*/

				// make it orange
				entity->lightcolour[0] = 255;
				entity->lightcolour[1] = 128;
				entity->lightcolour[2] = 64;
			}
			else if (!Q_strncasecmp(entity->classname, "light_flame", 11))
			{
				// make it orange
			/*
				In Hexen 2 :
				light_flame_large_yellow
				light_flame_small_yellow
			*/

				entity->lightcolour[0] = 255;
				entity->lightcolour[1] = 128;
				entity->lightcolour[2] = 64;
			}
			else if (!strcmp (entity->classname, "light_gem"))
			{
				// make it orange
				entity->lightcolour[0] = 255;
				entity->lightcolour[1] = 128;
				entity->lightcolour[2] = 64;
			}

			/*
			// No Such Light Type in Hexen 2
			else if (!strncmp (entity->classname, "light_fluor", 11))
			{
				// make all colours 0 - fluoro lights are tested the same way as normal ones,
				// so we don't want any colour info to begin with
				num_clights--;
				entity->lightcolour[0] = 0;
				entity->lightcolour[1] = 0;
				entity->lightcolour[2] = 0;
			}
			else if (!strcmp (entity->classname, "light_globe"))
			{
				// make it orange
				entity->lightcolour[0] = 255;
				entity->lightcolour[1] = 128;
				entity->lightcolour[2] = 64;
			}
			else if (!strcmp (entity->classname, "light_candle"))
			{
				// make it orange
				entity->lightcolour[0] = 255;
				entity->lightcolour[1] = 128;
				entity->lightcolour[2] = 64;
			}
			else if (!strcmp (entity->classname, "light_lantern"))
			{
				// make it orange
				entity->lightcolour[0] = 255;
				entity->lightcolour[1] = 128;
				entity->lightcolour[2] = 64;
			}
			*/
			// End Convert to Hexen2 Entities
			else
			{
				// we don't need any colour info to begin with
				num_clights--;
				entity->lightcolour[0] = 0;
				entity->lightcolour[1] = 0;
				entity->lightcolour[2] = 0;
			}

			num_lights++;
			//printf ("entity light %i\n", entity->light);
		}

		if (!strcmp (entity->classname, "light"))
		{
			if (entity->targetname[0] && !entity->style)
			{
				char	s[16];

				entity->style = LightStyleForTargetname (entity->targetname, true);
				sprintf (s,"%i", entity->style);
				SetKeyValue (entity, "style", s);
			}
		}

		if (!strcmp (entity->classname, "worldspawn"))
		{
			// mapname according to the message field requires line
			// number entity->message to be read from strings.txt
			if (strlen(entity->netname))
				printf ("Map name : \"%s\"\n\n", entity->netname);
			else if (strlen(entity->message) && atoi(entity->message) > 0)
				printf ("Map name : at line %d in strings.txt\n\n", atoi(entity->message));
			else
				printf ("Map name : -- unknown --\n\n");

			//printf("BSP is version %i\n", bsp_ver);

			if (entity->light > 0 && !worldminlight)
			{
				worldminlight = entity->light;
				printf("using minlight value %i from worldspawn.\n", worldminlight);
			}
			else if (worldminlight)
			{
				printf("Using minlight value %i from command line.\n", worldminlight);
			}

			worldminlight = 0;
				// WTF? Why nullify the -light cmdline arg??
				// This was never in TyrLite but is in MHColour.
		}
	}

	printf ("%d entities read, %d are lights.\n", num_entities, num_lights);

	MatchTargets ();
}


char *ValueForKey (entity_t *ent, char *key)
{
	epair_t	*ep;

	for (ep = ent->epairs ; ep ; ep = ep->next)
		if (!strcmp (ep->key, key) )
			return ep->value;
	return "";
}

void SetKeyValue (entity_t *ent, char *key, char *value)
{
	epair_t	*ep;

	for (ep = ent->epairs ; ep ; ep = ep->next)
		if (!strcmp (ep->key, key) )
		{
			strcpy (ep->value, value);
			return;
		}

	ep = malloc (sizeof(*ep));
	ep->next = ent->epairs;
	ent->epairs = ep;
	strcpy (ep->key, key);
	strcpy (ep->value, value);
}

float FloatForKey (entity_t *ent, char *key)
{
	char	*k;

	k = ValueForKey (ent, key);
	return (float)atof(k);
}

void GetVectorForKey (entity_t *ent, char *key, vec3_t vec)
{
	char	*k;

	k = ValueForKey (ent, key);
#ifdef DOUBLEVEC_T
	sscanf (k, "%lf %lf %lf", &vec[0], &vec[1], &vec[2]);
#else
	sscanf (k, "%f %f %f", &vec[0], &vec[1], &vec[2]);
#endif
}


entity_t *FindEntityWithKeyPair (char *key, char *value)
{
	entity_t	*ent;
	epair_t	*ep;
	int			i;

	for (i = 0 ; i < num_entities ; i++)
	{
		ent = &entities[ i ];
		for (ep=ent->epairs ; ep ; ep=ep->next)
			if (!strcmp (ep->key, key))
			{
				if (!strcmp( ep->value, value ))
					return ent;
				break;
			}
	}
	return NULL;
}



/*
================
WriteEntitiesToString
================
*/
void WriteEntitiesToString (void)
{
	char	*buf, *end;
	epair_t	*ep;
	char	line[128];
	int		i;

	buf = dentdata;
	end = buf;
	*end = 0;

	printf ("%i switchable light styles\n", numlighttargets);

	for (i = 0 ; i < num_entities ; i++)
	{
		ep = entities[i].epairs;
		if (!ep)
			continue;	// ent got removed

		strcat (end,"{\n");
		end += 2;

		for (ep = entities[i].epairs ; ep ; ep = ep->next)
		{
			if (!strncmp(entities[i].classname, "light", 5) && compress_ents)
			{
				// mfah - added style and target here cos the engine needs them!
				if (!strcmp(ep->key, "classname"))
				{
					sprintf (line, "\"%s\" \"%s\"\n", ep->key, ep->value);
					strcat (end, line);
					end += strlen(line);
				}
				else if (!strcmp(ep->key, "origin"))
				{
					sprintf (line, "\"%s\" \"%s\"\n", ep->key, ep->value);
					strcat (end, line);
					end += strlen(line);
				}
				else if (!strcmp(ep->key, "style"))
				{
					sprintf (line, "\"%s\" \"%s\"\n", ep->key, ep->value);
					strcat (end, line);
					end += strlen(line);
				}
				else if (!strcmp(ep->key, "target"))
				{
					sprintf (line, "\"%s\" \"%s\"\n", ep->key, ep->value);
					strcat (end, line);
					end += strlen(line);
				}
				else if (!strcmp(ep->key, "targetname"))
				{
					sprintf (line, "\"%s\" \"%s\"\n", ep->key, ep->value);
					strcat (end, line);
					end += strlen(line);
				}

				// output a colour...
				//sprintf (line, "\"_color\" \"%i %i %i\"\n", entities[i].lightcolour[0], entities[i].lightcolour[1], entities[i].lightcolour[2]);
				//strcat (end, line);
				//end += strlen(line);
			}
			else
			{
				sprintf (line, "\"%s\" \"%s\"\n", ep->key, ep->value);
				strcat (end, line);
				end += strlen(line);
			}
		}
		strcat (end,"}\n");
		end += 2;

		if (end > buf + MAX_MAP_ENTSTRING)
			Error ("Entity text too long");
	}
	entdatasize = end - buf + 1;
}

