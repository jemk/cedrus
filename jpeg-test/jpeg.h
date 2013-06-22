/*
 * Simple JPEG parser (only basic functions)
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

#ifndef __JPEG_H__
#define __JPEG_H__

#include <stdint.h>

struct jpeg_t
{
	uint8_t bits;
	uint16_t width;
	uint16_t height;
	struct comp_t
	{
		uint8_t samp_h;
		uint8_t samp_v;
		uint8_t qt;
		uint8_t ht_ac;
		uint8_t ht_dc;
	} comp[3];
	struct quant_t
	{
		uint8_t coeff[64];
	} *quant[4];
	struct huffman_t
	{
		uint8_t num[16];
		uint8_t codes[256];
	} *huffman[8];
	uint16_t restart_interval;
	uint8_t *data;
	uint32_t data_len;
};

int parse_jpeg(struct jpeg_t *jpeg, const uint8_t *data, const int len);
void dump_jpeg(const struct jpeg_t *jpeg);

#endif
