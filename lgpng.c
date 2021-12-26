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
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "lgpng.h"

const char *chunktypemap[CHUNK_TYPE__MAX] = {
	"IHDR",
	"PLTE",
	"IDAT",
	"IEND",
	"tRNS",
	"cHRM",
	"gAMA",
	"iCCP",
	"sBIT",
	"sRGB",
	"iTXt",
	"tEXt",
	"zTXt",
	"bKGD",
	"hIST",
	"pHYs",
	"sPLT",
	"tIME"
};

const char *colourtypemap[COLOUR_TYPE__MAX] = {
	"greyscale",
	"error",
	"truecolour",
	"indexed",
	"greyscale + alpha",
	"error",
	"truecolour + alpha",
};

const char *compressiontypemap[COMPRESSION_TYPE__MAX] = {
	"deflate",
};

const char *filtermethodmap[FILTER_METHOD__MAX] = {
	"adaptive",
};

const char *interlacemap[INTERLACE_METHOD__MAX] = {
	"standard",
	"adam7",
};

size_t
write_png_sig(uint8_t *buf)
{
	char	sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};

	(void)memcpy(buf, sig, sizeof(sig));
	return(sizeof(sig));
}

/*
 * Specialized initialization functions, perhaps to be replaced later
 * by a unified interface.
 */
void
init_IHDR(struct IHDR *ihdr)
{
	ihdr->length = 13;
	ihdr->type = CHUNK_TYPE_IHDR;
	ihdr->data.width = 0;
	ihdr->data.height = 0;
	ihdr->data.bitdepth = 8;
	ihdr->data.colourtype = COLOUR_TYPE_TRUECOLOUR;
	ihdr->data.compression = COMPRESSION_TYPE_DEFLATE;
	ihdr->data.filter = FILTER_METHOD_ADAPTIVE;
	ihdr->data.interlace = INTERLACE_METHOD_STANDARD;
	ihdr->crc = 0;
}

void
init_PLTE(struct PLTE *plte)
{
	plte->length = 0;
	plte->type = CHUNK_TYPE_PLTE;
	(void)memset(plte->data.entry, '\0', sizeof(plte->data.entry));
	plte->crc = 0;
}

void
init_IDAT(struct IDAT *idat)
{
	idat->length = 0;
	idat->type = CHUNK_TYPE_IDAT;
	idat->data = NULL;
	idat->crc = 0;
}

void
init_tRNS(struct tRNS *trns, enum colourtype colourtype)
{
	if (COLOUR_TYPE_TRUECOLOUR == colourtype) {
		trns->length = 6;
	} else if (COLOUR_TYPE_GREYSCALE == colourtype) {
		trns->length = 2;
	} else {
		trns->length = 0; /* True length has to be decided later */
	}
	trns->type = CHUNK_TYPE_tRNS;
	(void)memset(&(trns->data), '\0', sizeof(trns->data));
	trns->crc = 0;
}

/*
 * Calculate the CRC member of a chunk and convert it to a format suitable
 * for writing.
 */
void
update_crc(struct chunk *chunk)
{
	uint32_t	crc;

	crc = crc32(0, Z_NULL, 0);
	crc = crc32(crc, chunktypemap[chunk->type], 4);
	if (CHUNK_TYPE_IDAT == chunk->type) {
		struct IDAT *idat;

		idat = (struct IDAT *)chunk;
		crc = crc32(crc, (Bytef *)idat->data, idat->length);
	} else {
	      crc = crc32(crc, (Bytef *)&(chunk->data), chunk->length);
	}
	chunk->crc = htonl(crc);
}

/*
 * Specialized version of write_chunk for IEND: its values are all known in
 * advance so skip the additional calls to memcpy().
 * It returns the number of written bytes.
 */
size_t
write_IEND(uint8_t *buf)
{
	uint8_t		data[] = {
		    0x00, 0x00, 0x00, 0x00, // Length = 0
		    0x49, 0x45, 0x4e, 0x44, // IEND
		    0xae, 0x42, 0x60, 0x82  // CRC
		};

	(void)memcpy(buf, data, sizeof(data));
	return(12);
}

/*
 * Convert chunk->length in a format suitable for writing and write the
 * chunk in buffer buf.
 * It returns the number of written bytes.
 */
size_t
write_chunk(uint8_t *buf, struct chunk *chunk)
{
	uint32_t	length, crc;

	length = htonl(chunk->length);
	crc = htonl(chunk->crc);
	(void)memcpy(buf, &length, 4);
	(void)memcpy(buf + 4, chunktypemap[chunk->type], 4);
	if (CHUNK_TYPE_IDAT == chunk->type) {
		struct IDAT *idat;

		idat = (struct IDAT *)chunk;
		(void)memcpy(buf + 8, idat->data, idat->length);
	} else {
		(void)memcpy(buf + 8, &(chunk->data), chunk->length);
	}
	(void)memcpy(buf + 8 + chunk->length, &(chunk->crc), 4);
	return(12 + chunk->length);
}

