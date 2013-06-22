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

#include <stdio.h>
#include "jpeg.h"

#define M_SOF0  0xc0
#define M_SOF1  0xc1
#define M_SOF2  0xc2
#define M_SOF3  0xc3
#define M_SOF5  0xc5
#define M_SOF6  0xc6
#define M_SOF7  0xc7
#define M_SOF9  0xc9
#define M_SOF10 0xca
#define M_SOF11 0xcb
#define M_SOF13 0xcd
#define M_SOF14 0xce
#define M_SOF15 0xcf
#define M_SOI   0xd8
#define M_EOI   0xd9
#define M_SOS   0xda
#define M_DQT   0xdb
#define M_DRI   0xdd
#define M_DHT   0xc4
#define M_DAC   0xcc

const char comp_types[5][3] = { "Y", "Cb", "Cr" };

static int process_dqt(struct jpeg_t *jpeg, const uint8_t *data, const int len)
{
	int pos;

	for (pos = 0; pos < len; pos += 65)
	{
		if ((data[pos] >> 4) != 0)
		{
			fprintf(stderr, "Only 8bit Quantization Tables supported\n");
			return 0;
		}
		jpeg->quant[data[pos] & 0x03] = (struct quant_t *)&(data[pos + 1]);
	}

	return 1;
}

static int process_dht(struct jpeg_t *jpeg, const uint8_t *data, const int len)
{
	int pos = 0;

	while (pos < len)
	{
		uint8_t id = ((data[pos] & 0x03) << 1) | ((data[pos] & 0x10) >> 4);

		jpeg->huffman[id] = (struct huffman_t *)&(data[pos + 1]);

		int i;
		pos += 17;
		for (i = 0; i < 16; i++)
			pos += jpeg->huffman[id]->num[i];
	}
	return 1;
}

int parse_jpeg(struct jpeg_t *jpeg, const uint8_t *data, const int len)
{
	if (len < 2 || data[0] != 0xff || data[1] != M_SOI)
		return 0;

	int pos = 2;
	int sos = 0;
	while (!sos)
	{
		int i;

		while (pos < len && data[pos] == 0xff)
			pos++;

		if (!(pos + 2 < len))
			return 0;

		uint8_t marker = data[pos++];
		uint16_t seg_len = ((uint16_t)data[pos]) << 8 | (uint16_t)data[pos + 1];

		if (!(pos + seg_len < len))
			return 0;

		switch (marker)
		{
		case M_DQT:
			if (!process_dqt(jpeg, &data[pos + 2], seg_len - 2))
				return 0;

			break;

		case M_DHT:
			if (!process_dht(jpeg, &data[pos + 2], seg_len - 2))
				return 0;

			break;

		case M_SOF0:
			jpeg->bits = data[pos + 2];
			jpeg->width = ((uint16_t)data[pos + 5]) << 8 | (uint16_t)data[pos + 6];
			jpeg->height = ((uint16_t)data[pos + 3]) << 8 | (uint16_t)data[pos + 4];
			for (i = 0; i < data[pos + 7]; i++)
			{
				uint8_t id = data[pos + 8 + 3 * i] - 1;
				if (id > 2)
				{
					fprintf(stderr, "only YCbCr supported\n");
					return 0;
				}
				jpeg->comp[id].samp_h = data[pos + 9 + 3 * i] >> 4;
				jpeg->comp[id].samp_v = data[pos + 9 + 3 * i] & 0x0f;
				jpeg->comp[id].qt = data[pos + 10 + 3 * i];
			}
			break;

		case M_DRI:
			jpeg->restart_interval = ((uint16_t)data[pos + 2]) << 8 | (uint16_t)data[pos + 3];
			break;

		case M_SOS:
			for (i = 0; i < data[pos + 2]; i++)
			{
				uint8_t id = data[pos + 3 + 2 * i] - 1;
				if (id > 2)
				{
					fprintf(stderr, "only YCbCr supported\n");
					return 0;
				}
				jpeg->comp[id].ht_dc = data[pos + 4 + 2 * i] >> 4;
				jpeg->comp[id].ht_ac = data[pos + 4 + 2 * i] & 0x0f;
			}
			sos = 1;
			break;

		case M_DAC:
			fprintf(stderr, "Arithmetic Coding unsupported\n");
			return 0;

		case M_SOF1:
		case M_SOF2:
		case M_SOF3:
		case M_SOF5:
		case M_SOF6:
		case M_SOF7:
		case M_SOF9:
		case M_SOF10:
		case M_SOF11:
		case M_SOF13:
		case M_SOF14:
		case M_SOF15:
			fprintf(stderr, "only Baseline DCT supported (yet?)\n");
			return 0;

		case M_SOI:
		case M_EOI:
			fprintf(stderr, "corrupted file\n");
			return 0;

		default:
			//fprintf(stderr, "unknown marker: 0x%02x len: %u\n", marker, seg_len);
			break;
		}
		pos += seg_len;
	}

	jpeg->data = (uint8_t *)&(data[pos]);
	jpeg->data_len = len - pos;

	return 1;
}

void dump_jpeg(const struct jpeg_t *jpeg)
{
	int i, j, k;
	printf("Width: %u  Height: %u  Bits: %u\nRestart interval: %u\nComponents:\n", jpeg->width, jpeg->height, jpeg->bits, jpeg->restart_interval);
	for (i = 0; i < 3; i++)
	{
		if (jpeg->comp[i].samp_h && jpeg->comp[i].samp_v)
			printf("\tType: %s  Sampling: %u:%u  QT: %u  HT: %u/%u\n", comp_types[i], jpeg->comp[i].samp_h, jpeg->comp[i].samp_v, jpeg->comp[i].qt, jpeg->comp[i].ht_dc, jpeg->comp[i].ht_dc);
	}
	printf("Quantization Tables:\n");
	for (i = 0; i < 4; i++)
	{
		if (jpeg->quant[i])
		{
			printf("\tID: %u\n", i);
			for (j = 0; j < 64 / 8; j++)
			{
				printf("\t\t");
				for (k = 0; k < 8; k++)
				{
					printf("0x%02x ", jpeg->quant[i]->coeff[j * 8 + k]);
				}
				printf("\n");
			}
		}
	}
	printf("Huffman Tables:\n");
	for (i = 0; i < 8; i++)
	{
		if (jpeg->huffman[i])
		{
			printf("\tID: %u (%cC)\n", (i & 0x06) >> 1, i & 0x01 ? 'A' : 'D');
			int sum = 0;
			for (j = 0; j < 16; j++)
			{
				if (jpeg->huffman[i]->num[j])
				{
					printf("\t\t%u bits:", j + 1);
					for (k = 0; k < jpeg->huffman[i]->num[j]; k++)
					{
						printf(" 0x%02x", jpeg->huffman[i]->codes[sum + k]);
					}
					printf("\n");
					sum += jpeg->huffman[i]->num[j];
				}
			}
		}
	}
	printf("Data length: %u\n", jpeg->data_len);
}
