/*
	crc.c
	crc functions

	$Id: crc.h,v 1.1 2007-04-14 21:30:15 sezero Exp $
	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		51 Franklin St, Fifth Floor,
		Boston, MA  02110-1301, USA
*/

#ifndef __HX2_CRC_H
#define __HX2_CRC_H

void CRC_Init(unsigned short *crcvalue);
void CRC_ProcessByte(unsigned short *crcvalue, unsigned char data);
void CRC_ProcessBlock (unsigned char *start, unsigned short *crcvalue, int count);
unsigned short CRC_Value(unsigned short crcvalue);
unsigned short CRC_Block (unsigned char *start, int count);

#endif	/* __HX2_CRC_H */

