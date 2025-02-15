/* The original QOI header &  implementation */
#include "qoi.h"

/*

QOI compressor - Lossy compressor following the "Quite OK Image" format specification

Dominic Szablewski - https://phoboslab.org
Chen J.C.


-- LICENSE: The MIT License(MIT)

Copyright(c) 2022 Dominic Szablewski & Chen J.C.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files(the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions :
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/* -----------------------------------------------------------------------------
Header - Public functions */

#ifndef QOI_CPR_H
#define QOI_CPR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	float weights[4];
	float lothresh;
	float hithresh;
	int mulalpha;
} qoi_cpr_cfg;

#ifndef QOI_NO_STDIO

/* Encode raw RGB or RGBA pixels into a "lossy" QOI image and write it to the file
system. The qoi_desc struct must be filled with the image width, height,
number of channels (3 = RGB, 4 = RGBA) and the colorspace. The qoi_cpr_cfg struct
must be filled with the RGBA channel weights, low contrast threshhold, low contrast
threshhold and multiply alpha mode.

The function returns 0 on failure (invalid parameters, or fopen or malloc
failed) or the number of bytes written on success. */

int qoi_cpr_write(const char *filename, const void *data, const qoi_desc *desc, const qoi_cpr_cfg *cfg);

#endif /* QOI_NO_STDIO */


/* Encode raw RGB or RGBA pixels into a "lossy" QOI image in memory.

The function either returns NULL on failure (invalid parameters or malloc
failed) or a pointer to the encoded data on success. On success the out_len
is set to the size in bytes of the encoded data.

The returned qoi data should be free()d after use. */

void *qoi_cpr_encode(const void *data, const qoi_desc *desc, const qoi_cpr_cfg *cfg, int *out_len);


#ifdef __cplusplus
}
#endif
#endif /* QOI_CPR_H */

/* -----------------------------------------------------------------------------
Implementation */

#ifdef QOI_IMPLEMENTATION
#include <stdlib.h>
#include <string.h>

#define QOI_CPR_MAXFLOAT 3.4028234e38

#define QOI_CPR_MAX(a,b) ((a) > (b) ? (a) : (b))
#define QOI_CPR_MIN(a,b) ((a) < (b) ? (a) : (b))
#define QOI_CPR_CLAMP(a,l,h) ((a) < (l) ? (l) : (a) > (h) ? (h) : (a))

inline int compare_color(const qoi_rgba_t px, const float alpha, const qoi_rgba_t px_cmp, const float *thresh, const qoi_cpr_cfg *cfg, float *score) {
	float diff[4] = {
		abs(px.rgba.r - px_cmp.rgba.r) * cfg->weights[0] * alpha,
		abs(px.rgba.g - px_cmp.rgba.g) * cfg->weights[1] * alpha,
		abs(px.rgba.b - px_cmp.rgba.b) * cfg->weights[2] * alpha,
		abs(px.rgba.a - px_cmp.rgba.a) * cfg->weights[3]
	};

	if (score) {
		*score = diff[0] + diff[1] + diff[2] + diff[3];
	}

	return diff[0] <= thresh[0] &&
		diff[1] <= thresh[0] &&
		diff[2] <= thresh[0] &&
		diff[3] <= thresh[1];
}

void *qoi_cpr_encode(const void *data, const qoi_desc *desc, const qoi_cpr_cfg *cfg, int *out_len) {
	int i, max_size, p, run;
	int px_len, px_end, px_pos, channels;
	float diff_prev[2], diff_next[2], local_thresh[2];
	unsigned char *bytes;
	const unsigned char *pixels;
	qoi_rgba_t index[64];
	unsigned long long mask;
	qoi_rgba_t px, px_prev, px_next, px_stored, px_potential;
	float alpha, diff_sum;

	if (
		data == NULL || out_len == NULL || desc == NULL ||
		desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 ||
		desc->colorspace > 1 ||
		desc->height >= QOI_PIXELS_MAX / desc->width
	) {
		return NULL;
	}

	max_size =
		desc->width * desc->height * (desc->channels + 1) +
		QOI_HEADER_SIZE + sizeof(qoi_padding);

	p = 0;
	bytes = (unsigned char *) QOI_MALLOC(max_size);
	if (!bytes) {
		return NULL;
	}

	qoi_write_32(bytes, &p, QOI_MAGIC);
	qoi_write_32(bytes, &p, desc->width);
	qoi_write_32(bytes, &p, desc->height);
	bytes[p++] = desc->channels;
	bytes[p++] = desc->colorspace;


	pixels = (const unsigned char *)data;

	QOI_ZEROARR(index);
	mask = (unsigned long long)1;

	run = 0;
	px_stored.v = px.v = 0xff000000; /* {0, 0, 0, 255} */
	px_next.rgba.r = pixels[0];
	px_next.rgba.g = pixels[1];
	px_next.rgba.b = pixels[2];
	px_next.rgba.a = desc->channels == 4 ? pixels[3] : 255;

	diff_prev[0] = abs(px_next.rgba.r - px.rgba.r) * cfg->weights[0]
		+ abs(px_next.rgba.g - px.rgba.g) * cfg->weights[1]
		+ abs(px_next.rgba.b - px.rgba.b) * cfg->weights[2];
	diff_prev[1] = abs(px_next.rgba.a - px.rgba.a);
	diff_sum = (cfg->weights[0] + cfg->weights[1] + cfg->weights[2]) * 255.f;
	if (!diff_sum) diff_sum = 1.f;

	px_len = desc->width * desc->height * desc->channels;
	px_end = px_len - desc->channels;
	channels = desc->channels;

	for (px_pos = 0; px_pos < px_len; px_pos += channels) {
		px_prev = px;
		px = px_next;
		alpha = 1.f;

		if (cfg->mulalpha) {
			px.v = px.rgba.a ? px.v : 0;
			alpha = px.rgba.a / 255.f;
		}

		if (px_pos + channels < px_len) {
			px_next.rgba.r = pixels[px_pos + channels + 0];
			px_next.rgba.g = pixels[px_pos + channels + 1];
			px_next.rgba.b = pixels[px_pos + channels + 2];

			if (channels == 4) {
				px_next.rgba.a = pixels[px_pos + channels + 3];
			}
		}
		else {
			px_next = px_prev; /* Keep maximum contrast */
		}

		diff_next[0] = abs(px_next.rgba.r - px.rgba.r) * cfg->weights[0]
			+ abs(px_next.rgba.g - px.rgba.g) * cfg->weights[1]
			+ abs(px_next.rgba.b - px.rgba.b) * cfg->weights[2];
		diff_next[1] = abs(px_next.rgba.a - px.rgba.a);

		float contrast = QOI_CPR_MIN(diff_prev[0], diff_next[0]) / diff_sum * alpha;
		local_thresh[0] = cfg->lothresh * (1 - contrast) + cfg->hithresh * contrast;
		diff_prev[0] = diff_next[0];

		contrast = QOI_CPR_MIN(diff_prev[1], diff_next[1]) / 255.f;
		local_thresh[1] = cfg->lothresh * (1 - contrast) + cfg->hithresh * contrast;
		diff_prev[1] = diff_next[1];

		if (px.v == px_stored.v || compare_color(px, alpha, px_stored, local_thresh, cfg, NULL)) {
			run++;
			if (run == 62 || px_pos == px_end) {
				bytes[p++] = QOI_OP_RUN | (run - 1);
				run = 0;
			}
		}
		else {
			int index_pos;

			if (run > 0) {
				bytes[p++] = QOI_OP_RUN | (run - 1);
				run = 0;
			}

			index_pos = QOI_COLOR_HASH(px) % 64;

			if (index[index_pos].v == px.v) {
				bytes[p++] = QOI_OP_INDEX | index_pos;
				px_stored = index[index_pos];
				continue;
			}

			float score_min = QOI_CPR_MAXFLOAT;
			float score;

			index_pos = -1;

			for (i = 0; i < 64; i++) {
				if (
					/* Make sure color is valid
					The behavior to update the index of invalid color is undefined */
					(mask & ((unsigned long long)1 << i)) &&
					compare_color(px, alpha, index[i], local_thresh, cfg, &score) &&
					score < score_min
				) {
					score_min = score;
					index_pos = i;
				}
			}

			if (index_pos >= 0) {
				bytes[p++] = QOI_OP_INDEX | index_pos;
				px_stored = index[index_pos];
			}
			else {
				if (abs(px.rgba.a - px_stored.rgba.a) * cfg->weights[3] <= local_thresh[1]) {
					signed char vr = px.rgba.r - px_stored.rgba.r;
					signed char vg = px.rgba.g - px_stored.rgba.g;
					signed char vb = px.rgba.b - px_stored.rgba.b;

					signed char _vr = QOI_CPR_CLAMP(vr, -2, 1);
					signed char _vg = QOI_CPR_CLAMP(vg, -2, 1);
					signed char _vb = QOI_CPR_CLAMP(vb, -2, 1);

					qoi_rgba_t px_potential = {
						.rgba.r = px_stored.rgba.r + _vr,
						.rgba.g = px_stored.rgba.g + _vg,
						.rgba.b = px_stored.rgba.b + _vb,
						.rgba.a = px_stored.rgba.a
					};

					if (px.v == px_potential.v || compare_color(px, alpha, px_potential, local_thresh, cfg, NULL)) {
						bytes[p++] = QOI_OP_DIFF | (_vr + 2) << 4 | (_vg + 2) << 2 | (_vb + 2);
						px_stored = px_potential;
					}
					else {
						_vg = QOI_CPR_CLAMP(vg, -32, 31);
						signed char vg_r = vr - _vg;
						signed char vg_b = vb - _vg;
						vg_r = QOI_CPR_CLAMP(vg_r, -8, 7);
						vg_b = QOI_CPR_CLAMP(vg_b, -8, 7);

						px_stored.rgba.r += _vg + vg_r;
						px_stored.rgba.g += _vg;
						px_stored.rgba.b += _vg + vg_b;

						if (px.v == px_stored.v || compare_color(px, alpha, px_stored, local_thresh, cfg, NULL)) {
							bytes[p++] = QOI_OP_LUMA     | (_vg  + 32);
							bytes[p++] = (vg_r + 8) << 4 | (vg_b +  8);
						}
						else {
							bytes[p++] = QOI_OP_RGB;
							px_stored = *(qoi_rgba_t *)(bytes + p) = px;
							p += 3;
							px_stored.rgba.a = px_potential.rgba.a;
						}
					}
				}
				else {
					bytes[p++] = QOI_OP_RGBA;
					*(qoi_rgba_t *)(bytes + p) = px;
					p += 4;
					px_stored = px;
				}

				index_pos = QOI_COLOR_HASH(px_stored) % 64;
				index[index_pos] = px_stored;
				mask |= (unsigned long long)1 << index_pos;
			}
		}
	}

	for (i = 0; i < (int)sizeof(qoi_padding); i++) {
		bytes[p++] = qoi_padding[i];
	}

	*out_len = p;
	return bytes;
}

#ifndef QOI_NO_STDIO
#include <stdio.h>

int qoi_cpr_write(const char *filename, const void *data, const qoi_desc *desc, const qoi_cpr_cfg *cfg) {
	FILE *f = fopen(filename, "wb");
	int size;
	void *encoded;

	if (!f) {
		return 0;
	}

	encoded = qoi_cpr_encode(data, desc, cfg, &size);
	if (!encoded) {
		fclose(f);
		return 0;
	}

	fwrite(encoded, 1, size, f);
	fclose(f);

	QOI_FREE(encoded);
	return size;
}

#endif /* QOI_NO_STDIO */
#endif /* QOI_IMPLEMENTATION */