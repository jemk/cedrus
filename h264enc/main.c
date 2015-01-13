/*
 * Copyright (c) 2014 Jens Kuske <jenskuske@gmail.com>
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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "ve.h"
#include "h264enc.h"

static int read_frame(int fd, void *buffer, int size)
{
	int total = 0, len;
	while (total < size)
	{
		len = read(fd, buffer + total, size - total);
		if (len == 0)
			return 0;
		total += len;
	}
	return 1;
}

int main(const int argc, const char **argv)
{
	if (argc != 5)
	{
		printf("Usage: %s <infile> <width> <height> <outfile>\n", argv[0]);
		return EXIT_FAILURE;
	}

	int width = atoi(argv[2]);
	int height = atoi(argv[3]);

	if (!ve_open())
		return EXIT_FAILURE;

	int in = 0, out;
	if (strcmp(argv[1], "-") != 0)
	{
		if ((in = open(argv[1], O_RDONLY)) == -1)
		{
			printf("could not open input file\n");
			return EXIT_FAILURE;
		}
	}

	if ((out = open(argv[4], O_CREAT | O_RDWR | O_TRUNC,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1)
	{
		printf("could not open output file\n");
		return EXIT_FAILURE;
	}

	struct h264enc_params params;
	params.src_width = (width + 15) & ~15;
	params.width = width;
	params.src_height = (height + 15) & ~15;
	params.height = height;
	params.profile_idc = 77;
	params.level_idc = 41;
	params.entropy_coding_mode = H264_EC_CABAC;
	params.qp = 24;
	params.keyframe_interval = 25;

	h264enc *encoder = h264enc_new(&params);
	if (encoder == NULL)
	{
		printf("could not create encoder\n");
		goto err;
	}

	void* output_buf = h264enc_get_bytestream_buffer(encoder);

	int input_size = params.src_width * (params.src_height + params.src_height / 2);
	void* input_buf = h264enc_get_input_buffer(encoder);

	while (read_frame(in, input_buf, input_size))
	{
		if (h264enc_encode_picture(encoder))
			write(out, output_buf, h264enc_get_bytestream_length(encoder));
		else
			printf("encoding error\n");
	}

	h264enc_free(encoder);

err:
	ve_close();
	close(out);
	close(in);

	return EXIT_SUCCESS;
}
