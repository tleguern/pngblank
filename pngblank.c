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

#define PNGBLANK_MAX_SIZE 8192

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
write_IDAT(uint8_t *buf, size_t off, size_t width, int bitdepth, enum colourtype colourtype, int level, int strategy)
{
	size_t		 dataz, deflatedz;
	uint32_t	 crc, length;
	size_t		 bufw = 0;
	uint8_t		 type[4] = "IDAT";
	uint8_t		*data = NULL, *deflated = NULL;
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
	strm.next_in = data;
	strm.avail_in = dataz;
	strm.next_out = deflated;
	strm.avail_out = deflatedz;
	if (Z_OK != deflateParams(&strm, level, strategy)) {
		fprintf(stderr, "deflateParams stream error: %s\n", strm.msg);
		goto exit;
	}
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
	data = NULL;
	/* Set deflatedz to the real compressed size */
	deflatedz = strm.total_out;
	length = htonl(deflatedz);
	crc = crc32(0, Z_NULL, 0);
	crc = crc32(crc, type, sizeof(type));
	crc = crc32(crc, deflated, deflatedz);
	crc = htonl(crc);
	(void)memcpy(buf + off + bufw, &length, sizeof(length));
	bufw += sizeof(length);
	(void)memcpy(buf + off + bufw, type, sizeof(type));
	bufw += sizeof(type);

	(void)memcpy(buf + off + bufw, deflated, deflatedz);
	bufw += deflatedz;

	(void)memcpy(buf + off + bufw, &crc, sizeof(crc));
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
	FILE		*f = stdout;
	uint8_t		*buf;
	const char	*errstr = NULL;
	size_t		 width, off, written, minimum_size;
	int		 ch;
	int		 bflag;
	int		 gflag;
	int		 lflag;
	int		 nflag;
	int		 sflag;

	bflag = 8;
	gflag = COLOUR_TYPE_TRUECOLOUR;
	lflag = Z_DEFAULT_COMPRESSION;
	nflag = 0;
	sflag = Z_DEFAULT_STRATEGY;
	while (-1 != (ch = getopt(argc, argv, "b:gl:ns:")))
		switch (ch) {
		case 'b':
			if (0 == (bflag = strtonum(optarg, 1, 16, &errstr))) {
				fprintf(stderr, "value is %s -- b\n", errstr);
				return(EX_DATAERR);
			}
			if (bflag != 1 && bflag != 2 && bflag != 4
			    && bflag != 8 && bflag != 16) {
				fprintf(stderr, "value is invalid -- b\n");
				return(EX_DATAERR);
			}
			break;
		case 'g':
			gflag = COLOUR_TYPE_GREYSCALE;
			break;
		case 'l':
			lflag = strtonum(optarg, 1, 9, &errstr);
			if (NULL != errstr) {
				fprintf(stderr, "value is %s -- l\n", errstr);
				return(EX_DATAERR);
			}
			break;
		case 'n':
			nflag = 1;
			break;
		case 's':
			if (strcmp(optarg, "default") == 0) {
				sflag = Z_DEFAULT_STRATEGY;
			} else if (strcmp(optarg, "filtered") == 0) {
				sflag = Z_FILTERED;
			} else if (strcmp(optarg, "huffmanonly") == 0) {
				sflag = Z_HUFFMAN_ONLY;
			} else if (strcmp(optarg, "fixed") == 0) {
				sflag = Z_FIXED;
			} else if (strcmp(optarg, "rle") == 0) {
				sflag = Z_RLE;
			} else {
				fprintf(stderr, "unknown compression"
				    " strategy -- s\n");
				return(EX_DATAERR);
			}
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
	off = 0;
	off += write_png_sig(buf);
	off += write_IHDR(buf + off, width, bflag, gflag);
	off += write_tRNS(buf + off, gflag);
	off += write_IDAT(buf, off, width, bflag, gflag, lflag, sflag);
	off += write_IEND(buf + off);
	if (0 == nflag) {
		fwrite(buf, sizeof(uint8_t), off, f);
	} else {
		printf("%zu\n", off);
	}
	fclose(f);
	return(0);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-gn] [-b bitdepth] [-l level]"
			" [-s strategy] width\n", getprogname());
}

