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

#include <stdio.h>
#include <stdint.h>
#include "mpeg.h"

static int parse_sequence_header(struct mpeg_t * const mpeg)
{
	const uint8_t * const data = mpeg->data + mpeg->pos;

	if ((mpeg->pos += 8) > mpeg->len)
		return 0;

	mpeg->width = (data[0] << 4) | (data[1] >> 4);
	mpeg->height = ((data[1] & 0x0f) << 8) | data[2];
	//ignore

	return 1;
}

static int parse_extension(struct mpeg_t * const mpeg)
{
	const uint8_t * const data = mpeg->data + mpeg->pos;

	if (mpeg->pos > mpeg->len)
		return 0;

	switch (data[0] >> 4)
	{
	case 0x1:
		if ((mpeg->pos += 6)> mpeg->len)
			return 0;

		//ignore
		mpeg->width |= ((data[1] & 0x01) << 13) | ((data[2] & 0x80) << 5);
		mpeg->height |= ((data[2] & 0x60) << 7);
		//ignore

		return 1;
	case 0x2:
		//ignore seq_display_ext;
		return 1;
	case 0x8:
		if ((mpeg->pos += 5)> mpeg->len)
			return 0;

		mpeg->f_code[0][0] = (data[0] & 0x0f);
		mpeg->f_code[0][1] = ((data[1] & 0xf0) >> 4);
		mpeg->f_code[1][0] = (data[1] & 0x0f);
		mpeg->f_code[1][1] = ((data[2] & 0xf0) >> 4);
		mpeg->intra_dc_precision = ((data[2] & 0x0c) >> 2);
		mpeg->picture_structure = (data[2] & 0x03);
		mpeg->top_field_first = ((data[3] & 0x80) >> 7);
		mpeg->frame_pred_frame_dct = ((data[3] & 0x40) >> 6);
		mpeg->concealment_motion_vectors = ((data[3] & 0x20) >> 5);
		mpeg->q_scale_type = ((data[3] & 0x10) >> 4);
		mpeg->intra_vlc_format = ((data[3] & 0x08) >> 3);
		mpeg->alternate_scan = ((data[3] & 0x04) >> 2);
		//ignore rest
		return 1;
	default:
		fprintf(stderr, "unhandled extension: %01x\n", data[0] >> 4);
		break;
	}

	return 0;
}

static int parse_gop(struct mpeg_t * const mpeg)
{
	//const uint8_t * const data = mpeg->data + mpeg->pos;

	if ((mpeg->pos += 4) > mpeg->len)
		return 0;

	mpeg->gop++;
	// ignore data
	return 1;
}

static int parse_picture(struct mpeg_t * const mpeg)
{
	const uint8_t * const data = mpeg->data + mpeg->pos;

	if ((mpeg->pos += 4) > mpeg->len)
		return 0;

	mpeg->temporal_reference = (data[0] << 2) | (data[1] >> 6);
	mpeg->picture_coding_type = ((data[1] & 0x38) >> 3);
	if (mpeg->picture_coding_type == 2 || mpeg->picture_coding_type == 3)
	{
		if (mpeg->pos++ > mpeg->len)
			return 0;

		mpeg->full_pel_forward_vector = ((data[3] & 0x04) >> 2);
		mpeg->f_code[0][0] = mpeg->f_code[0][1] = ((data[3] & 0x03) << 1) | ((data[4] & 0x80) >> 7);

		if (mpeg->picture_coding_type == 3)
		{
			mpeg->full_pel_backward_vector = ((data[4] & 0x40) >> 6);
			mpeg->f_code[1][0] = mpeg->f_code[1][1] = ((data[4] & 0x38) >> 3);
		}
	}
	else
	{
		mpeg->full_pel_forward_vector = mpeg->full_pel_backward_vector = 0;
		mpeg->f_code[0][0] = mpeg->f_code[0][1] = 0;
		mpeg->f_code[1][0] = mpeg->f_code[1][1] = 0;
	}

	// ignore extra data

	mpeg->picture_structure = 0x3;
	mpeg->top_field_first = 0x1;
	mpeg->frame_pred_frame_dct = 0x1;

	return 1;
}

int parse_mpeg(struct mpeg_t * const mpeg)
{
	while (mpeg->pos < mpeg->len)
	{
		int zeros = 0;
		for ( ; mpeg->pos < mpeg->len; mpeg->pos++)
		{
			if (mpeg->data[mpeg->pos] == 0x00)
				zeros++;
			else if (mpeg->data[mpeg->pos] == 0x01 && zeros >= 2)
			{
				mpeg->pos++;
				break;
			}
			else
				zeros = 0;
		}

		uint8_t marker = mpeg->data[mpeg->pos++];

		switch (marker)
		{
		case 0x00:
			if (!parse_picture(mpeg))
				return 0;
			break;
		case 0xb3:
			if (!parse_sequence_header(mpeg))
				return 0;
			break;
		case 0xb5:
			if (!parse_extension(mpeg))
				return 0;
			break;
		case 0xb8:
			if (!parse_gop(mpeg))
				return 0;
			break;		
		default:
			if (marker >= 0x01 && marker <= 0xaf)
				return 1;

			fprintf(stderr, "unhandled startcode: %02x\n", marker);
			break;
		}
	}
	return 0;
}
