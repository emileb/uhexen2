/*
	r_vars.c
	global refresh variables

	$Id: r_vars.c,v 1.3 2007-03-14 21:03:37 sezero Exp $
*/

#include	"quakedef.h"

#if	!id386

// all global and static refresh variables are collected in a contiguous block
// to avoid cache conflicts.

//-------------------------------------------------------
// global refresh variables
//-------------------------------------------------------

// FIXME: make into one big structure, like cl or sv
// FIXME: do separately for refresh engine and driver

int		r_bmodelactive;

#endif	// !id386

