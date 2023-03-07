/*
 * Copyright (c) 2018,2020 Tristan Le Guern <tleguern@bouledef.eu>
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
#include <libdeflate.h>

#include "lgpng.h"

#define PNGBLANK_MAX_SIZE 8192

enum {
	PNG_BLANK_ZLIB,
	PNG_BLANK_LIBDEFLATE
};

static void usage(void);

static int
create_IDAT_with_zlib(struct IDAT *idat, int level, int strategy)
{
	size_t		 deflatedz;
	uint8_t		*deflated = NULL;
	z_stream	 strm;

	/* Prepare for a single-step compression */
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
	deflatedz = deflateBound(&strm, idat->length);
	if (NULL == (deflated = calloc(deflatedz, sizeof(*deflated)))) {
		fprintf(stderr, "calloc()\n");
		goto exit;
	}
	strm.next_in = idat->data.data;
	strm.avail_in = idat->length;
	strm.next_out = deflated;
	strm.avail_out = deflatedz;
	if (Z_OK != deflateParams(&strm, level, strategy)) {
		fprintf(stderr, "deflateParams stream error: %s\n", strm.msg);
		goto exit;
	}
	/* Finaly compress data */
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
	free(idat->data.data);
	/* Set deflatedz to the real compressed size */
	deflatedz = strm.total_out;
	idat->length = deflatedz;
	idat->data.data = deflated;
	return(0);
exit:
	free(deflated);
	return(-1);
}

static int
create_IDAT_with_libdeflate(struct IDAT *idat, int level)
{
	size_t				 deflatedz;
	uint8_t				*deflated = NULL;
	struct libdeflate_compressor	*compressor = NULL;

	compressor = libdeflate_alloc_compressor(level);
	deflatedz = libdeflate_zlib_compress_bound(compressor, idat->length);
	if (NULL == (deflated = calloc(deflatedz, 1))) {
		fprintf(stderr, "calloc()\n");
		goto exit;
	}
	if (0 == (deflatedz = libdeflate_zlib_compress(compressor, idat->data.data, idat->length, deflated, deflatedz))) {
		fprintf(stderr, "Can't compress data with libdeflate\n");
		goto exit;
	}
	free(idat->data.data);
	deflated = realloc(deflated, deflatedz);
	idat->length = deflatedz;
	idat->data.data = deflated;
	libdeflate_free_compressor(compressor);
	return(0);
exit:
	free(deflated);
	libdeflate_free_compressor(compressor);
	return(-1);
}

int
main(int argc, char *argv[])
{
	FILE		*f = stdout;
	uint8_t		*buf;
	const char	*errstr = NULL;
	char		*rawlflag = NULL;
	size_t		 width, off;
	int		 ch, colourtype;
	int		 bflag;
	int		 cflag;
	int		 gflag;
	int		 lflag;
	int		 nflag;
	int		 pflag;
	int		 sflag;
	struct IHDR	 ihdr;
	struct PLTE	 plte;
	struct IDAT	 idat;
	struct tRNS	 trns;
	uint32_t	 iend_crc;
	int		 max;
	int		 zlib_max = 9;
	int		 libdeflate_max = 12;

#if HAVE_PLEDGE
        pledge("stdio", NULL);
#endif

	bflag = 8;
	cflag = PNG_BLANK_ZLIB;
	gflag = 0;
	lflag = Z_DEFAULT_COMPRESSION;
	nflag = 0;
	pflag = 0;
	sflag = Z_DEFAULT_STRATEGY;
	colourtype = COLOUR_TYPE_TRUECOLOUR;
	max = zlib_max;
	while (-1 != (ch = getopt(argc, argv, "b:c:gl:nps:")))
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
		case 'c':
			if (0 == strcmp("zlib", optarg)) {
				cflag = PNG_BLANK_ZLIB;
			} else if (0 == strcmp("libdeflate", optarg)) {
				cflag = PNG_BLANK_LIBDEFLATE;
				lflag = 6;
				max = libdeflate_max;
			} else {
				fprintf(stderr, "invalid compression library -- %s",
				    optarg);
			}
			break;
		case 'g':
			gflag = 1;
			break;
		case 'l':
			rawlflag = optarg;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'p':
			pflag = 1;
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
		fprintf(stderr, "Width should be between 1 and 512, not %s\n", argv[0]);
		return(EX_DATAERR);
	}
	if (1 == gflag && 1 == pflag) {
		fprintf(stderr, "Options -g and -p are mutualy exclusive\n");
		usage();
		return(EX_USAGE);
	} else if (1 == gflag) {
		colourtype = COLOUR_TYPE_GREYSCALE;
	} else if (1 == pflag) {
		colourtype = COLOUR_TYPE_INDEXED;
	}

	/* libdeflate and zlib do not accept the same compression levels */
	if (NULL != rawlflag) {
		lflag = strtonum(rawlflag, 1, max, &errstr);
		if (NULL != errstr) {
			fprintf(stderr, "value is %s, should be"
			    "between 1 and %d -- l\n", errstr, max);
			return(EX_DATAERR);
		}
	}

	/* Prepare the output buffer, used mainly for -n */
	if (NULL == (buf = calloc(PNGBLANK_MAX_SIZE, 1))) {
		fprintf(stderr, "malloc(%i)\n", PNGBLANK_MAX_SIZE);
		return(EX_OSERR);
	}

	/* IHDR preparation */
	ihdr.length = 13;
	ihdr.type = CHUNK_TYPE_IHDR;
	ihdr.data.width = htonl(width);
	ihdr.data.height = htonl(width);
	ihdr.data.bitdepth = bflag;
	ihdr.data.colourtype = colourtype;
	ihdr.data.compression = COMPRESSION_TYPE_DEFLATE;
	ihdr.data.filter = FILTER_METHOD_ADAPTIVE;
	ihdr.data.interlace = INTERLACE_METHOD_STANDARD;
	lgpng_chunk_crc(ihdr.length, "IHDR", (uint8_t *)&ihdr.data, &(ihdr.crc));

	/* PLTE preparation */
	if (1 == pflag) {
		plte.length = 3; /* Three bytes in a PLTE entry, it's RGB */
		plte.type = CHUNK_TYPE_PLTE;
		plte.data.entries = 1;
		(void)memset(plte.data.entry, '\0', sizeof(plte.data.entry));
		lgpng_chunk_crc(plte.length, "PLTE", (uint8_t *)&plte.data.entry, &(plte.crc));
	}

	/* tRNS preparation */
	if (COLOUR_TYPE_TRUECOLOUR == colourtype) {
		trns.length = 6;
	} else if (COLOUR_TYPE_GREYSCALE == colourtype) {
		trns.length = 2;
	} else if (COLOUR_TYPE_INDEXED == colourtype) {
		trns.length = 1;
	}
	trns.type = CHUNK_TYPE_tRNS;
	(void)memset(&(trns.data), '\0', sizeof(trns.data));
	lgpng_chunk_crc(trns.length, "tRNS", (uint8_t *)&trns.data, &(trns.crc));

	/* IDAT preparation */
	/* Calculate the buffer size using bitdepth and colour type */
	if (COLOUR_TYPE_TRUECOLOUR == ihdr.data.colourtype) {
		idat.length = width * width * 3 * ihdr.data.bitdepth / 8 + width;
	} else if (COLOUR_TYPE_GREYSCALE == ihdr.data.colourtype) {
		idat.length = (width / (8 / ihdr.data.bitdepth) + \
		    (width % (8 / ihdr.data.bitdepth) != 0 ? 1 : 0) + 1) * width;
	} else if (COLOUR_TYPE_INDEXED == ihdr.data.colourtype) {
		idat.length = width * width * ihdr.data.bitdepth / 8 + width;
	} else {
		fprintf(stderr, "Invalid colourtype\n");
		return(-1);
	}
	idat.type = CHUNK_TYPE_IDAT;
	/* The data is a stream of zero so calloc is perfect */
	if (NULL == (idat.data.data = calloc(idat.length, 1))) {
		return(-1);
	}
	if (PNG_BLANK_ZLIB == cflag) {
		if (-1 == create_IDAT_with_zlib(&idat, lflag, sflag)) {
			return(1);
		}
	} else {
		if (-1 == create_IDAT_with_libdeflate(&idat, lflag)) {
			return(1);
		}
	}
	lgpng_chunk_crc(idat.length, "IDAT", idat.data.data, &(idat.crc));

	/* IEND preparation */
	lgpng_chunk_crc(0, "IEND", NULL, &iend_crc);

	off = lgpng_data_write_sig(buf);
	off += lgpng_data_write_chunk(buf + off, ihdr.length, "IHDR", (uint8_t *)&ihdr.data, ihdr.crc);
	if (1 == pflag) {
		off += lgpng_data_write_chunk(buf + off, plte.length, "PLTE", (uint8_t *)&plte.data.entry, plte.crc);
	}
	off += lgpng_data_write_chunk(buf + off, trns.length, "tRNS", (uint8_t *)&trns.data, trns.crc);
	off += lgpng_data_write_chunk(buf + off, idat.length, "IDAT", idat.data.data, idat.crc);
	off += lgpng_data_write_chunk(buf + off, 0, "IEND", NULL, iend_crc);
	free(idat.data.data);
	if (0 == nflag) {
		fwrite(buf, sizeof(uint8_t), off, f);
	} else {
		printf("%zu\n", off);
	}
	return(0);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-gnp] [-b bitdepth] [-c library] [-l level]"
			" [-s strategy] [-c library] width\n", getprogname());
}

