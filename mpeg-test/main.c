/*
 * Proof of Concept MPEG1/2 decoder using Allwinners CedarX
 *
 * WARNING: Don't use on "production systems". This was made by reverse
 * engineering and might crash your system or destroy data!
 * It was made only for my personal use of testing the reverse engineered
 * things, so don't expect good code quality. It's far from complete and
 * might crash if the MPEG doesn't fit it's requirements!
 *
 *
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
#include <err.h>
#include <libavformat/avformat.h>
#include "mpeg.h"
#include "ve.h"
#include "disp.h"

static const uint8_t mpeg_default_intra_quant[64] =
{
	 8, 16, 16, 19, 16, 19, 22, 22,
	22, 22, 22, 22, 26, 24, 26, 27,
	27, 27, 26, 26, 26, 26, 27, 27,
	27, 29, 29, 29, 34, 34, 34, 29,
	29, 29, 27, 27, 29, 29, 32, 32,
	34, 34, 37, 38, 37, 35, 35, 34,
	35, 38, 38, 40, 40, 40, 48, 48,
	46, 46, 56, 56, 58, 69, 69, 83
};

static const uint8_t mpeg_default_non_intra_quant[64] =
{
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16
};

static void set_quantization_tables(void * const regs, const uint8_t * const table1, const uint8_t * const table2)
{
	int i;
	for (i = 0; i < 64; i++)
		writel((uint32_t)(64 + i) << 8 | table1[i], regs + VE_MPEG_IQ_MIN_INPUT);
	for (i = 0; i < 64; i++)
		writel((uint32_t)(i) << 8 | table2[i], regs + VE_MPEG_IQ_MIN_INPUT);
}

struct frame_t
{
	void *luma_buffer;
	void *chroma_buffer;
	uint16_t width, height;
	int color;

	int ref_counter;
};

struct frame_buffers_t
{
	struct frame_t *output;
	struct frame_t *backward;
	struct frame_t *forward;
};

void frame_show(struct frame_t *frame)
{
	static int disp_initialized = 0;
	static int frame_id = 0;

	if (!disp_initialized)
	{
		if (!disp_open())
		{
			fprintf(stderr, "Can't open /dev/disp\n");
			return;
		}

		disp_set_para(ve_virt2phys(frame->luma_buffer), ve_virt2phys(frame->chroma_buffer),
			frame->color, frame->width, frame->height,
			0, 0, 800, 600);

		disp_initialized = 1;
	}

	disp_new_frame(ve_virt2phys(frame->luma_buffer), ve_virt2phys(frame->chroma_buffer),
		frame_id++, 24000);
}

struct frame_t *frame_new(uint16_t width, uint16_t height, int color)
{
	int size = ((width + 31) & ~31) * ((height + 31) & ~31);

	struct frame_t *frame = malloc(sizeof(struct frame_t));
	frame->luma_buffer = ve_malloc(size);
	frame->chroma_buffer = ve_malloc(size);

	frame->width = width;
	frame->height = height;
	frame->color = color;

	frame->ref_counter = 1;

	return frame;
}

struct frame_t *frame_ref(struct frame_t *frame)
{
	if (!frame)
		return NULL;

	frame->ref_counter++;

	return frame;
}

void frame_unref(struct frame_t *frame)
{
	if (!frame)
		return;

	frame->ref_counter--;

	if (frame->ref_counter <= 0)
	{
		ve_free(frame->luma_buffer);
		ve_free(frame->chroma_buffer);
		free(frame);
	}
}

void decode_mpeg(struct frame_buffers_t *frame_buffers, const struct mpeg_t * const mpeg)
{
	int input_size = (mpeg->len + 65535) & ~65535;
	uint8_t *input_buffer = ve_malloc(input_size);
	memcpy(input_buffer, mpeg->data, mpeg->len);
	ve_flush_cache(input_buffer, mpeg->len);

	// activate MPEG engine
	void *ve_regs = ve_get(VE_ENGINE_MPEG, 0);

	// set quantisation tables
	set_quantization_tables(ve_regs, mpeg_default_intra_quant, mpeg_default_non_intra_quant);

	// set size
	uint16_t width = (mpeg->width + 15) / 16;
	uint16_t height = (mpeg->height + 15) / 16;
	writel((width << 8) | height, ve_regs + VE_MPEG_SIZE);
	writel(((width * 16) << 16) | (height * 16), ve_regs + VE_MPEG_FRAME_SIZE);

	// set picture header
	uint32_t pic_header = 0x00000000;
	pic_header |= ((mpeg->picture_coding_type & 0xf) << 28);
	pic_header |= ((mpeg->f_code[0][0] & 0xf) << 24);
	pic_header |= ((mpeg->f_code[0][1] & 0xf) << 20);
	pic_header |= ((mpeg->f_code[1][0] & 0xf) << 16);
	pic_header |= ((mpeg->f_code[1][1] & 0xf) << 12);
	pic_header |= ((mpeg->intra_dc_precision & 0x3) << 10);
	pic_header |= ((mpeg->picture_structure & 0x3) << 8);
	pic_header |= ((mpeg->top_field_first & 0x1) << 7);
	pic_header |= ((mpeg->frame_pred_frame_dct & 0x1) << 6);
	pic_header |= ((mpeg->concealment_motion_vectors & 0x1) << 5);
	pic_header |= ((mpeg->q_scale_type & 0x1) << 4);
	pic_header |= ((mpeg->intra_vlc_format & 0x1) << 3);
	pic_header |= ((mpeg->alternate_scan & 0x1) << 2);
	pic_header |= ((mpeg->full_pel_forward_vector & 0x1) << 1);
	pic_header |= ((mpeg->full_pel_backward_vector & 0x1) << 0);
	writel(pic_header, ve_regs + VE_MPEG_PIC_HDR);

	// ??
	writel(0x00000000, ve_regs + 0x100 + 0x10);

	// ??
	writel(0x800001b8, ve_regs + VE_MPEG_CTRL);

	// ??
	writel(0x00000000, ve_regs + 0x100 + 0xc4);

	// ??
	writel(0x00000000, ve_regs + 0x100 + 0xc8);

	// set forward/backward predicion buffers
	if (mpeg->picture_coding_type == PCT_I || mpeg->picture_coding_type == PCT_P)
	{
		frame_unref(frame_buffers->forward);
		frame_buffers->forward = frame_ref(frame_buffers->backward);
		frame_unref(frame_buffers->backward);
		frame_buffers->backward = frame_ref(frame_buffers->output);
	}
	writel(ve_virt2phys(frame_buffers->forward->luma_buffer), ve_regs + VE_MPEG_FWD_LUMA);
	writel(ve_virt2phys(frame_buffers->forward->chroma_buffer), ve_regs + VE_MPEG_FWD_CHROMA);
	writel(ve_virt2phys(frame_buffers->backward->luma_buffer), ve_regs + VE_MPEG_BACK_LUMA);
	writel(ve_virt2phys(frame_buffers->backward->chroma_buffer), ve_regs + VE_MPEG_BACK_CHROMA);

	// set output buffers (Luma / Croma)
	writel(ve_virt2phys(frame_buffers->output->luma_buffer), ve_regs + VE_MPEG_REC_LUMA);
	writel(ve_virt2phys(frame_buffers->output->chroma_buffer), ve_regs + VE_MPEG_REC_CHROMA);
	writel(ve_virt2phys(frame_buffers->output->luma_buffer), ve_regs + VE_MPEG_ROT_LUMA);
	writel(ve_virt2phys(frame_buffers->output->chroma_buffer), ve_regs + VE_MPEG_ROT_CHROMA);

	// set input offset in bits
	writel((mpeg->pos - 4) * 8, ve_regs + VE_MPEG_VLD_OFFSET);

	// set input length in bits (+ little bit more, else it fails sometimes ??)
	writel((mpeg->len - (mpeg->pos - 4) + 16) * 8, ve_regs + VE_MPEG_VLD_LEN);

	// input end
	writel(ve_virt2phys(input_buffer) + input_size - 1, ve_regs + VE_MPEG_VLD_END);

	// set input buffer
	writel(ve_virt2phys(input_buffer) | 0x50000000, ve_regs + VE_MPEG_VLD_ADDR);

	// trigger
	writel((mpeg->type ? 0x02000000 : 0x01000000) | 0x8000000f, ve_regs + VE_MPEG_TRIGGER);

	// wait for interrupt
	ve_wait(1);

	// clean interrupt flag (??)
	writel(0x0000c00f, ve_regs + VE_MPEG_STATUS);

	// stop MPEG engine
	ve_put();

	ve_free(input_buffer);
}

#define RING_BUFFER_SIZE (8)

int main(int argc, char** argv)
{
	AVFormatContext* avfmt_ctx = NULL;
	int video_stream;
	enum AVCodecID video_codec;

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s filename\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	char *filename = argv[1];

	av_register_all();

	if (avformat_open_input(&avfmt_ctx, filename, NULL, NULL) < 0)
	{
		fprintf(stderr, "Could not open source file %s\n", filename);
		exit(1);
	}

	if (avformat_find_stream_info(avfmt_ctx, NULL) < 0)
	{
		fprintf(stderr, "Could not find stream information\n");
		avformat_close_input(&avfmt_ctx);
		exit(1);
	}

	video_stream = av_find_best_stream(avfmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (video_stream < 0)
	{
		fprintf(stderr, "Could not find video stream in input file\n");
		avformat_close_input(&avfmt_ctx);
		exit(1);
	}

	video_codec = avfmt_ctx->streams[video_stream]->codec->codec_id;

	if (video_codec != AV_CODEC_ID_MPEG1VIDEO && video_codec != AV_CODEC_ID_MPEG2VIDEO)
	{
		fprintf(stderr, "Can't handle codec %s\n", avcodec_get_name(video_codec));
		avformat_close_input(&avfmt_ctx);
		exit(1);
	}

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	struct mpeg_t mpeg;
	memset(&mpeg, 0, sizeof(mpeg));
	if (video_codec == AV_CODEC_ID_MPEG1VIDEO)
		mpeg.type = MPEG1;
	else
		mpeg.type = MPEG2;

	if (!ve_open())
		err(EXIT_FAILURE, "Can't open VE");

	struct frame_buffers_t frame_buffers = { NULL, NULL, NULL };

	unsigned int disp_frame = 0, gop_offset = 0, gop_frames = 0, last_gop = 0;
	struct frame_t *frames[RING_BUFFER_SIZE];
	memset(frames, 0, sizeof(frames));

	printf("Playing now... press Enter for next frame!\n");

	while (av_read_frame(avfmt_ctx, &pkt) >= 0)
	{
		mpeg.data = pkt.data;
		mpeg.len = pkt.size;
		mpeg.pos = 0;

		if (pkt.stream_index == video_stream && parse_mpeg(&mpeg))
		{
			// create output buffer
			frame_buffers.output = frame_new(mpeg.width, mpeg.height, COLOR_YUV420);
			if (!frame_buffers.backward)
				frame_buffers.backward = frame_ref(frame_buffers.output);
			if (!frame_buffers.forward)
				frame_buffers.forward = frame_ref(frame_buffers.output);

			// decode frame
			decode_mpeg(&frame_buffers, &mpeg);

			// simple frame reordering (not safe, only for testing)
			// count frames
			if (mpeg.gop > last_gop)
			{
				last_gop = mpeg.gop;
				gop_offset += gop_frames;
				gop_frames = 0;
			}
			gop_frames++;

			// save frame in ringbuffer
			if (frames[(gop_offset + mpeg.temporal_reference) % RING_BUFFER_SIZE] != NULL)
			{
				printf("Buffer overrun!\n");
				frame_unref(frames[(gop_offset + mpeg.temporal_reference) % RING_BUFFER_SIZE]);
			}
			frames[(gop_offset + mpeg.temporal_reference) % RING_BUFFER_SIZE] = frame_buffers.output;

			// if we decoded a displayable frame, show it
			if (frames[disp_frame % RING_BUFFER_SIZE] != NULL)
			{
				frame_show(frames[disp_frame % RING_BUFFER_SIZE]);
				frame_unref(frames[(disp_frame - 2) % RING_BUFFER_SIZE]);
				frames[(disp_frame - 2) % RING_BUFFER_SIZE] = NULL;
				disp_frame++;
				getchar();
			}

		}
		av_free_packet(&pkt);
	}

	// show left over frames
	while (disp_frame < gop_offset + gop_frames && frames[disp_frame % RING_BUFFER_SIZE] != NULL)
	{
		frame_show(frames[disp_frame % RING_BUFFER_SIZE]);
		frame_unref(frames[(disp_frame - 2) % RING_BUFFER_SIZE]);
		frames[(disp_frame - 2) % RING_BUFFER_SIZE] = NULL;
		disp_frame++;
		getchar();
	}

	disp_close();

	frame_unref(frames[(disp_frame - 2) % RING_BUFFER_SIZE]);
	frame_unref(frames[(disp_frame - 1) % RING_BUFFER_SIZE]);

	frame_unref(frame_buffers.forward);
	frame_unref(frame_buffers.backward);

	ve_close();

	avformat_close_input(&avfmt_ctx);

	return 0;
}
