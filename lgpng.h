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

#ifndef LGPNG_H__
#define LGPNG_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* Generic chunk */

enum chunktype {
	CHUNK_TYPE_IHDR,
	CHUNK_TYPE_PLTE,
	CHUNK_TYPE_IDAT,
	CHUNK_TYPE_IEND,
	CHUNK_TYPE_tRNS,
	CHUNK_TYPE_cHRM,
	CHUNK_TYPE_gAMA,
	CHUNK_TYPE_iCCP,
	CHUNK_TYPE_sBIT,
	CHUNK_TYPE_sRGB,
	CHUNK_TYPE_iTXt,
	CHUNK_TYPE_tEXt,
	CHUNK_TYPE_zTXt,
	CHUNK_TYPE_bKGD,
	CHUNK_TYPE_hIST,
	CHUNK_TYPE_pHYs,
	CHUNK_TYPE_sPLT,
	CHUNK_TYPE_tIME,
	CHUNK_TYPE__MAX,
};

extern const char *chunktypemap[CHUNK_TYPE__MAX];

struct chunk {
	uint32_t	 length;
	enum chunktype	 type;
	uint32_t	 crc;
	uint8_t		 data[];
};

/* IHDR chunk */
enum colourtype {
	COLOUR_TYPE_GREYSCALE,
	COLOUR_TYPE_FILLER1,
	COLOUR_TYPE_TRUECOLOUR,
	COLOUR_TYPE_INDEXED,
	COLOUR_TYPE_GREYSCALE_ALPHA,
	COLOUR_TYPE_FILLER5,
	COLOUR_TYPE_TRUECOLOUR_ALPHA,
	COLOUR_TYPE__MAX,
};

extern const char *colourtypemap[COLOUR_TYPE__MAX];

enum compressiontype {
	COMPRESSION_TYPE_DEFLATE,
	COMPRESSION_TYPE__MAX,
};

extern const char *compressiontypemap[COMPRESSION_TYPE__MAX];

enum filtermethod {
	FILTER_METHOD_ADAPTIVE,
	FILTER_METHOD__MAX,
};

extern const char *filtermethodmap[FILTER_METHOD__MAX];

enum interlace_method {
	INTERLACE_METHOD_STANDARD,
	INTERLACE_METHOD_ADAM7,
	INTERLACE_METHOD__MAX,
};

extern const char *interlacemap[INTERLACE_METHOD__MAX];

struct IHDR {
	uint32_t	 length;
	enum chunktype	 type;
	uint32_t	 crc;
	struct {
		uint32_t	width;
		uint32_t	height;
		int8_t		bitdepth;
		int8_t		colourtype;
		int8_t		compression;
		int8_t		filter;
		int8_t		interlace;
	} __attribute__((packed)) data;
};

/* PLTE chunk */
struct rgb8 {
	uint8_t	red;
	uint8_t	green;
	uint8_t	blue;
} __attribute__((packed));

struct PLTE {
	uint32_t	 length;
	enum chunktype	 type;
	uint32_t	 crc;
	struct {
		struct rgb8	entry[256];
	} data;
};

/* IDAT chunk */
enum filtertype {
	FILTER_TYPE_NONE,
	FILTER_TYPE_SUB,
	FILTER_TYPE_UP,
	FILTER_TYPE_AVERAGE,
	FILTER_TYPE_PAETH,
	FILTER_TYPE__MAX,
};

struct IDAT {
	uint32_t	 length;
	enum chunktype	 type;
	uint32_t	 crc;
	uint8_t		*data;
};

/* tRNS chunk */
struct tRNS {
	uint32_t	 length;
	enum chunktype	 type;
	uint32_t	 crc;
	struct {
		uint16_t	gray;
		uint16_t	red;
		uint16_t	green;
		uint16_t	blue;
		uint8_t		palette[256];
	} data;
};

/* Generic interfaces */
size_t		write_png_sig(uint8_t *);
void		update_crc(struct chunk *);
size_t		write_chunk(uint8_t *, struct chunk *);

/* Specialized interfaces */
void		init_IHDR(struct IHDR *);
void		init_PLTE(struct PLTE *);
void		init_IDAT(struct IDAT *);
void		init_tRNS(struct tRNS *, enum colourtype);
size_t		write_IEND(uint8_t *);

#endif
