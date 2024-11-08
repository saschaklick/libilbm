/* ilbmlib.c
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

#include <stdlib.h>
#include <string.h>

#include "libilbm.h"

int log_verbosity = LIBILBM_VERBOSITY;

#define UINT32_BE(v) ( (((v >> 24) & 0xff) << 0) | (((v >> 16) & 0xff) << 8) | (((v >> 8) & 0xff) << 16) | (((v >> 0) & 0xff) << 24) )
#define UINT16_BE(v) ( (((v >> 8) & 0xff) << 0) | (((v >> 0) & 0xff) << 8) )
#define INT16_BE(v)  ( (((v >> 8) & 0xff) << 0) | (((v >> 0) & 0xff) << 8) )

ilbm_chunk * ilbm_read_chunk(FILE * file_p) {
    fpos_t pos;
    if(fgetpos(file_p, &pos) != 0){
        log_error("chunk pos failed");
        return NULL;
    }   

    char name[4];
    if(fread(name, 4, 1, file_p) != 1){                        
        return NULL;
    }

    ilbm_chunk * p_chunk = (ilbm_chunk *)malloc(sizeof(ilbm_chunk));  
    if(p_chunk == NULL){
        log_error("chunk malloc failed");
        return NULL;
    }
    
    memcpy(p_chunk->name, name, 4);
    p_chunk->addr = pos.__pos;

    if(fread((void *)&p_chunk->size, 4, 1, file_p) != 1){
        log_error("size read failed");
        free(p_chunk);
        return NULL;
    }

    p_chunk->size = UINT32_BE(p_chunk->size);
    p_chunk->next_chunk = NULL;

    uint32_t c_size = p_chunk->size;
    
    if(pos.__pos == 0){
        c_size = 4;
    }
    p_chunk->content = (uint8_t *)malloc(c_size); 
    if(p_chunk->content == NULL){
        log_error("content malloc failed");
        free(p_chunk);
        return NULL;
    }

    if(fread(p_chunk->content, c_size, 1, file_p) != 1){
        log_error("content read failed");
        free(p_chunk->content);
        free(p_chunk);
        return NULL;
    }

    if((c_size & 1) && !feof(file_p)){        
        if(fseek(file_p, 1, SEEK_CUR) != 0){
            log_error("pad seek failed");
            free(p_chunk->content);
            free(p_chunk);
            return NULL;
        } 
    }
    
    return p_chunk;
}

ilbm_image * ilbm_read(FILE *file_p, ILBM_FORMAT format) {

    log_info("libilbm %s", LIBILBM_VERSION);

    if(file_p == NULL){        
        return NULL;
    }
    
    ilbm_image * p_img   = (ilbm_image *)malloc(sizeof(ilbm_image));  
    if(p_img == NULL){
        log_error("malloc failed");
        return NULL;
    }

    p_img->format = ILBM_FORMAT_ILBM;
    p_img->error = ILBM_OK;    
    p_img->warnings = 0;
    p_img->next_image = NULL;
    p_img->pixels = NULL;
    p_img->palette = NULL;
    p_img->alpha = NULL;

    p_img->first_chunk = NULL;
    p_img->form_chunk = ilbm_read_chunk(file_p);
    p_img->bmhd_chunk = NULL;
    p_img->body_chunk = NULL;
    p_img->cmap_chunk = NULL;

    if(p_img->form_chunk == NULL){
        p_img->error = ILBM_ERROR_FORM_MISSING;
        return p_img;
    }

    if(*(uint32_t *)(p_img->form_chunk->name) != *(uint32_t *)"FORM"){
        p_img->warnings |= (1 << ILBM_WARN_FORM_BY_POSITION);        
    }

    if(format == ILBM_FORMAT_PBM || memcmp(p_img->form_chunk->content, "PBM ", 4) == 0){
        p_img->format = ILBM_FORMAT_PBM;
    }

    uint32_t chunk_cnt = 0;    
    ilbm_chunk * chunk = NULL;
    while (1) {
        ilbm_chunk * c = ilbm_read_chunk(file_p);        
        if(c != NULL){
            if(p_img->first_chunk == NULL){
                p_img->first_chunk = c;                
            }else{
                chunk->next_chunk = c;
            }
            chunk = c;            
        
            log_info("chunk %2d: \"%4.4s\" addr: %d size: %d", chunk_cnt, chunk->name, chunk->addr, chunk->size);            

            chunk_cnt++;
        }else{
            break;
        }
        chunk = c;
    }    

    if(chunk_cnt < 3){        
        p_img->error = ILBM_ERROR_NO_CHUNKS;
        return p_img;
    }

    ilbm_chunk * bmhd_chunk = p_img->first_chunk;
    while(bmhd_chunk != NULL){
        if(*(uint32_t *)(bmhd_chunk->name) == *(uint32_t *)"BMHD"){        
            break;
        }
        bmhd_chunk = bmhd_chunk->next_chunk;
    }
    if(bmhd_chunk == NULL){
        bmhd_chunk = p_img->first_chunk;
        while(bmhd_chunk != NULL){
            if(bmhd_chunk->size == sizeof(ilbm_head)) { 
                p_img->warnings |= (1 << ILBM_WARN_BHMD_BY_POSITION);                
                break;
            }
            bmhd_chunk = bmhd_chunk->next_chunk;
        }
    }
    if(bmhd_chunk == NULL){
        bmhd_chunk = p_img->first_chunk;
        p_img->warnings |= (1 << ILBM_WARN_BHMD_BY_POSITION) | (1 << ILBM_WARN_BHMD_SIZE_MISMATCH);        
    }
    p_img->bmhd_chunk = bmhd_chunk;
    
    ilbm_head bmhd;
    memcpy(&bmhd,  p_img->first_chunk->content, sizeof(bmhd));

    bmhd.width = UINT16_BE(bmhd.width);
    bmhd.height = UINT16_BE(bmhd.height);
    bmhd.trans_clr = UINT16_BE(bmhd.trans_clr);
    bmhd.page_width = INT16_BE(bmhd.page_width);
    bmhd.page_height = INT16_BE(bmhd.page_height);
    
    log_info("format      : %s", ilbm_format_strs[p_img->format]);

    log_info("width       : %i", bmhd.width);
    log_info("height      : %i", bmhd.height);
    log_info("x_origin    : %i", bmhd.x_origin);
    log_info("y_origin    : %i", bmhd.y_origin);

    log_info("num_planes  : %i", bmhd.num_planes);
    log_info("mask        : %i", bmhd.mask);
    log_info("compression : %i", bmhd.compression);
    log_info("pad1        : %i", bmhd.pad1);

    log_info("trans_clr   : %i", bmhd.trans_clr);
    log_info("x_aspect    : %i", bmhd.x_aspect);
    log_info("y_aspect    : %i", bmhd.y_aspect);
    log_info("page_width  : %i", bmhd.page_width);
    log_info("page_height : %i", bmhd.page_height);

    p_img->width = bmhd.width;
    p_img->height = bmhd.height;
    p_img->size = p_img->width * p_img->height; 

    if(p_img->width == 0 || p_img->width > 1024){
        p_img->error = ILBM_ERROR_ILLEGAL_WIDTH;
        return p_img;
    }

    if(p_img->height == 0 || p_img->height > 768){
        p_img->error = ILBM_ERROR_ILLEGAL_WIDTH;
        return p_img;
    }

    if(bmhd.mask != 0){
        p_img->alpha = (uint8_t *)malloc(p_img->size);
        if(p_img->alpha == NULL){
            log_error("alpha malloc failed");        
            return p_img;
        }        
        memset(p_img->alpha, 0xff, p_img->size);
    }

    ilbm_chunk * body_chunk = p_img->first_chunk;
    while(body_chunk != NULL){
        if(*(uint32_t *)(body_chunk->name) == *(uint32_t *)"BODY"){        
            break;    
        }
        body_chunk = body_chunk->next_chunk;        
    }
    if(body_chunk == 0){        
        ilbm_chunk * chunk = p_img->first_chunk;                
        uint32_t size_max = 0;
        while(chunk != NULL){
            if(chunk->size >= size_max){
                size_max = chunk->size;
                body_chunk = chunk;
            }
            chunk = chunk->next_chunk;        
        }
        if(body_chunk != bmhd_chunk){
            p_img->warnings |= (1 << ILBM_WARN_BODY_BY_SIZE);            
        }else{            
            p_img->error = ILBM_ERROR_BODY_MISSING;
            return p_img;
        }
    }
    p_img->body_chunk = body_chunk;

    p_img->pixels = (uint8_t *)malloc(p_img->size * sizeof(uint32_t));
    if(p_img->pixels == NULL){
        log_error("pixels malloc failed");        
        return p_img;
    }
    memset(p_img->pixels, 0, p_img->size * sizeof(uint32_t));

    uint8_t * pixels = p_img->pixels;
    uint32_t plane_no = 0;
    uint32_t row_no = 0;
    uint32_t col_no = 0;
    uint32_t byte_i = 0;
    if(bmhd.compression == 0){
        
        while(1){
            if(byte_i >= body_chunk->size){                
                break;
            }

            uint8_t byte = body_chunk->content[byte_i];                        
            byte_i++;            

            if(col_no >= p_img->width){
                col_no = 0;
                plane_no++;
                if(plane_no == bmhd.num_planes){
                    plane_no = 0;
                    row_no++;
                    if(row_no == bmhd.height){
                        break;
                    }
                }
            }

            for(uint32_t ii = 0; ii < 8; ii++){                        
                if(col_no >= p_img->width) break;

                if(byte & (0x80 >> ii)){
                    p_img->pixels[row_no * p_img->width + col_no] |= (1 << plane_no);
                }
                col_no++;
            }       
        }

    }else{
        while(1){
            if(byte_i >= body_chunk->size){                
                break;
            }

            uint8_t byte = body_chunk->content[byte_i];                        
            byte_i++;
            
            log_dev("act: %c len: %3d next: %3d| r: %3d c: %3d p: %d", byte > 128 ? 'r' : byte < 128 ? 'l' : 'e', byte > 128 ? 257 - byte : byte + 1, body_chunk->content[byte_i], row_no, col_no, plane_no);

            if(byte > 128){
                if(byte_i >= body_chunk->size){
                    p_img->error = ILBM_ERROR_BODY_SHORT_REPEAT;                    
                    break;
                }

                uint8_t repeat = body_chunk->content[byte_i];
                byte_i++;
                
                switch(p_img->format){
                    case ILBM_FORMAT_ILBM:
                        for(uint32_t i = 0; i < 257 - byte; i++){                    
                            for(uint32_t ii = 0; ii < 8; ii++){                        
                                if(col_no >= p_img->width){
                                    col_no = 0;
                                    plane_no++;
                                    if(plane_no == bmhd.num_planes){
                                        plane_no = 0;
                                        row_no++;
                                    }
                                }
                                
                                if(repeat & (0x80 >> ii)){
                                    p_img->pixels[row_no * p_img->width + col_no] |= (1 << plane_no);                            
                                }                        
                                col_no++;                      
                            }
                        }                        
                        break;
                    case ILBM_FORMAT_PBM:
                        for(uint32_t i = 0; i < 257 - byte; i++){                                                
                            if(col_no >= p_img->width){
                                col_no = 0;
                                row_no++;
                            }
                                
                            p_img->pixels[row_no * p_img->width + col_no] = repeat;                                                        
                            col_no++;                      
                        }                        
                        break;
                }
            }else
            if(byte < 128){
                for(uint32_t i = 0; i < (byte + 1); i++){
                    if(byte_i > body_chunk->size){                        
                        p_img->error = ILBM_ERROR_BODY_SHORT_LITERAL;
                        break;
                    }

                    uint8_t literal = body_chunk->content[byte_i];
                    byte_i++;

                    switch(p_img->format){
                        case ILBM_FORMAT_ILBM:
                            for(uint32_t ii = 0; ii < 8; ii++){                        
                                if(col_no >= p_img->width){
                                    col_no = 0;
                                    plane_no++;
                                    if(plane_no == bmhd.num_planes){
                                        plane_no = 0;
                                        row_no++;
                                    }
                                }

                                if(literal & (0x80 >> ii)){
                                    p_img->pixels[row_no * p_img->width + col_no] |= (1 << plane_no);
                                }
                                col_no++;
                            }                    
                            break;
                        case ILBM_FORMAT_PBM:
                            if(col_no >= p_img->width){
                                col_no = 0;
                                row_no++;
                            }

                            p_img->pixels[row_no * p_img->width + col_no] = literal;
                            
                            col_no++;                    
                            break;
                    }
                }
            }else{
                break;
            }            
        }

    }

    uint32_t color_max = 0;
    for(uint32_t i = 0; i < p_img->size; i++){
        if(p_img->pixels[i] > color_max) color_max = p_img->pixels[i];
    }

    if(bmhd.mask == 2){
        for(uint32_t i = 0; i < p_img->size; i++){
            if(p_img->pixels[i] == bmhd.trans_clr){
                p_img->alpha[i] = 0x00;
            }
        }
    }

    ilbm_chunk * cmap_chunk = p_img->first_chunk;
    while(cmap_chunk != NULL){
        if(*(uint32_t *)(cmap_chunk->name) == *(uint32_t *)"CMAP"){                
            break;    
        }
        cmap_chunk = cmap_chunk->next_chunk;        
    }
    if(cmap_chunk == NULL){
        cmap_chunk = p_img->first_chunk;
        while(cmap_chunk != NULL){                
            if((cmap_chunk != bmhd_chunk) && (cmap_chunk->size == (1 << bmhd.num_planes) * 3)){
                p_img->warnings |= (1 << ILBM_WARN_CMAP_BY_EXACT_SIZE);                
                break;    
            }
            cmap_chunk = cmap_chunk->next_chunk;        
        }
    }
    if(cmap_chunk == NULL){
        cmap_chunk = p_img->first_chunk;
        while(cmap_chunk != NULL){            
            if((cmap_chunk != bmhd_chunk) && (cmap_chunk->size >= color_max * 3)){
                p_img->warnings |= (1 << ILBM_WARN_CMAP_BY_MIN_SIZE);                
                break;    
            }
            cmap_chunk = cmap_chunk->next_chunk;        
        }
    }

    if(cmap_chunk == NULL){        
        p_img->error = ILBM_ERROR_CMAP_MISSING;
        return p_img;
    }
    p_img->cmap_chunk = cmap_chunk;

    p_img->color_count = cmap_chunk->size / 3;
    p_img->palette = (uint8_t *)malloc(cmap_chunk->size);
    if(p_img->palette == NULL){
        log_error("palette malloc failed");        
        return p_img;
    }
    memcpy(p_img->palette, cmap_chunk->content, cmap_chunk->size);
    
    if(log_verbosity >= 2){
        for(int warn_i = 0; warn_i < ILBM_WARN_EOL; warn_i++){
            if(p_img->warnings & (1 << warn_i)){
                char warn_str[64];
                int ret = ilbm_warn_snprint(warn_str, sizeof(warn_str), p_img, warn_i);
                if(ret > 0){
                    log_warning(warn_str);
                }else{
                    log_warning("%d", warn_i);
                }
            }
        }
    }

    return p_img;
}

void ilbm_free(ilbm_image * p_first_img) {
    ilbm_image * p_img = p_first_img;

    while(p_img != NULL){
        ilbm_chunk * p_chunk = p_img->first_chunk;
        while(p_chunk != NULL){
            ilbm_chunk * p_tmp = (void *)p_chunk;                    

            if(p_chunk->content != NULL) free(p_chunk->content);
            p_chunk = p_chunk->next_chunk;
            free(p_tmp);
        }
        
        if(p_img->pixels != NULL) free((void *)p_img->pixels);
        if(p_img->palette != NULL) free((void *)p_img->palette);
        if(p_img->alpha != NULL) free((void *)p_img->alpha);
        
        ilbm_image * p_tmp = p_img;        
        
        p_img = p_img->next_image;

        free((void *)p_tmp);        
    }
}

int ilbm_error_snprint(char *buf, size_t len, ilbm_image * p_img) {
    switch(p_img->error){
        case ILBM_OK: return snprintf(buf, len, "OK");
        case ILBM_ERROR_NO_CHUNKS: return snprintf(buf, len, "No basic chunk structures found");
        case ILBM_ERROR_FORM_MISSING: return snprintf(buf, len, "First chunk \"FORM\" missing");
        case ILBM_ERROR_BMHD_MISSING: return snprintf(buf, len, "Header chunk \"BMHD\" missing");
        case ILBM_ERROR_BODY_MISSING: return snprintf(buf, len, "Bitmap chunk \"BODY\" missing");
        case ILBM_ERROR_CMAP_MISSING: return snprintf(buf, len, "Palette chunk \"CMAP\" missing");
        case ILBM_ERROR_BODY_SHORT_REPEAT: return snprintf(buf, len, "Overflow in stream repeat");
        case ILBM_ERROR_BODY_SHORT_LITERAL: return snprintf(buf, len, "Overflow in stream literal");
    }
    return 0;
}

int ilbm_warn_snprint(char *buf, size_t len, ilbm_image * p_img, ILBM_WARNING warning) {
    switch(warning){
        case ILBM_WARN_FORM_BY_POSITION: if(p_img->form_chunk->name != NULL) return snprintf(buf, len, "First chunk \"%4.4s\" instead of \"FORM\"", p_img->form_chunk->name);
        case ILBM_WARN_BHMD_BY_POSITION: if(p_img->bmhd_chunk->name != NULL) return snprintf(buf, len, "Header chunk \"%4.4s\" instead of \"BHMD\"", p_img->bmhd_chunk->name);
        case ILBM_WARN_BHMD_SIZE_MISMATCH: return snprintf(buf, len, "Header chunk larger as expected");
        case ILBM_WARN_BODY_BY_SIZE: if(p_img->body_chunk->name != NULL) return snprintf(buf, len, "Planes chunk \"%4.4s\" instead of \"BODY\"", p_img->body_chunk->name);
        case ILBM_WARN_CMAP_BY_EXACT_SIZE: if(p_img->cmap_chunk->name != NULL) return snprintf(buf, len, "Palette chunk \"%4.4s\" instead of \"CMAP\"", p_img->cmap_chunk->name);
        case ILBM_WARN_CMAP_BY_MIN_SIZE: if(p_img->cmap_chunk->name != NULL) return snprintf(buf, len, "Oversized palette chunk \"%4.4s\" instead of \"CMAP\"", p_img->cmap_chunk->name);
    }
    return 0;
}

void log_set_verbosity(int verbosity) {
    log_verbosity = verbosity;
}

void log_dev(const char *format, ...) {
    if(log_verbosity < 4) return;
    
    va_list args;
    va_start(args, format);

    printf("* libilbm ");
    printf("[DEV    ] ");
    vprintf(format, args);
    printf("\n");

    va_end(args);
}

void log_info(const char *format, ...) {
    if(log_verbosity < 3) return;
    
    va_list args;
    va_start(args, format);

    printf("* libilbm ");
    printf("[INFO   ] ");
    vprintf(format, args);
    printf("\n");

    va_end(args);
}

void log_warning(const char *format, ...) {
    if(log_verbosity < 2) return;

    va_list args;
    va_start(args, format);

    printf("* libilbm ");
    printf("[WARNING] ");
    vprintf(format, args);
    printf("\n");

    va_end(args);
}

void log_error(const char *format, ...) {
    if(log_verbosity < 1) return;

    va_list args;
    va_start(args, format);

    printf("* libilbm ");
    printf("[ERROR  ] ");
    vprintf(format, args);
    printf("\n");

    va_end(args);
}