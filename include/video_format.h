/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2024 Jiaxun Yang <jiaxun.yang@flygoat.com>
 */

#ifndef _VIDEO_FORMAT_H_
#define _VIDEO_FORMAT_H_

#include <compiler.h>

/*
 * Bits per pixel selector. Each value n is such that the bits-per-pixel is
 * 2 ^ n
 */
enum video_log2_bpp {
	VIDEO_BPP8 = 3,
	VIDEO_BPP16,
	VIDEO_BPP32,
};

/*
 * Convert enum video_log2_bpp to bytes and bits. Note we omit the outer
 * brackets to allow multiplication by fractional pixels.
 */
#define VNBYTES(bpix)	((1 << (bpix)) / 8)

#define VNBITS(bpix)	(1 << (bpix))

/* Struct video_rgb - Describes a video colour, always 8 bpc */
struct video_rgb {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

/* Naming respects linux/include/uapi/drm/drm_fourcc.h */
enum video_format {
	VIDEO_DEFAULT = 0,
	VIDEO_RGB332,		/* [7:0] R:G:B 3:3:2 */
	VIDEO_RGB565,		/* [15:0] R:G:B 5:6:5 little endian */
	VIDEO_RGB565_BE,	/* [15:0] R:G:B 5:6:5 big endian */
	VIDEO_XRGB8888,		/* [31:0] x:R:G:B 8:8:8:8 little endian */
	VIDEO_BGRX8888,		/* [31:0] B:G:R:x 8:8:8:8 little endian */
	VIDEO_XRGB8888_BE = VIDEO_BGRX8888,
	VIDEO_XBGR8888,		/* [31:0] x:B:G:R 8:8:8:8 little endian */
	VIDEO_RGBA8888,		/* [31:0] R:G:B:A 8:8:8:8 little endian */
	VIDEO_XRGB2101010,	/* [31:0] x:R:G:B 2:10:10:10 little endian */
	VIDEO_XRGB2101010_BE,	/* [31:0] x:R:G:B 2:10:10:10 big endian */
	VIDEO_FMT_END
};

/**
 * video_rgb_to_pixel8() - convert a RGB color to a 8 bit pixel's
 * memory representation.
 *
 * @video_format Format of pixel
 * @rgb          RGB color
 * Return:	 color value
 */
static inline uint8_t video_rgb_to_pixel8(enum video_format format,
					  struct video_rgb rgb)
{
	switch (format) {
	case VIDEO_DEFAULT:
	case VIDEO_RGB332:
		return ((rgb.r >> 5) << 5) |
		       ((rgb.g >> 5) << 2) |
		       (rgb.b >> 6);
	default:
		break;
	}
	return 0;
}

/**
 * video_rgb_to_pixel16() - convert a RGB color to a 16 bit pixel's
 * memory representation.
 *
 * @video_format Format of pixel
 * @rgb          RGB color
 * Return:	 color value
 */
static inline uint16_t video_rgb_to_pixel16(enum video_format format,
					    struct video_rgb rgb)
{
	unsigned int val = 0;

	/* Handle layout first */
	switch (format) {
	case VIDEO_DEFAULT:
	case VIDEO_RGB565:
	case VIDEO_RGB565_BE:
		val = ((rgb.r >> 3) << 11) |
		      ((rgb.g >> 2) <<  5) |
		      ((rgb.b >> 3) <<  0);
		break;
	default:
		break;
	}

	/* Then handle endian */
	switch (format) {
	case VIDEO_RGB565_BE:
		return cpu_to_be16(val);
	default:
		return cpu_to_le16(val);
	}

}

/**
 * video_rgb_to_pixel32() - convert a RGB color to a 32 bit pixel's
 * memory representation.
 *
 * @video_format Format of pixel
 * @rgb          RGB color
 * Return:	 color value
 */
static inline uint32_t video_rgb_to_pixel32(enum video_format format,
					    struct video_rgb rgb)
{
	unsigned int val = 0;

	/* Handle layout first */
	switch (format) {
#ifdef __LITTLE_ENDIAN
	case VIDEO_DEFAULT:
#endif
	case VIDEO_XRGB8888:
		val = (rgb.r << 16) | (rgb.g << 8) | rgb.b;
		break;
#ifdef __BIG_ENDIAN
	case VIDEO_DEFAULT:
#endif
	case VIDEO_BGRX8888:
		val = (rgb.b << 24) | (rgb.g << 8) | (rgb.r << 8);
		break;
	case VIDEO_XBGR8888:
		val = (rgb.b << 16) | (rgb.g << 8) | rgb.b;
		break;
	case VIDEO_RGBA8888:
		val = (rgb.r << 24) | (rgb.g << 16) | (rgb.b << 8) | 0xff;
		break;
	case VIDEO_XRGB2101010:
	case VIDEO_XRGB2101010_BE:
		val = (rgb.r << 22) | (rgb.g << 12) | (rgb.b << 2);
	default:
		break;
	}

	/* Then handle endian */
	switch (format) {
	case VIDEO_XRGB2101010_BE:
		return cpu_to_be32(val);
	default:
		return cpu_to_le32(val);
	}

	return 0;
}
#endif
