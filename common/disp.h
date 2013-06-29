/*
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

#ifndef __DISP_H__
#define __DISP_H__

#include <stdint.h>

#define COLOR_YUV420 (0)
#define COLOR_YUV422 (2)

int disp_open(void);
int disp_set_para(const uint32_t luma_buffer, const uint32_t chroma_buffer,
			const int color_format, const int width, const int height,
			const int out_x, const int out_y, const int put_width, const int out_height);
int disp_new_frame(const uint32_t luma_buffer, const uint32_t chroma_buffer,
			const int id, const int frame_rate);
void disp_close(void);

#endif
