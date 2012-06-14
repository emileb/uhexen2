/*
	pr_exec.c
	PROGS execution

	$Id$
*/

// HEADER FILES ------------------------------------------------------------

#include "quakedef.h"
#include <ctype.h>

// MACROS ------------------------------------------------------------------

#define MAX_STACK_DEPTH	64	/* was 32 */
#define LOCALSTACK_SIZE	2048

// TYPES -------------------------------------------------------------------

typedef struct
{
	int		s;
	dfunction_t	*f;
} prstack_t;

/* switch types */
enum {
	SWITCH_F,
	SWITCH_V,
	SWITCH_S,
	SWITCH_E,
	SWITCH_FNC
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

const char *PR_GlobalString(int ofs);
const char *PR_GlobalStringNoContents(int ofs);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int EnterFunction(dfunction_t *f);
static int LeaveFunction(void);
static void PrintStatement(dstatement_t *s);
static void PrintCallHistory(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

qboolean	pr_trace;
dfunction_t	*pr_xfunction;
int		pr_xstatement;
int		pr_argc;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static prstack_t pr_stack[MAX_STACK_DEPTH];
static int pr_depth;
static int localstack[LOCALSTACK_SIZE];
static int localstack_used;

static const char *pr_opnames[] =
{
	"DONE",
	"MUL_F", "MUL_V",  "MUL_FV", "MUL_VF",
	"DIV",
	"ADD_F", "ADD_V",
	"SUB_F", "SUB_V",
	"EQ_F", "EQ_V", "EQ_S", "EQ_E", "EQ_FNC",
	"NE_F", "NE_V", "NE_S", "NE_E", "NE_FNC",
	"LE", "GE", "LT", "GT",
	"INDIRECT", "INDIRECT", "INDIRECT",
	"INDIRECT", "INDIRECT", "INDIRECT",
	"ADDRESS",
	"STORE_F", "STORE_V", "STORE_S",
	"STORE_ENT", "STORE_FLD", "STORE_FNC",
	"STOREP_F", "STOREP_V", "STOREP_S",
	"STOREP_ENT", "STOREP_FLD", "STOREP_FNC",
	"RETURN",
	"NOT_F", "NOT_V", "NOT_S", "NOT_ENT", "NOT_FNC",
	"IF", "IFNOT",
	"CALL0", "CALL1", "CALL2", "CALL3", "CALL4",
	"CALL5", "CALL6", "CALL7", "CALL8",
	"STATE",
	"GOTO",
	"AND", "OR",
	"BITAND", "BITOR",
	"OP_MULSTORE_F", "OP_MULSTORE_V", "OP_MULSTOREP_F", "OP_MULSTOREP_V",
	"OP_DIVSTORE_F", "OP_DIVSTOREP_F",
	"OP_ADDSTORE_F", "OP_ADDSTORE_V", "OP_ADDSTOREP_F", "OP_ADDSTOREP_V",
	"OP_SUBSTORE_F", "OP_SUBSTORE_V", "OP_SUBSTOREP_F", "OP_SUBSTOREP_V",
	"OP_FETCH_GBL_F",
	"OP_FETCH_GBL_V",
	"OP_FETCH_GBL_S",
	"OP_FETCH_GBL_E",
	"OP_FETCH_GBL_FNC",
	"OP_CSTATE", "OP_CWSTATE",

	"OP_THINKTIME",

	"OP_BITSET", "OP_BITSETP", "OP_BITCLR",	"OP_BITCLRP",

	"OP_RAND0", "OP_RAND1",	"OP_RAND2",	"OP_RANDV0", "OP_RANDV1", "OP_RANDV2",

	"OP_SWITCH_F", "OP_SWITCH_V", "OP_SWITCH_S", "OP_SWITCH_E", "OP_SWITCH_FNC",

	"OP_CASE",
	"OP_CASERANGE"

};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// PR_ExecuteProgram
//
//==========================================================================

#define OPA ((eval_t *)&pr_globals[(unsigned short)st->a])
#define OPB ((eval_t *)&pr_globals[(unsigned short)st->b])
#define OPC ((eval_t *)&pr_globals[(unsigned short)st->c])

void PR_ExecuteProgram (func_t fnum)
{
	eval_t		*ptr;
	dstatement_t	*st;
	dfunction_t	*f, *newf;
	int profile, startprofile;
	edict_t		*ed;
	int		exitdepth;
	/* switch/case support:  */
	int		case_type = -1;
	float	switch_float = 0; /* avoid 'maybe used unititialized' */

	if (!fnum || fnum >= progs->numfunctions)
	{
		if (*sv_globals.self)
		{
			ED_Print(PROG_TO_EDICT(*sv_globals.self));
		}
		Host_Error("%s: NULL function", __thisfunc__);
	}

	f = &pr_functions[fnum];

	pr_trace = false;

	exitdepth = pr_depth;

	st = &pr_statements[EnterFunction(f)];
	startprofile = profile = 0;

    while (1)
    {
	st++;	/* next statement */

	if (++profile > 100000)
	{
		pr_xstatement = st - pr_statements;
		PR_RunError("runaway loop error");
	}

	if (pr_trace)
	{
		PrintStatement(st);
	}

	switch (st->op)
	{
	case OP_ADD_F:
		OPC->_float = OPA->_float + OPB->_float;
		break;
	case OP_ADD_V:
		OPC->vector[0] = OPA->vector[0] + OPB->vector[0];
		OPC->vector[1] = OPA->vector[1] + OPB->vector[1];
		OPC->vector[2] = OPA->vector[2] + OPB->vector[2];
		break;

	case OP_SUB_F:
		OPC->_float = OPA->_float - OPB->_float;
		break;
	case OP_SUB_V:
		OPC->vector[0] = OPA->vector[0] - OPB->vector[0];
		OPC->vector[1] = OPA->vector[1] - OPB->vector[1];
		OPC->vector[2] = OPA->vector[2] - OPB->vector[2];
		break;

	case OP_MUL_F:
		OPC->_float = OPA->_float * OPB->_float;
		break;
	case OP_MUL_V:
		OPC->_float = OPA->vector[0] * OPB->vector[0] +
			      OPA->vector[1] * OPB->vector[1] +
			      OPA->vector[2] * OPB->vector[2];
		break;
	case OP_MUL_FV:
		OPC->vector[0] = OPA->_float * OPB->vector[0];
		OPC->vector[1] = OPA->_float * OPB->vector[1];
		OPC->vector[2] = OPA->_float * OPB->vector[2];
		break;
	case OP_MUL_VF:
		OPC->vector[0] = OPB->_float * OPA->vector[0];
		OPC->vector[1] = OPB->_float * OPA->vector[1];
		OPC->vector[2] = OPB->_float * OPA->vector[2];
		break;

	case OP_DIV_F:
		OPC->_float = OPA->_float / OPB->_float;
		break;

	case OP_BITAND:
		OPC->_float = (int)OPA->_float & (int)OPB->_float;
		break;

	case OP_BITOR:
		OPC->_float = (int)OPA->_float | (int)OPB->_float;
		break;

	case OP_GE:
		OPC->_float = OPA->_float >= OPB->_float;
		break;
	case OP_LE:
		OPC->_float = OPA->_float <= OPB->_float;
		break;
	case OP_GT:
		OPC->_float = OPA->_float > OPB->_float;
		break;
	case OP_LT:
		OPC->_float = OPA->_float < OPB->_float;
		break;
	case OP_AND:
		OPC->_float = OPA->_float && OPB->_float;
		break;
	case OP_OR:
		OPC->_float = OPA->_float || OPB->_float;
		break;

	case OP_NOT_F:
		OPC->_float = !OPA->_float;
		break;
	case OP_NOT_V:
		OPC->_float = !OPA->vector[0] && !OPA->vector[1] && !OPA->vector[2];
		break;
	case OP_NOT_S:
		OPC->_float = !OPA->string || !*PR_GetString(OPA->string);
		break;
	case OP_NOT_FNC:
		OPC->_float = !OPA->function;
		break;
	case OP_NOT_ENT:
		OPC->_float = (PROG_TO_EDICT(OPA->edict) == sv.edicts);
		break;

	case OP_EQ_F:
		OPC->_float = OPA->_float == OPB->_float;
		break;
	case OP_EQ_V:
		OPC->_float = (OPA->vector[0] == OPB->vector[0]) &&
			      (OPA->vector[1] == OPB->vector[1]) &&
			      (OPA->vector[2] == OPB->vector[2]);
		break;
	case OP_EQ_S:
		OPC->_float = !strcmp(PR_GetString(OPA->string), PR_GetString(OPB->string));
		break;
	case OP_EQ_E:
		OPC->_float = OPA->_int == OPB->_int;
		break;
	case OP_EQ_FNC:
		OPC->_float = OPA->function == OPB->function;
		break;

	case OP_NE_F:
		OPC->_float = OPA->_float != OPB->_float;
		break;
	case OP_NE_V:
		OPC->_float = (OPA->vector[0] != OPB->vector[0]) ||
			      (OPA->vector[1] != OPB->vector[1]) ||
			      (OPA->vector[2] != OPB->vector[2]);
		break;
	case OP_NE_S:
		OPC->_float = strcmp(PR_GetString(OPA->string), PR_GetString(OPB->string));
		break;
	case OP_NE_E:
		OPC->_float = OPA->_int != OPB->_int;
		break;
	case OP_NE_FNC:
		OPC->_float = OPA->function != OPB->function;
		break;

	case OP_STORE_F:
	case OP_STORE_ENT:
	case OP_STORE_FLD:	// integers
	case OP_STORE_S:
	case OP_STORE_FNC:	// pointers
		OPB->_int = OPA->_int;
		break;
	case OP_STORE_V:
		OPB->vector[0] = OPA->vector[0];
		OPB->vector[1] = OPA->vector[1];
		OPB->vector[2] = OPA->vector[2];
		break;

	case OP_STOREP_F:
	case OP_STOREP_ENT:
	case OP_STOREP_FLD:	// integers
	case OP_STOREP_S:
	case OP_STOREP_FNC:	// pointers
		ptr = (eval_t *)((byte *)sv.edicts + OPB->_int);
		ptr->_int = OPA->_int;
		break;
	case OP_STOREP_V:
		ptr = (eval_t *)((byte *)sv.edicts + OPB->_int);
		ptr->vector[0] = OPA->vector[0];
		ptr->vector[1] = OPA->vector[1];
		ptr->vector[2] = OPA->vector[2];
		break;

	case OP_MULSTORE_F:	// f *= f
		OPB->_float *= OPA->_float;
		break;
	case OP_MULSTORE_V:	// v *= f
		OPB->vector[0] *= OPA->_float;
		OPB->vector[1] *= OPA->_float;
		OPB->vector[2] *= OPA->_float;
		break;
	case OP_MULSTOREP_F:	// e.f *= f
		ptr = (eval_t *)((byte *)sv.edicts + OPB->_int);
		OPC->_float = (ptr->_float *= OPA->_float);
		break;
	case OP_MULSTOREP_V:	// e.v *= f
		ptr = (eval_t *)((byte *)sv.edicts + OPB->_int);
		OPC->vector[0] = (ptr->vector[0] *= OPA->_float);
		OPC->vector[0] = (ptr->vector[1] *= OPA->_float);
		OPC->vector[0] = (ptr->vector[2] *= OPA->_float);
		break;

	case OP_DIVSTORE_F:	// f /= f
		OPB->_float /= OPA->_float;
		break;
	case OP_DIVSTOREP_F:	// e.f /= f
		ptr = (eval_t *)((byte *)sv.edicts + OPB->_int);
		OPC->_float = (ptr->_float /= OPA->_float);
		break;

	case OP_ADDSTORE_F:	// f += f
		OPB->_float += OPA->_float;
		break;
	case OP_ADDSTORE_V:	// v += v
		OPB->vector[0] += OPA->vector[0];
		OPB->vector[1] += OPA->vector[1];
		OPB->vector[2] += OPA->vector[2];
		break;
	case OP_ADDSTOREP_F:	// e.f += f
		ptr = (eval_t *)((byte *)sv.edicts + OPB->_int);
		OPC->_float = (ptr->_float += OPA->_float);
		break;
	case OP_ADDSTOREP_V:	// e.v += v
		ptr = (eval_t *)((byte *)sv.edicts + OPB->_int);
		OPC->vector[0] = (ptr->vector[0] += OPA->vector[0]);
		OPC->vector[1] = (ptr->vector[1] += OPA->vector[1]);
		OPC->vector[2] = (ptr->vector[2] += OPA->vector[2]);
		break;

	case OP_SUBSTORE_F:	// f -= f
		OPB->_float -= OPA->_float;
		break;
	case OP_SUBSTORE_V:	// v -= v
		OPB->vector[0] -= OPA->vector[0];
		OPB->vector[1] -= OPA->vector[1];
		OPB->vector[2] -= OPA->vector[2];
		break;
	case OP_SUBSTOREP_F:	// e.f -= f
		ptr = (eval_t *)((byte *)sv.edicts + OPB->_int);
		OPC->_float = (ptr->_float -= OPA->_float);
		break;
	case OP_SUBSTOREP_V:	// e.v -= v
		ptr = (eval_t *)((byte *)sv.edicts + OPB->_int);
		OPC->vector[0] = (ptr->vector[0] -= OPA->vector[0]);
		OPC->vector[1] = (ptr->vector[1] -= OPA->vector[1]);
		OPC->vector[2] = (ptr->vector[2] -= OPA->vector[2]);
		break;

	case OP_ADDRESS:
		ed = PROG_TO_EDICT(OPA->edict);
#ifdef PARANOID
		NUM_FOR_EDICT(ed);	// Make sure it's in range
#endif
		if (ed == (edict_t *)sv.edicts && sv.state == ss_active)
		{
			pr_xstatement = st - pr_statements;
			PR_RunError("assignment to world entity");
		}
		OPC->_int = (byte *)((int *)&ed->v + OPB->_int) - (byte *)sv.edicts;
		break;

	case OP_LOAD_F:
	case OP_LOAD_FLD:
	case OP_LOAD_ENT:
	case OP_LOAD_S:
	case OP_LOAD_FNC:
		ed = PROG_TO_EDICT(OPA->edict);
#ifdef PARANOID
		NUM_FOR_EDICT(ed);	// Make sure it's in range
#endif
		OPC->_int = ((eval_t *)((int *)&ed->v + OPB->_int))->_int;
		break;

	case OP_LOAD_V:
		ed = PROG_TO_EDICT(OPA->edict);
#ifdef PARANOID
		NUM_FOR_EDICT(ed);	// Make sure it's in range
#endif
		ptr = (eval_t *)((int *)&ed->v + OPB->_int);
		OPC->vector[0] = ptr->vector[0];
		OPC->vector[1] = ptr->vector[1];
		OPC->vector[2] = ptr->vector[2];
		break;

	case OP_FETCH_GBL_F:
	case OP_FETCH_GBL_S:
	case OP_FETCH_GBL_E:
	case OP_FETCH_GBL_FNC:
	  {	int i = (int)OPB->_float;
		if (i < 0 || i > G_INT((unsigned short)st->a - 1))
		{
			pr_xstatement = st - pr_statements;
			PR_RunError("array index out of bounds: %d", i);
		}
		OPC->_int = ((eval_t *)&pr_globals[(unsigned short)st->a + i])->_int;
	  }	break;
	case OP_FETCH_GBL_V:
	  {	int i = (int)OPB->_float;
		if (i < 0 || i > G_INT((unsigned short)st->a - 1))
		{
			pr_xstatement = st - pr_statements;
			PR_RunError("array index out of bounds: %d", i);
		}
		ptr = (eval_t *)&pr_globals[(unsigned short)st->a + (i * 3)];
		OPC->vector[0] = ptr->vector[0];
		OPC->vector[1] = ptr->vector[1];
		OPC->vector[2] = ptr->vector[2];
	  }	break;

	case OP_IFNOT:
		if (!OPA->_int)
		{
			st += st->b - 1;	/* -1 to offset the st++ */
		}
		break;

	case OP_IF:
		if (OPA->_int)
		{
			st += st->b - 1;	/* -1 to offset the st++ */
		}
		break;

	case OP_GOTO:
		st += st->a - 1;		/* -1 to offset the st++ */
		break;

	case OP_CALL8:
	case OP_CALL7:
	case OP_CALL6:
	case OP_CALL5:
	case OP_CALL4:
	case OP_CALL3:
	case OP_CALL2:	// Copy second arg to shared space
		VectorCopy(OPC->vector, G_VECTOR(OFS_PARM1));
	case OP_CALL1:	// Copy first arg to shared space
		VectorCopy(OPB->vector, G_VECTOR(OFS_PARM0));
	case OP_CALL0:
		pr_xfunction->profile += profile - startprofile;
		startprofile = profile;
		pr_xstatement = st - pr_statements;
		pr_argc = st->op - OP_CALL0;
		if (!OPA->function)
		{
			PR_RunError("NULL function");
		}
		newf = &pr_functions[OPA->function];
		if (newf->first_statement < 0)
		{ // Built-in function
			int i = -newf->first_statement;
			if (i >= pr_numbuiltins)
			{
				PR_RunError("Bad builtin call number %d", i);
			}
			pr_builtins[i]();
			break;
		}
		// Normal function
		st = &pr_statements[EnterFunction(newf)];
		break;

	case OP_DONE:
	case OP_RETURN:
		pr_xfunction->profile += profile - startprofile;
		startprofile = profile;
		pr_xstatement = st - pr_statements;
		pr_globals[OFS_RETURN] = pr_globals[(unsigned short)st->a];
		pr_globals[OFS_RETURN + 1] = pr_globals[(unsigned short)st->a + 1];
		pr_globals[OFS_RETURN + 2] = pr_globals[(unsigned short)st->a + 2];
		st = &pr_statements[LeaveFunction()];
		if (pr_depth == exitdepth)
		{ // Done
			return;
		}
		break;

	case OP_STATE:
		ed = PROG_TO_EDICT(*sv_globals.self);
/* Id 1.07 changes
#ifdef FPS_20
		ed->v.nextthink = *sv_globals.time + 0.05;
#else
		ed->v.nextthink = *sv_globals.time + 0.1;
#endif
*/
		ed->v.nextthink = *sv_globals.time + HX_FRAME_TIME;
		ed->v.frame = OPA->_float;
		ed->v.think = OPB->function;
		break;

	case OP_CSTATE:	// Cycle state
	  {	int startFrame, endFrame;
		ed = PROG_TO_EDICT(*sv_globals.self);
		ed->v.nextthink = *sv_globals.time + HX_FRAME_TIME;
		ed->v.think = pr_xfunction - pr_functions;
		*sv_globals.cycle_wrapped = false;
		startFrame = (int)OPA->_float;
		endFrame = (int)OPB->_float;
		if (startFrame <= endFrame)
		{ // Increment
			if (ed->v.frame < startFrame || ed->v.frame > endFrame)
			{
				ed->v.frame = startFrame;
			}
			else
			{
				ed->v.frame++;
				if (ed->v.frame > endFrame)
				{
					*sv_globals.cycle_wrapped = true;
					ed->v.frame = startFrame;
				}
			}
		}
		else
		{ // Decrement
			if (ed->v.frame > startFrame || ed->v.frame < endFrame)
			{
				ed->v.frame = startFrame;
			}
			else
			{
				ed->v.frame--;
				if (ed->v.frame < endFrame)
				{
					*sv_globals.cycle_wrapped = true;
					ed->v.frame = startFrame;
				}
			}
		}
	  }	break;

	case OP_CWSTATE:	// Cycle weapon state
	  {	int startFrame, endFrame;
		ed = PROG_TO_EDICT(*sv_globals.self);
		ed->v.nextthink = *sv_globals.time + HX_FRAME_TIME;
		ed->v.think = pr_xfunction - pr_functions;
		*sv_globals.cycle_wrapped = false;
		startFrame = (int)OPA->_float;
		endFrame = (int)OPB->_float;
		if (startFrame <= endFrame)
		{ // Increment
			if (ed->v.weaponframe < startFrame
				|| ed->v.weaponframe > endFrame)
			{
				ed->v.weaponframe = startFrame;
			}
			else
			{
				ed->v.weaponframe++;
				if (ed->v.weaponframe > endFrame)
				{
					*sv_globals.cycle_wrapped = true;
					ed->v.weaponframe = startFrame;
				}
			}
		}
		else
		{ // Decrement
			if (ed->v.weaponframe > startFrame
				|| ed->v.weaponframe < endFrame)
			{
				ed->v.weaponframe = startFrame;
			}
			else
			{
				ed->v.weaponframe--;
				if (ed->v.weaponframe < endFrame)
				{
					*sv_globals.cycle_wrapped = true;
					ed->v.weaponframe = startFrame;
				}
			}
		}
	  }	break;

	case OP_THINKTIME:
		ed = PROG_TO_EDICT(OPA->edict);
#ifdef PARANOID
		NUM_FOR_EDICT(ed);	// Make sure it's in range
#endif
		if (ed == (edict_t *)sv.edicts && sv.state == ss_active)
		{
			pr_xstatement = st - pr_statements;
			PR_RunError("assignment to world entity");
		}
		ed->v.nextthink = *sv_globals.time + OPB->_float;
		break;

	case OP_BITSET:		// f (+) f
		OPB->_float = (int)OPB->_float | (int)OPA->_float;
		break;
	case OP_BITSETP:	// e.f (+) f
		ptr = (eval_t *)((byte *)sv.edicts + OPB->_int);
		ptr->_float = (int)ptr->_float | (int)OPA->_float;
		break;
	case OP_BITCLR:		// f (-) f
		OPB->_float = (int)OPB->_float & ~((int)OPA->_float);
		break;
	case OP_BITCLRP:	// e.f (-) f
		ptr = (eval_t *)((byte *)sv.edicts + OPB->_int);
		ptr->_float = (int)ptr->_float & ~((int)OPA->_float);
		break;

	case OP_RAND0:
	  {	float val;
		val = (rand() & 0x7fff) / ((float)0x7fff);
		G_FLOAT(OFS_RETURN) = val;
	  }	break;
	case OP_RAND1:
	  {	float val;
		val = (rand() & 0x7fff) / ((float)0x7fff) * OPA->_float;
		G_FLOAT(OFS_RETURN) = val;
	  }	break;
	case OP_RAND2:
	  {	float val;
		if (OPA->_float < OPB->_float)
		{
			val = OPA->_float + ((rand() & 0x7fff) / ((float)0x7fff) * (OPB->_float - OPA->_float));
		}
		else
		{
			val = OPB->_float + ((rand() & 0x7fff) / ((float)0x7fff) * (OPA->_float - OPB->_float));
		}
		G_FLOAT(OFS_RETURN) = val;
	  }	break;
	case OP_RANDV0:
	  {	float val;
		val = (rand() & 0x7fff) / ((float)0x7fff);
		G_FLOAT(OFS_RETURN + 0) = val;
		val = (rand() & 0x7fff) / ((float)0x7fff);
		G_FLOAT(OFS_RETURN + 1) = val;
		val = (rand() & 0x7fff) / ((float)0x7fff);
		G_FLOAT(OFS_RETURN + 2) = val;
	  }	break;
	case OP_RANDV1:
	  {	float val;
		val = (rand() & 0x7fff) / ((float)0x7fff) * OPA->vector[0];
		G_FLOAT(OFS_RETURN + 0) = val;
		val = (rand() & 0x7fff) / ((float)0x7fff) * OPA->vector[1];
		G_FLOAT(OFS_RETURN + 1) = val;
		val = (rand() & 0x7fff) / ((float)0x7fff) * OPA->vector[2];
		G_FLOAT(OFS_RETURN + 2) = val;
	  }	break;
	case OP_RANDV2:
	  {	float val;
		int	i;
		for (i = 0; i < 3; i++)
		{
			if (OPA->vector[i] < OPB->vector[i])
			{
				val = OPA->vector[i] + ((rand() & 0x7fff) / ((float)0x7fff) * (OPB->vector[i] - OPA->vector[i]));
			}
			else
			{
				val = OPB->vector[i] + (rand() * (1.0 / RAND_MAX) * (OPA->vector[i] - OPB->vector[i]));
			}
			G_FLOAT(OFS_RETURN + i) = val;
		}
	  }	break;
	case OP_SWITCH_F:
		case_type = SWITCH_F;
		switch_float = OPA->_float;
		st += st->b - 1;	/* -1 to offset the st++ */
		break;
	case OP_SWITCH_V:
	case OP_SWITCH_S:
	case OP_SWITCH_E:
	case OP_SWITCH_FNC:
		pr_xstatement = st - pr_statements;
		PR_RunError("%s not done yet!", pr_opnames[st->op]);
		break;

	case OP_CASERANGE:
		if (case_type != SWITCH_F)
		{
			pr_xstatement = st - pr_statements;
			PR_RunError("caserange fucked!");
		}
		if ((switch_float >= OPA->_float) && (switch_float <= OPB->_float))
		{
			st += st->c - 1;		/* -1 to offset the st++ */
		}
		break;
	case OP_CASE:
		switch (case_type)
		{
		case SWITCH_F:
			if (switch_float == OPA->_float)
			{
				st += st->b - 1;	/* -1 to offset the st++ */
			}
			break;
		case SWITCH_V:
		case SWITCH_S:
		case SWITCH_E:
		case SWITCH_FNC:
			pr_xstatement = st - pr_statements;
			PR_RunError("OP_CASE for %s not done yet!",
					pr_opnames[case_type + OP_SWITCH_F - SWITCH_F]);
			break;
		default:
			pr_xstatement = st - pr_statements;
			PR_RunError("fucked case!");
		}
		break;

	default:
		pr_xstatement = st - pr_statements;
		PR_RunError("Bad opcode %i", st->op);
	}
    }	/* end of while(1) loop */
}
#undef OPA
#undef OPB
#undef OPC


//==========================================================================
//
// EnterFunction
//
//==========================================================================

static int EnterFunction (dfunction_t *f)
{
	int	i, j, c, o;

	pr_stack[pr_depth].s = pr_xstatement;
	pr_stack[pr_depth].f = pr_xfunction;
	pr_depth++;
	if (pr_depth >= MAX_STACK_DEPTH)
	{
		PR_RunError("stack overflow");
	}

	// save off any locals that the new function steps on
	c = f->locals;
	if (localstack_used + c > LOCALSTACK_SIZE)
	{
		PR_RunError ("%s: locals stack overflow", __thisfunc__);
	}

	for (i = 0; i < c ; i++)
	{
		localstack[localstack_used + i] = ((int *)pr_globals)[f->parm_start + i];
	}
	localstack_used += c;

	// copy parameters
	o = f->parm_start;
	for (i = 0; i < f->numparms; i++)
	{
		for (j = 0; j < f->parm_size[i]; j++)
		{
			((int *)pr_globals)[o] = ((int *)pr_globals)[OFS_PARM0 + i*3 + j];
			o++;
		}
	}

	pr_xfunction = f;
	return f->first_statement - 1;	// offset the s++
}


//==========================================================================
//
// LeaveFunction
//
//==========================================================================

static int LeaveFunction (void)
{
	int	i, c;

	if (pr_depth <= 0)
	{
		Host_Error("prog stack underflow");
	}

	// Restore locals from the stack
	c = pr_xfunction->locals;
	localstack_used -= c;
	if (localstack_used < 0)
	{
		PR_RunError("%s: locals stack underflow", __thisfunc__);
	}

	for (i = 0; i < c; i++)
	{
		((int *)pr_globals)[pr_xfunction->parm_start + i] = localstack[localstack_used + i];
	}

	// up stack
	pr_depth--;
	pr_xfunction = pr_stack[pr_depth].f;
	return pr_stack[pr_depth].s;
}


//==========================================================================
//
// PR_RunError
//
//==========================================================================

void PR_RunError (const char *error, ...)
{
	va_list	argptr;
	char	string[1024];

	va_start (argptr, error);
	q_vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);

	PrintStatement(pr_statements + pr_xstatement);
	PrintCallHistory();

	fprintf (stderr, "%s\n", string);
	Con_Printf("%s\n", string);

	pr_depth = 0;	// dump the stack so host_error can shutdown functions

	Host_Error("Program error");
}


//==========================================================================
//
// PrintCallHistory
//
//==========================================================================

static void PrintCallHistory (void)
{
	int		i;
	dfunction_t	*f;

	if (pr_depth == 0)
	{
		Con_Printf("<NO STACK>\n");
		return;
	}

	pr_stack[pr_depth].f = pr_xfunction;
	for (i = pr_depth; i >= 0; i--)
	{
		f = pr_stack[i].f;
		if (!f)
		{
			Con_Printf("<NO FUNCTION>\n");
		}
		else
		{
			Con_Printf("%12s : %s\n", PR_GetString(f->s_file), PR_GetString(f->s_name));
		}
	}
}


//==========================================================================
//
// PrintStatement
//
//==========================================================================

static void PrintStatement (dstatement_t *s)
{
	int	i;

	if ((unsigned int)s->op < sizeof(pr_opnames)/sizeof(pr_opnames[0]))
	{
		Con_Printf("%s ", pr_opnames[s->op]);
		i = strlen(pr_opnames[s->op]);
		for ( ; i < 10; i++)
		{
			Con_Printf(" ");
		}
	}

	if (s->op == OP_IF || s->op == OP_IFNOT)
	{
		Con_Printf("%sbranch %i", PR_GlobalString(s->a), s->b);
	}
	else if (s->op == OP_GOTO)
	{
		Con_Printf("branch %i", s->a);
	}
	else if ((unsigned int)(s->op-OP_STORE_F) < 6)
	{
		Con_Printf("%s", PR_GlobalString(s->a));
		Con_Printf("%s", PR_GlobalStringNoContents(s->b));
	}
	else
	{
		if (s->a)
		{
			Con_Printf("%s", PR_GlobalString(s->a));
		}
		if (s->b)
		{
			Con_Printf("%s", PR_GlobalString(s->b));
		}
		if (s->c)
		{
			Con_Printf("%s", PR_GlobalStringNoContents(s->c));
		}
	}
	Con_Printf("\n");
}


//==========================================================================
//
// PR_Profile_f
//
//==========================================================================

void PR_Profile_f (void)
{
	int		i, j;
	int		pmax;
	dfunction_t	*f, *bestFunc;
	int		total;
	int		funcCount;
	qboolean	byHC;
	const char	*saveName = NULL;
	FILE	*saveFile = NULL;
	int		currentFile;
	int		bestFile;
	int		tally;
	const char	*s;

	if (sv.state != ss_active)
		return;

	byHC = false;
	funcCount = 10;
	for (i = 1; i < Cmd_Argc(); i++)
	{
		s = Cmd_Argv(i);
		if (*s == 'h' || *s == 'H')
		{ // Sort by HC source file
			byHC = true;
		}
		else if (*s == 's' || *s == 'S')
		{ // Save to file
			if (i + 1 < Cmd_Argc() && !isdigit(*Cmd_Argv(i + 1)))
			{
				i++;
				saveName = FS_MakePath(FS_USERDIR, NULL, Cmd_Argv(i));
			}
			else
			{
				saveName = FS_MakePath(FS_USERDIR, NULL, "profile.txt");
			}
		}
		else if (isdigit(*s))
		{ // Specify function count
			funcCount = atoi(Cmd_Argv(i));
			if (funcCount < 1)
			{
				funcCount = 1;
			}
		}
	}

	total = 0;
	for (i = 0; i < progs->numfunctions; i++)
	{
		total += pr_functions[i].profile;
	}

	if (saveName)
	{ // Create the output file
		saveFile = fopen(saveName, "w");
		if (saveFile == NULL)
			Con_Printf("Could not open %s\n", saveName);
	}

	if (byHC == false)
	{
		j = 0;
		do
		{
			pmax = 0;
			bestFunc = NULL;
			for (i = 0; i < progs->numfunctions; i++)
			{
				f = &pr_functions[i];
				if (f->profile > pmax)
				{
					pmax = f->profile;
					bestFunc = f;
				}
			}
			if (bestFunc)
			{
				if (j < funcCount)
				{
					if (saveFile)
					{
						fprintf(saveFile, "%05.2f %s\n",
								((float)bestFunc->profile / (float)total) * 100.0,
								PR_GetString(bestFunc->s_name));
					}
					else
					{
						Con_Printf("%05.2f %s\n",
								((float)bestFunc->profile / (float)total) * 100.0,
								PR_GetString(bestFunc->s_name));
					}
				}
				j++;
				bestFunc->profile = 0;
			}
		} while (bestFunc);

		if (saveFile)
		{
			fclose(saveFile);
		}
		return;
	}

	currentFile = -1;
	do
	{
		tally = 0;
		bestFile = Q_MAXINT;
		for (i = 0; i < progs->numfunctions; i++)
		{
			if (pr_functions[i].s_file > currentFile
				&& pr_functions[i].s_file < bestFile)
			{
				bestFile = pr_functions[i].s_file;
				tally = pr_functions[i].profile;
				continue;
			}
			if (pr_functions[i].s_file == bestFile)
			{
				tally += pr_functions[i].profile;
			}
		}
		currentFile = bestFile;
		if (tally && currentFile != Q_MAXINT)
		{
			if (saveFile)
			{
				fprintf(saveFile, "\"%s\"\n", PR_GetString(currentFile));
			}
			else
			{
				Con_Printf("\"%s\"\n", PR_GetString(currentFile));
			}

			j = 0;
			do
			{
				pmax = 0;
				bestFunc = NULL;
				for (i = 0; i < progs->numfunctions; i++)
				{
					f = &pr_functions[i];
					if (f->s_file == currentFile && f->profile > pmax)
					{
						pmax = f->profile;
						bestFunc = f;
					}
				}
				if (bestFunc)
				{
					if (j < funcCount)
					{
						if (saveFile)
						{
							fprintf(saveFile, "   %05.2f %s\n",
									((float)bestFunc->profile / (float)total) * 100.0,
									PR_GetString(bestFunc->s_name));
						}
						else
						{
							Con_Printf("   %05.2f %s\n",
									((float)bestFunc->profile / (float)total) * 100.0,
									PR_GetString(bestFunc->s_name));
						}
					}
					j++;
					bestFunc->profile = 0;
				}
			} while (bestFunc);
		}
	} while (currentFile != Q_MAXINT);

	if (saveFile)
	{
		fclose(saveFile);
	}
}

