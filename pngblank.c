/*
 * Copyright (c) 2018 Tristan Le Guern <tleguern@bouledef.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <arpa/inet.h>

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "lgpng.h"

#define PNGBLANK_MAX_SIZE 1024

static size_t
write_tRNS(uint8_t *buf, enum colourtype colourtype)
{
	uint32_t	crc, length;
	size_t		bufw, trnsz;
	uint8_t		type[4] = "tRNS";
	uint8_t		trns[6];

	(void)memset(trns, 0, sizeof(trns));
	if (colourtype == COLOUR_TYPE_TRUECOLOUR) {
		trnsz = 6;
	} else if (colourtype == COLOUR_TYPE_GREYSCALE) {
		trnsz = 2;
	} else {
		return(0);
	}
	length = htonl(trnsz);
	crc = crc32(0, Z_NULL, 0);
	crc = crc32(crc, type, sizeof(type));
	crc = crc32(crc, (Bytef *)trns, trnsz);
	crc = htonl(crc);
	bufw = 0;
	(void)memcpy(buf + bufw, &length, sizeof(length));
	bufw += sizeof(length);
	(void)memcpy(buf + bufw, type, sizeof(type));
	bufw += sizeof(type);
	(void)memcpy(buf + bufw, &trns, trnsz);
	bufw += trnsz;
	(void)memcpy(buf + bufw, &crc, sizeof(crc));
	bufw += sizeof(crc);
	return(bufw);
}

static size_t
write_IDAT(uint8_t *buf, size_t width, int bitdepth, enum colourtype colourtype)
{
	size_t		 dataz, deflatedz;
	uint32_t	 crc, length;
	size_t		 bufw = 0;
	uint8_t		 type[4] = "IDAT";
	uint8_t		*data = NULL, *deflated = NULL;
	int		 level = Z_DEFAULT_COMPRESSION;
	z_stream	 strm;

	if (colourtype == COLOUR_TYPE_TRUECOLOUR) {
		dataz = width * width * 3 * bitdepth / 8 + width;
	} else if (colourtype == COLOUR_TYPE_GREYSCALE) {
		dataz = (width / (8 / bitdepth) + \
		    (width % (8 / bitdepth) != 0 ? 1 : 0) + 1) * width;
	} else {
		return(0);
	}
	/* We are going to compress data, a string of NULL bytes */
	if (NULL == (data = calloc(dataz, sizeof(*data)))) {
		goto exit;
	}
	deflatedz = deflateBound(&strm, dataz);
	if (NULL == (deflated = calloc(deflatedz, sizeof(*deflated)))) {
		goto exit;
	}
	strm.zalloc = NULL;
	strm.zfree = NULL;
	strm.opaque = NULL;
	switch (deflateInit(&strm, level)) {
	case Z_OK:
		break;
	default:
		fprintf(stderr, "deflateInit: %s\n", strm.msg);
		goto exit;
	}
	fprintf(stderr, "dataz: %zu\n", dataz);
	fprintf(stderr, "deflatedz: %zu\n", deflatedz);
	strm.next_in = data;
	strm.avail_in = dataz;
	strm.next_out = deflated;
	strm.avail_out = deflatedz;
	/* Compress data in a single step */
	switch (deflate(&strm, Z_FINISH)) {
	case Z_OK:
		fprintf(stderr, "deflate: more space was needed\n");
		goto exit;
	case Z_STREAM_END:
		break;
	default:
		fprintf(stderr, "deflate: %s\n", strm.msg);
		goto exit;
	}
	if (Z_OK != deflateEnd(&strm)) {
		fprintf(stderr, "%s\n", strm.msg);
		goto exit;
	}
	free(data);
	/* Set deflatedz to the real compressed size */
	deflatedz = strm.total_out;
	fprintf(stderr, "dataz: %lld\n", strm.total_in);
	fprintf(stderr, "deflatedz: %lld\n", strm.total_out);
	fprintf(stderr, "Compression finished\n");
	length = htonl(deflatedz);
	crc = crc32(0, Z_NULL, 0);
	crc = crc32(crc, type, sizeof(type));
	crc = crc32(crc, deflated, deflatedz);
	crc = htonl(crc);
	(void)memcpy(buf + bufw, &length, sizeof(length));
	bufw += sizeof(length);
	(void)memcpy(buf + bufw, type, sizeof(type));
	bufw += sizeof(type);
	(void)memcpy(buf + bufw, deflated, deflatedz);
	bufw += deflatedz;
	(void)memcpy(buf + bufw, &crc, sizeof(crc));
	bufw += sizeof(crc);
	free(deflated);
	return(bufw);
exit:
	free(data);
	free(deflated);
	return(-1);
}

int
main(int argc, char *argv[])
{
	FILE	*f = stdout;
	uint8_t	*buf;
	size_t	 width, bufz;
	int	 ch;
	int	 bflag;
	int	 gflag;

	bflag = 8;
	gflag = COLOUR_TYPE_TRUECOLOUR;
	while (-1 != (ch = getopt(argc, argv, "b:g")))
		switch (ch) {
		case 'b':
			if (0 == (bflag = strtonum(optarg, 1, 16, NULL))) {
				fprintf(stderr, "Invalid bit depth value\n");
				return(EX_DATAERR);
			}
			if (bflag != 1 && bflag != 2 && bflag != 4
			    && bflag != 8 && bflag != 16) {
				fprintf(stderr, "Invalid bit depth value\n");
				return(EX_DATAERR);
			}
			break;
		case 'g':
			gflag = COLOUR_TYPE_GREYSCALE;
			break;
		default:
			usage();
			exit(EX_USAGE);
		}
	argc -= optind;
	argv += optind;

	if (argc == 0 || argc > 1) {
		fprintf(stderr, "Width expected\n");
		usage();
		return(EX_USAGE);
	}
	if (0 == (width = strtonum(argv[0], 1, 512, NULL))) {
		fprintf(stderr, "Width should be between 1 and 512\n");
		return(EX_DATAERR);
	}

	if (NULL == (buf = calloc(PNGBLANK_MAX_SIZE, 1))) {
		fprintf(stderr, "malloc(%i)\n", PNGBLANK_MAX_SIZE);
		return(EX_OSERR);
	}
	bufz = 0;
	bufz += write_png_sig(buf);
	bufz += write_IHDR(buf + bufz, width, bflag, gflag);
	bufz += write_tRNS(buf + bufz, gflag);
	bufz += write_IDAT(buf + bufz, width, bflag, gflag);
	bufz += write_IEND(buf + bufz);
	fwrite(buf, sizeof(uint8_t), bufz, f);
	fclose(f);
	return(0);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-g] [-b bitdepth] width\n", getprogname());
}

