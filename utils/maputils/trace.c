// trace.c

#include "util_inc.h"
#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "entities.h"
#include "threads.h"
#include "light.h"

typedef struct tnode_s
{
	int		type;
	vec3_t	normal;
	float	dist;
	int		children[2];
	int		pad;
} tnode_t;

static tnode_t		*tnodes, *tnode_p;

/*
==============
MakeTnode

Converts the disk node structure into the efficient tracing structure
==============
*/
static void MakeTnode (int nodenum)
{
	tnode_t			*t;
	dplane_t		*plane;
	int				i;
	dnode_t 		*node;

	t = tnode_p++;

	node = dnodes + nodenum;
	plane = dplanes + node->planenum;

	t->type = plane->type;
	VectorCopy (plane->normal, t->normal);
	t->dist = plane->dist;

	for (i = 0 ; i < 2 ; i++)
	{
		if (node->children[i] < 0)
			t->children[i] = dleafs[-node->children[i] - 1].contents;
		else
		{
			t->children[i] = tnode_p - tnodes;
			MakeTnode (node->children[i]);
		}
	}
}


/*
=============
MakeTnodes

Loads the node structure out of a .bsp file to be used for light occlusion
=============
*/
void MakeTnodes (dmodel_t *bm)
{
	tnode_p = tnodes = malloc(numnodes * sizeof(tnode_t));

	MakeTnode (0);
}


/*
==============================================================================

LINE TRACING

The major lighting operation is a point to point visibility test, performed
by recursive subdivision of the line by the BSP tree.

==============================================================================
*/

typedef struct
{
	vec3_t	backpt;
	int		side;
	int		node;
} tracestack_t;


/*
==============
TestLine
==============
*/
qboolean TestLine (vec3_t start, vec3_t stop)
{
	int			node, side;
	float		front, back;
	float 		frontx, fronty, frontz, backx, backy, backz;
	tracestack_t	*tstack_p;
	tracestack_t	tracestack[64];
	tnode_t			*tnode;

	frontx = (float)start[0];
	fronty = (float)start[1];
	frontz = (float)start[2];
	backx = (float)stop[0];
	backy = (float)stop[1];
	backz = (float)stop[2];

	tstack_p = tracestack;
	node = 0;

	while (1)
	{
		while (node < 0 && node != CONTENTS_SOLID)
		{
		// pop up the stack for a back side
			tstack_p--;
			if (tstack_p < tracestack)
				return true;
			node = tstack_p->node;

		// set the hit point for this plane

			frontx = backx;
			fronty = backy;
			frontz = backz;

		// go down the back side

			backx = (float)tstack_p->backpt[0];
			backy = (float)tstack_p->backpt[1];
			backz = (float)tstack_p->backpt[2];

			node = tnodes[tstack_p->node].children[!tstack_p->side];
		}

		if (node == CONTENTS_SOLID)
			return false;	// DONE!

		tnode = &tnodes[node];

		switch (tnode->type)
		{
		case PLANE_X:
			front = frontx - tnode->dist;
			back = backx - tnode->dist;
			break;
		case PLANE_Y:
			front = fronty - tnode->dist;
			back = backy - tnode->dist;
			break;
		case PLANE_Z:
			front = frontz - tnode->dist;
			back = backz - tnode->dist;
			break;
		default:
			front = (float)((frontx*tnode->normal[0] + fronty*tnode->normal[1] + frontz*tnode->normal[2]) - tnode->dist);
			back = (float)((backx*tnode->normal[0] + backy*tnode->normal[1] + backz*tnode->normal[2]) - tnode->dist);
			break;
		}

	//	if (front > 0 && back > 0)
		if (front > -ON_EPSILON && back > -ON_EPSILON)
		{
			node = tnode->children[0];
			continue;
		}

	//	if (front <= 0 && back <= 0)
		if (front < ON_EPSILON && back < ON_EPSILON)
		{
			node = tnode->children[1];
			continue;
		}

	//	side = front < 0;
		side = (front < 0.0f) ? 1 : 0;

	//	front = front / (front-back);
		front /= (front - back);

		tstack_p->node = node;
		tstack_p->side = side;
		tstack_p->backpt[0] = backx;
		tstack_p->backpt[1] = backy;
		tstack_p->backpt[2] = backz;

		tstack_p++;

		backx = frontx + front*(backx-frontx);
		backy = fronty + front*(backy-fronty);
		backz = frontz + front*(backz-frontz);

		node = tnode->children[side];
	}
}

