/****************************************************************************
 *
 *  Filename: cpia2patch.h
 *
 *  Copyright 2001, STMicrolectronics, Inc.
 *
 *  Contact:  steve.miller@st.com
 *
 *  Description:
 *     This file contains patch data for the CPiA2 (stv0672) VP4.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ****************************************************************************/

#ifndef CPIA2_PATCH_HEADER
#define CPIA2_PATCH_HEADER

typedef struct {
	unsigned char reg;
	unsigned char count;
	const unsigned char *data;
} cpia2_patch;

static const unsigned char start_address_hi[1] = {
	0x01
};

static const unsigned char start_address_lo[1] = {
	0xBC
};

static const unsigned char patch_block0[64] = {
	0xE3, 0x02, 0xE3, 0x03, 0xE3, 0x04, 0xE3, 0x05,
	0xE3, 0x06, 0xE3, 0x07, 0x93, 0x44, 0x56, 0xD4,
	0x93, 0x4E, 0x56, 0x51, 0x93, 0x4E, 0x51, 0xD6,
	0x93, 0x4E, 0x4F, 0x54, 0x93, 0x4E, 0x92, 0x4F,
	0x92, 0xA4, 0x93, 0x05, 0x92, 0xF4, 0x93, 0x1B,
	0x92, 0x92, 0x91, 0xE6, 0x92, 0x36, 0x92, 0x74,
	0x92, 0x4A, 0x92, 0x8C, 0x92, 0x8E, 0xC8, 0xD0,
	0x0B, 0x42, 0x02, 0xA0, 0xCA, 0x92, 0x09, 0x02
};

static const unsigned char patch_block1[64] = {
	0xC9, 0x10, 0x0A, 0x0A, 0x0A, 0x81, 0xE3, 0xB8,
	0xE3, 0xB0, 0xE3, 0xA8, 0xE3, 0xA0, 0xE3, 0x98,
	0xE3, 0x90, 0xE1, 0x00, 0xCF, 0xD7, 0x0A, 0x12,
	0xCC, 0x95, 0x08, 0xB2, 0x0A, 0x18, 0xE1, 0x00,
	0x01, 0xEE, 0x0C, 0x08, 0x4A, 0x12, 0xC8, 0x18,
	0xF0, 0x9A, 0xC0, 0x22, 0xF3, 0x1C, 0x4A, 0x13,
	0xF3, 0x14, 0xC8, 0xA0, 0xF2, 0x14, 0xF2, 0x1C,
	0xEB, 0x13, 0xD3, 0xA2, 0x63, 0x16, 0x48, 0x9E
};

static const unsigned char patch_block2[64] = {
	0xF0, 0x18, 0xA4, 0x03, 0xF3, 0x93, 0xC0, 0x58,
	0xF7, 0x13, 0x51, 0x9C, 0xE9, 0x20, 0xCF, 0xEF,
	0x63, 0xF9, 0x92, 0x2E, 0xD3, 0x5F, 0x63, 0xFA,
	0x92, 0x2E, 0xD3, 0x67, 0x63, 0xFB, 0x92, 0x2E,
	0xD3, 0x6F, 0xE9, 0x1A, 0x63, 0x16, 0x48, 0xA7,
	0xF0, 0x20, 0xA4, 0x06, 0xF3, 0x94, 0xC0, 0x27,
	0xF7, 0x14, 0xF5, 0x13, 0x51, 0x9D, 0xF6, 0x13,
	0x63, 0x18, 0xC4, 0x20, 0xCB, 0xEF, 0x63, 0xFC
};

static const unsigned char patch_block3[64] = {
	0x92, 0x2E, 0xD3, 0x77, 0x63, 0xFD, 0x92, 0x2E,
	0xD3, 0x7F, 0x63, 0xFE, 0x92, 0x2E, 0xD3, 0x87,
	0x63, 0xFF, 0x92, 0x2E, 0xD3, 0x8F, 0x64, 0x38,
	0x92, 0x2E, 0xD3, 0x97, 0x64, 0x39, 0x92, 0x2E,
	0xD3, 0x9F, 0xE1, 0x00, 0xF5, 0x3A, 0xF4, 0x3B,
	0xF7, 0xBF, 0xF2, 0xBC, 0xF2, 0x3D, 0xE1, 0x00,
	0x80, 0x87, 0x90, 0x80, 0x51, 0xD5, 0x02, 0x22,
	0x02, 0x32, 0x4B, 0xD3, 0xF7, 0x11, 0x0B, 0xDA
};

static const unsigned char patch_block4[64] = {
	0xE1, 0x00, 0x0E, 0x02, 0x02, 0x40, 0x0D, 0xB5,
	0xE3, 0x02, 0x48, 0x55, 0xE5, 0x12, 0xA4, 0x01,
	0xE8, 0x1B, 0xE3, 0x90, 0xF0, 0x18, 0xA4, 0x01,
	0xE8, 0xBF, 0x8D, 0xB8, 0x4B, 0xD1, 0x4B, 0xD8,
	0x0B, 0xCB, 0x0B, 0xC2, 0xE1, 0x00, 0xE3, 0x02,
	0xE3, 0x03, 0x52, 0xD3, 0x60, 0x59, 0xE6, 0x93,
	0x0D, 0x22, 0x52, 0xD4, 0xE6, 0x93, 0x0D, 0x2A,
	0xE3, 0x98, 0xE3, 0x90, 0xE1, 0x00, 0x02, 0x5D
};

static const unsigned char patch_block5[64] = {
	0x02, 0x63, 0xE3, 0x02, 0xC8, 0x12, 0x02, 0xCA,
	0xC8, 0x52, 0x02, 0xC2, 0x82, 0x68, 0xE3, 0x02,
	0xC8, 0x14, 0x02, 0xCA, 0xC8, 0x90, 0x02, 0xC2,
	0x0A, 0xD0, 0xC9, 0x93, 0x0A, 0xDA, 0xCC, 0xD2,
	0x0A, 0xE2, 0x63, 0x12, 0x02, 0xDA, 0x0A, 0x98,
	0x0A, 0xA0, 0x0A, 0xA8, 0xE3, 0x90, 0xE1, 0x00,
	0xE3, 0x02, 0x0A, 0xD0, 0xC9, 0x93, 0x0A, 0xDA,
	0xCC, 0xD2, 0x0A, 0xE2, 0x63, 0x12, 0x02, 0xDA
};

static const unsigned char patch_block6[64] = {
	0x0A, 0x98, 0x0A, 0xA0, 0x0A, 0xA8, 0x49, 0x91,
	0xE5, 0x6A, 0xA4, 0x04, 0xC8, 0x12, 0x02, 0xCA,
	0xC8, 0x52, 0x82, 0x89, 0xC8, 0x14, 0x02, 0xCA,
	0xC8, 0x90, 0x02, 0xC2, 0xE3, 0x90, 0xE1, 0x00,
	0x08, 0x60, 0xE1, 0x00, 0x48, 0x53, 0xE8, 0x97,
	0x08, 0x5A, 0xE1, 0x00, 0xE3, 0x02, 0xE3, 0x03,
	0x54, 0xD3, 0x60, 0x59, 0xE6, 0x93, 0x0D, 0x52,
	0xE3, 0x98, 0xE3, 0x90, 0xE1, 0x00, 0x02, 0x9C
};

static const unsigned char patch_block7[64] = {
	0xE3, 0x02, 0x55, 0x13, 0x93, 0x17, 0x55, 0x13,
	0x93, 0x17, 0xE3, 0x90, 0xE1, 0x00, 0x75, 0x30,
	0xE3, 0x02, 0xE3, 0x03, 0x55, 0x55, 0x60, 0x59,
	0xE6, 0x93, 0x0D, 0xB2, 0xE3, 0x98, 0xE3, 0x90,
	0xE1, 0x00, 0x02, 0xAE, 0xE7, 0x92, 0xE9, 0x18,
	0xEA, 0x9A, 0xE8, 0x98, 0xE8, 0x10, 0xE8, 0x11,
	0xE8, 0x51, 0xD2, 0xDA, 0xD2, 0xF3, 0xE8, 0x13,
	0xD2, 0xFA, 0xE8, 0x50, 0xD2, 0xEA, 0xE8, 0xD0
};

static const unsigned char patch_block8[64] = {
	0xE8, 0xD1, 0xD3, 0x0A, 0x03, 0x09, 0x48, 0x23,
	0xE5, 0x2C, 0xA0, 0x03, 0x48, 0x24, 0xEA, 0x1C,
	0x03, 0x08, 0xD2, 0xE3, 0xD3, 0x03, 0xD3, 0x13,
	0xE1, 0x00, 0x02, 0xCB, 0x05, 0x93, 0x57, 0x93,
	0xF0, 0x9A, 0xAC, 0x0B, 0xE3, 0x07, 0x92, 0xEA,
	0xE2, 0x9F, 0xE5, 0x06, 0xE3, 0xB0, 0xA0, 0x02,
	0xEB, 0x1E, 0x82, 0xD7, 0xEA, 0x1E, 0xE2, 0x3B,
	0x85, 0x9B, 0xE9, 0x1E, 0xC8, 0x90, 0x85, 0x94
};

static const unsigned char patch_block9[64] = {
	0x02, 0xDE, 0x05, 0x80, 0x57, 0x93, 0xF0, 0xBA,
	0xAC, 0x06, 0x92, 0xEA, 0xE2, 0xBF, 0xE5, 0x06,
	0xA0, 0x01, 0xEB, 0xBF, 0x85, 0x88, 0xE9, 0x3E,
	0xC8, 0x90, 0x85, 0x81, 0xE9, 0x3E, 0xF0, 0xBA,
	0xF3, 0x39, 0xF0, 0x3A, 0x60, 0x17, 0xF0, 0x3A,
	0xC0, 0x90, 0xF0, 0xBA, 0xE1, 0x00, 0x00, 0x3F,
	0xE3, 0x02, 0xE3, 0x03, 0x58, 0x10, 0x60, 0x59,
	0xE6, 0x93, 0x0D, 0xA2, 0x58, 0x12, 0xE6, 0x93
};

static const unsigned char patch_block10[64] = {
	0x0D, 0xAA, 0xE3, 0x98, 0xE3, 0x90, 0xE1, 0x00,
	0x03, 0x01, 0xE1, 0x00, 0x03, 0x03, 0x9B, 0x7D,
	0x8B, 0x8B, 0xE3, 0x02, 0xE3, 0x03, 0x58, 0x56,
	0x60, 0x59, 0xE6, 0x93, 0x0D, 0xBA, 0xE3, 0x98,
	0xE3, 0x90, 0xE1, 0x00, 0x03, 0x0F, 0x93, 0x11,
	0xE1, 0x00, 0xE3, 0x02, 0x4A, 0x11, 0x0B, 0x42,
	0x91, 0xAF, 0xE3, 0x90, 0xE1, 0x00, 0xF2, 0x91,
	0xF0, 0x91, 0xA3, 0xFE, 0xE1, 0x00, 0x60, 0x92
};

static const unsigned char patch_block11[64] = {
	0xC0, 0x5F, 0xF0, 0x13, 0xF0, 0x13, 0x59, 0x5B,
	0xE2, 0x13, 0xF0, 0x11, 0x5A, 0x19, 0xE2, 0x13,
	0xE1, 0x00, 0x00, 0x00, 0x03, 0x27, 0x68, 0x61,
	0x76, 0x61, 0x6E, 0x61, 0x00, 0x06, 0x03, 0x2C,
	0xE3, 0x02, 0xE3, 0x03, 0xE9, 0x38, 0x59, 0x15,
	0x59, 0x5A, 0xF2, 0x9A, 0xBC, 0x0B, 0xA4, 0x0A,
	0x59, 0x1E, 0xF3, 0x11, 0xF0, 0x1A, 0xE2, 0xBB,
	0x59, 0x15, 0xF0, 0x11, 0x19, 0x2A, 0xE5, 0x02
};

static const unsigned char patch_block12[54] = {
	0xA4, 0x01, 0xEB, 0xBF, 0xE3, 0x98, 0xE3, 0x90,
	0xE1, 0x00, 0x03, 0x42, 0x19, 0x28, 0xE1, 0x00,
	0xE9, 0x30, 0x60, 0x79, 0xE1, 0x00, 0xE3, 0x03,
	0xE3, 0x07, 0x60, 0x79, 0x93, 0x4E, 0xE3, 0xB8,
	0xE3, 0x98, 0xE1, 0x00, 0xE9, 0x1A, 0xF0, 0x1F,
	0xE2, 0x33, 0xF0, 0x91, 0xE2, 0x92, 0xE0, 0x32,
	0xF0, 0x31, 0xE1, 0x00, 0x00, 0x00
};

static const unsigned char do_call[1] = {
	0x01
};


#define PATCH_DATA_SIZE 18

static const cpia2_patch patch_data[PATCH_DATA_SIZE] = {
	{0x0A, sizeof(start_address_hi), start_address_hi}
	,			// 0
	{0x0B, sizeof(start_address_lo), start_address_lo}
	,			// 1
	{0x0C, sizeof(patch_block0), patch_block0}
	,			// 2
	{0x0C, sizeof(patch_block1), patch_block1}
	,			// 3
	{0x0C, sizeof(patch_block2), patch_block2}
	,			// 4
	{0x0C, sizeof(patch_block3), patch_block3}
	,			// 5
	{0x0C, sizeof(patch_block4), patch_block4}
	,			// 6
	{0x0C, sizeof(patch_block5), patch_block5}
	,			// 7
	{0x0C, sizeof(patch_block6), patch_block6}
	,			// 8
	{0x0C, sizeof(patch_block7), patch_block7}
	,			// 9
	{0x0C, sizeof(patch_block8), patch_block8}
	,			// 10
	{0x0C, sizeof(patch_block9), patch_block9}
	,			//11
	{0x0C, sizeof(patch_block10), patch_block10}
	,			// 12
	{0x0C, sizeof(patch_block11), patch_block11}
	,			// 13
	{0x0C, sizeof(patch_block12), patch_block12}
	,			// 14
	{0x0A, sizeof(start_address_hi), start_address_hi}
	,			// 15
	{0x0B, sizeof(start_address_lo), start_address_lo}
	,			// 16
	{0x0D, sizeof(do_call), do_call}	//17
};


#endif
