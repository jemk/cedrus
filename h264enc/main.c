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

static void put_bits(void* regs, uint32_t x, int num)
{
	writel(x, regs + VE_AVC_BASIC_BITS);
	writel(0x1 | ((num & 0x1f) << 8), regs + VE_AVC_TRIGGER);
	// again the problem, how to check for finish?
}

static void put_ue(void* regs, uint32_t x)
{
	x++;
	put_bits(regs, x, (32 - __builtin_clz(x)) * 2 - 1);
}

static void put_se(void* regs, int x)
{
	x = 2 * x - 1;
	x ^= (x >> 31);
	put_ue(regs, x);
}

static void put_start_code(void* regs)
{
	uint32_t tmp = readl(regs + VE_AVC_PARAM);

	// disable emulation_prevention_three_byte
	writel(tmp | (0x1 << 31), regs + VE_AVC_PARAM);

	put_bits(regs, 0, 31);
	put_bits(regs, 1, 1);

	writel(tmp, regs + VE_AVC_PARAM);
}

static void put_rbsp_trailing_bits(void* regs)
{
	unsigned int cur_bs_len = readl(regs + VE_AVC_VLE_LENGTH);

	int num_zero_bits = 8 - ((cur_bs_len + 1) & 0x7);
	put_bits(regs, 1 << num_zero_bits, num_zero_bits + 1);
}

static void put_seq_parameter_set(void* regs, int width, int height)
{
	put_bits(regs, 3 << 5 | 7 << 0, 8);	// NAL Header
	put_bits(regs, 77, 8);			// profile_idc
	put_bits(regs, 0x0, 8);			// constraints
	put_bits(regs, 4 * 10 + 1, 8);		// level_idc
	put_ue(regs, 0);			// seq_parameter_set_id

	put_ue(regs, 0);			// log2_max_frame_num_minus4
	put_ue(regs, 0);			// pic_order_cnt_type
	// if (pic_order_cnt_type == 0)
		put_ue(regs, 4);		// log2_max_pic_order_cnt_lsb_minus4

	put_ue(regs, 1);			// max_num_ref_frames
	put_bits(regs, 0, 1);			// gaps_in_frame_num_value_allowed_flag

	put_ue(regs, width - 1);		// pic_width_in_mbs_minus1
	put_ue(regs, height - 1);		// pic_height_in_map_units_minus1

	put_bits(regs, 1, 1);			// frame_mbs_only_flag
	// if (!frame_mbs_only_flag)

	put_bits(regs, 1, 1);			// direct_8x8_inference_flag
	put_bits(regs, 0, 1);			// frame_cropping_flag
	// if (frame_cropping_flag)

	put_bits(regs, 0, 1);			// vui_parameters_present_flag
	// if (vui_parameters_present_flag)
}

static void put_pic_parameter_set(void *regs)
{
	put_bits(regs, 3 << 5 | 8 << 0, 8);	// NAL Header
	put_ue(regs, 0);			// pic_parameter_set_id
	put_ue(regs, 0);			// seq_parameter_set_id
	put_bits(regs, 1, 1);			// entropy_coding_mode_flag
	put_bits(regs, 0, 1);			// bottom_field_pic_order_in_frame_present_flag
	put_ue(regs, 0);			// num_slice_groups_minus1
	// if (num_slice_groups_minus1 > 0)

	put_ue(regs, 0);			// num_ref_idx_l0_default_active_minus1
	put_ue(regs, 0);			// num_ref_idx_l1_default_active_minus1
	put_bits(regs, 0, 1);			// weighted_pred_flag
	put_bits(regs, 0, 2);			// weighted_bipred_idc
	put_se(regs, 0);			// pic_init_qp_minus26
	put_se(regs, 0);			// pic_init_qs_minus26
	put_se(regs, 4);			// chroma_qp_index_offset
	put_bits(regs, 1, 1);			// deblocking_filter_control_present_flag
	put_bits(regs, 0, 1);			// constrained_intra_pred_flag
	put_bits(regs, 0, 1);			// redundant_pic_cnt_present_flag
}

static void put_slice_header(void* regs)
{
	put_bits(regs, 3 << 5 | 5 << 0, 8);	// NAL Header

	put_ue(regs, 0);			// first_mb_in_slice
	put_ue(regs, 2);			// slice_type
	put_ue(regs, 0);			// pic_parameter_set_id
	put_bits(regs, 0, 4);			// frame_num

	// if (IdrPicFlag)
		put_ue(regs, 0);		// idr_pic_id

	// if (pic_order_cnt_type == 0)
		put_bits(regs, 0, 8);		// pic_order_cnt_lsb

	// dec_ref_pic_marking
		put_bits(regs, 0, 1);		// no_output_of_prior_pics_flag
		put_bits(regs, 0, 1);		// long_term_reference_flag

	put_se(regs, 4);			// slice_qp_delta

	// if (deblocking_filter_control_present_flag)
		put_ue(regs, 0);		// disable_deblocking_filter_idc
		// if (disable_deblocking_filter_idc != 1)
			put_se(regs, 0);	// slice_alpha_c0_offset_div2
			put_se(regs, 0);	// slice_beta_offset_div2
}

static void put_aud(void* regs)
{
	put_bits(regs, 0 << 5 | 9 << 0, 8);	// NAL Header

	put_bits(regs, 7, 3);			// primary_pic_type
}

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
	printf("THIS IS ONLY A PROOF OF CONCEPT, READ THE README!\n\n\n");
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

	int output_size = 1 * 1024 * 1024;
	void* output_buf = ve_malloc(output_size);

	int tile_w = (width + 31) & ~31;
	int tile_w2 = (width / 2 + 31) & ~31;
	int tile_h = (height + 31) & ~31;
	int tile_h2 = (height / 2 + 31) & ~31;
	int mb_w = (width + 15) / 16;
	int mb_h = (height + 15) / 16;

	int plane_size = mb_w * 16 * mb_h * 16;
	void* input_buf = ve_malloc(plane_size + plane_size / 2);

	void* reconstruct_buf = ve_malloc(tile_w * tile_h + tile_w * tile_h2);
	void* small_luma_buf = ve_malloc(tile_w2 * tile_h2);
	void* mb_info_buf = ve_malloc(0x1000);

	// activate AVC engine
	void *ve_regs = ve_get(VE_ENGINE_AVC, 0);

	int frame_num = 0;
	while (read_frame(in, input_buf, plane_size + plane_size / 2))
	{
		ve_flush_cache(input_buf, plane_size + plane_size / 2);

		// flush output buffer, otherwise we might read old cached data
		ve_flush_cache(output_buf, output_size);

		// output buffer
		writel(0x0, ve_regs + VE_AVC_VLE_OFFSET);
		writel(ve_virt2phys(output_buf), ve_regs + VE_AVC_VLE_ADDR);
		writel(ve_virt2phys(output_buf) + output_size - 1, ve_regs + VE_AVC_VLE_END);

		writel(0x04000000, ve_regs + 0xb8c); // ???

		// input size
		writel(mb_w << 16, ve_regs + VE_ISP_INPUT_STRIDE);
		writel((mb_w << 16) | (mb_h << 0), ve_regs + VE_ISP_INPUT_SIZE);

		// input buffer
		writel(ve_virt2phys(input_buf), ve_regs + VE_ISP_INPUT_LUMA);
		writel(ve_virt2phys(input_buf) + plane_size, ve_regs + VE_ISP_INPUT_CHROMA);

		put_start_code(ve_regs);
		put_aud(ve_regs);
		put_rbsp_trailing_bits(ve_regs);

		// reference output
		writel(ve_virt2phys(reconstruct_buf), ve_regs + VE_AVC_REC_LUMA);
		writel(ve_virt2phys(reconstruct_buf) + tile_w * tile_h, ve_regs + VE_AVC_REC_CHROMA);
		writel(ve_virt2phys(small_luma_buf), ve_regs + VE_AVC_REC_SLUMA);
		writel(ve_virt2phys(mb_info_buf), ve_regs + VE_AVC_MB_INFO);

		if (frame_num == 0)
		{
			put_start_code(ve_regs);
			put_seq_parameter_set(ve_regs, mb_w, mb_h);
			put_rbsp_trailing_bits(ve_regs);

			put_start_code(ve_regs);
			put_pic_parameter_set(ve_regs);
			put_rbsp_trailing_bits(ve_regs);
		}

		put_start_code(ve_regs);
		put_slice_header(ve_regs);

		writel(readl(ve_regs + VE_AVC_CTRL) | 0xf, ve_regs + VE_AVC_CTRL);
		writel(readl(ve_regs + VE_AVC_STATUS) | 0x7, ve_regs + VE_AVC_STATUS);

		// parameters
		writel(0x00000100, ve_regs + VE_AVC_PARAM);
		writel(0x00041e1e, ve_regs + VE_AVC_QP);
		writel(0x00000104, ve_regs + VE_AVC_MOTION_EST);

		writel(0x8, ve_regs + VE_AVC_TRIGGER);
		ve_wait(1);

		writel(readl(ve_regs + VE_AVC_STATUS), ve_regs + VE_AVC_STATUS);

		write(out, output_buf, readl(ve_regs + VE_AVC_VLE_LENGTH) / 8);

		frame_num++;
	}

	ve_put();

	ve_free(input_buf);
	ve_free(output_buf);
	ve_free(reconstruct_buf);
	ve_free(small_luma_buf);
	ve_free(mb_info_buf);
	ve_close();
	close(out);
	close(in);

	return EXIT_SUCCESS;
}
