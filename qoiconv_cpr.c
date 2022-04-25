/*

Command line tool to convert between png <> qoi format
The png > qoi conversion is Lossy

Requires "stb_image.h" and "stb_image_write.h"
Compile with: 
	gcc qoiconv_cpr.c -std=c99 -O3 -o qoiconv_cpr

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


#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_LINEAR
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define QOI_IMPLEMENTATION
#include "qoi_cpr.h"


#define STR_ENDS_WITH(S, E) (strcmp(S + strlen(S) - (sizeof(E)-1), E) == 0)

int main(int argc, char **argv) {
	if (argc < 3) {
		printf("Usage: qoiconv_cpr <infile> <outfile> [options]\n");
		printf("Options:\n");
		printf("  --weights ...... RGBA channel weights (in percentage, default 60 100 40 75).\n");
		printf("  --lothresh ..... low contrast threshhold (default 0.5)\n");
		printf("  --hithresh ..... high contrast threshhold (default 24)\n");
		printf("  --mulalpha ..... multiply alpha before comparison (default unmultiply)\n");
		printf("  --quality ...... jpeg encode quality (default 95)\n");
		printf("Examples\n");
		printf("  qoiconv_cpr input.png output.qoi --weights 60 100 40 75 --lowthresh 0.5 --highthresh 24 --mulalpha\n");
		printf("  qoiconv_cpr input.qoi output.png\n");
		exit(1);
	}

	qoi_cpr_cfg config = {
		.weights = {0.6f, 1.f, 0.4f, 0.75f},
		.lothresh = 0.5f, 
		.hithresh = 24.f,
		.mulalpha = 0
	};
	int quality = 95;

	int i = 3;
	while (i < argc) {
		if (strcmp(argv[i], "--weights") == 0) {
			if (i + 4 >= argc) { printf("Not enough weights args"); exit(1); }
			config.weights[0] = atof(argv[++i]) / 100.f;
			config.weights[1] = atof(argv[++i]) / 100.f;
			config.weights[2] = atof(argv[++i]) / 100.f;
			config.weights[3] = atof(argv[++i]) / 100.f;
		}
		else if (strcmp(argv[i], "--lothresh") == 0) {
			if (i + 1 >= argc) { printf("Missing lothresh arg"); exit(1); }
			config.lothresh = atof(argv[++i]);
		}
		else if (strcmp(argv[i], "--hithresh") == 0) {
			if (i + 1 >= argc) { printf("Missing hithresh arg"); exit(1); }
			config.hithresh = atof(argv[++i]);
		}
		else if (strcmp(argv[i], "--mulalpha") == 0) { config.mulalpha = 1; }
		else if (strcmp(argv[i], "--quality") == 0) {
			if (i + 1 >= argc) { printf("Missing quality arg"); exit(1); }
			quality = atoi(argv[++i]);
		}
		else { printf("Unknown option %s", argv[i]); exit(1); }
		i++;
	}

	void *pixels = NULL;
	int w, h, channels;
	if (STR_ENDS_WITH(argv[1], ".png") || STR_ENDS_WITH(argv[1], ".jpg") || STR_ENDS_WITH(argv[1], ".jpeg")) {
		if(!stbi_info(argv[1], &w, &h, &channels)) {
			printf("Couldn't read header %s\n", argv[1]);
			exit(1);
		}

		// Encodings to be RGB or RGBA
		channels = channels <= 3 ? 3 : 4;

		pixels = (void *)stbi_load(argv[1], &w, &h, NULL, channels);
	}
	else if (STR_ENDS_WITH(argv[1], ".qoi")) {
		qoi_desc desc;
		pixels = qoi_read(argv[1], &desc, 0);
		channels = desc.channels;
		w = desc.width;
		h = desc.height;
	}

	if (pixels == NULL) {
		printf("Couldn't load/decode %s\n", argv[1]);
		exit(1);
	}

	int encoded = 0;
	if (STR_ENDS_WITH(argv[2], ".png")) {
		encoded = stbi_write_png(argv[2], w, h, channels, pixels, 0);
	}
	if (STR_ENDS_WITH(argv[2], ".jpg") || STR_ENDS_WITH(argv[2], ".jpeg")) {
		encoded = stbi_write_jpg(argv[2], w, h, channels, pixels, quality);
	}
	else if (STR_ENDS_WITH(argv[2], ".qoi")) {
		encoded = qoi_cpr_write(argv[2], pixels, &(qoi_desc){
			.width = w,
			.height = h, 
			.channels = channels,
			.colorspace = QOI_SRGB
		}, &config);
	}

	if (!encoded) {
		printf("Couldn't write/encode %s\n", argv[2]);
		exit(1);
	}

	free(pixels);
	return 0;
	}