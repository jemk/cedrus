/*
 * Simple MPEG1/2 parser
 *
 * Copyright (c) 2013 Jens Kuske <jenskuske@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#ifndef __MPEG_H__
#define __MPEG_H__

#include <stdint.h>

struct mpeg_t
{
	const uint8_t *data;
	int len;
	int pos;
	unsigned int gop;
	enum { MPEG1, MPEG2 } type;

	uint16_t width;
	uint16_t height;
	
	uint16_t temporal_reference;
	enum { PCT_I = 1, PCT_P, PCT_B, PCT_D } picture_coding_type;
	uint8_t full_pel_forward_vector;
	uint8_t full_pel_backward_vector;
	uint8_t f_code[2][2];

	uint8_t intra_dc_precision;
	uint8_t picture_structure;
	uint8_t top_field_first;
	uint8_t frame_pred_frame_dct;
	uint8_t concealment_motion_vectors;
	uint8_t q_scale_type;
	uint8_t intra_vlc_format;
	uint8_t alternate_scan;
};

int parse_mpeg(struct mpeg_t * const mpeg);

#endif
