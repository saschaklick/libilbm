/* ilbmlib.h
 * Copyright (C) 2024 Sascha Klick <sascha.klick@github.com>
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <https://www.gnu.org/licenses/>.
 */

#ifndef LIBILBM_H
#define LIBILBM_H

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#define STR(a) #a
#define XSTR(a) STR(a)

#define LIBILBM_VER_MAJ 0
#define LIBILBM_VER_MIN 1
#define LIBILBM_VER_REV 3
#define LIBILBM_VERSION XSTR(LIBILBM_VER_MAJ) "." XSTR(LIBILBM_VER_MIN) "." XSTR(LIBILBM_VER_REV)

#ifndef LIBILBM_VERBOSITY
    #define LIBILBM_VERBOSITY 0
#endif

enum {
    ILBM_FORMAT_ILBM,
    ILBM_FORMAT_PBM,
    ILBM_FORMAT_AUTO,
    ILBM_FORMAT_EOL
} typedef ILBM_FORMAT;

const char * ilbm_format_strs[] = { "ILBM", "PBM" };

enum {
    ILBM_BMHD,
    ILBM_BODY,
    ILBM_CAMG,
    ILBM_CMAP,
    ILBM_CRNG,
    ILBM_CCRT,
    ILBM_DEST,
    ILBM_GRAB,
    ILBM_SPRT,
    ILBM_TINY,
    ILBM_UKWN,
    ILBM_EOL
} typedef ILBM_CHUNK;

enum {
    ILBM_OK,
    ILBM_ERROR_ILLEGAL_WIDTH,
    ILBM_ERROR_ILLEGAL_HEIGHT,
    ILBM_ERROR_NO_CHUNKS,
    ILBM_ERROR_FORM_MISSING,
    ILBM_ERROR_BMHD_MISSING,
    ILBM_ERROR_BODY_MISSING,
    ILBM_ERROR_CMAP_MISSING,
    ILBM_ERROR_BODY_SHORT_REPEAT,
    ILBM_ERROR_BODY_SHORT_LITERAL,    
    ILBM_ERROR_EOL
} typedef ILBM_ERROR;

const char * ilbm_error_strs[] = { "OK", "Illegal width", "Illegal height", "No chunks found", "Magic missing", "Header missing", "Body missing", "Colormap missing", "Short repeat in body", "Short literal in body" };

enum {
    ILBM_WARN_FORM_BY_POSITION,
    ILBM_WARN_BHMD_BY_POSITION,
    ILBM_WARN_BHMD_SIZE_MISMATCH,
    ILBM_WARN_BODY_BY_SIZE,
    ILBM_WARN_CMAP_BY_EXACT_SIZE,
    ILBM_WARN_CMAP_BY_MIN_SIZE,
    ILBM_WARN_EOL
} typedef ILBM_WARNING;

struct ilbm_chunk {
    const ILBM_CHUNK    type;
    char                name[4];
    uint32_t            addr;
    uint32_t            size;    
    uint8_t           * content;
    struct ilbm_chunk * next_chunk;
} typedef ilbm_chunk;

struct __attribute__((packed)) {
    uint16_t width;
    uint16_t height;
    int16_t  x_origin;
    int16_t  y_origin;
    uint8_t  num_planes;
    uint8_t  mask;
    uint8_t  compression;
    uint8_t  pad1;
    uint16_t trans_clr;
    uint8_t  x_aspect;
    uint8_t  y_aspect;
    int16_t  page_width;
    int16_t  page_height;
} typedef ilbm_head;

struct ilbm_image {
    ILBM_FORMAT         format;
    uint32_t            width;
    uint32_t            height;
    uint32_t            size;
    uint8_t *           pixels;
    uint32_t            color_count;
    uint8_t *           palette;    
    uint8_t *           alpha;
    ilbm_chunk *        first_chunk;
    ilbm_chunk *        form_chunk;
    ilbm_chunk *        bmhd_chunk;
    ilbm_chunk *        body_chunk;
    ilbm_chunk *        cmap_chunk;
    ILBM_ERROR          error;    
    uint32_t            warnings;
    struct ilbm_image * next_image;
} typedef ilbm_image;

ilbm_image * ilbm_read(FILE *file_p, ILBM_FORMAT format);

void ilbm_free(ilbm_image * p_img);

int ilbm_error_snprint(char *buf, size_t len, ilbm_image * p_img);

int ilbm_warn_snprint(char *buf, size_t len, ilbm_image * p_img, ILBM_WARNING warning);

void log_set_verbosity(int verbosity);

void log_dev(const char *format, ...);

void log_info(const char *format, ...);

void log_warning(const char *format, ...);

void log_error(const char *format, ...);

#endif