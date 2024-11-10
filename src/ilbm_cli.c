/* ilbm_cli.c
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

#define LIBILBM_VERBOSITY 3

#include "libilbm.h"
#include "libilbm.c"

#include <stdio.h>
#include <glob.h>
#include <strings.h>

void print_img(ilbm_image * p_img, uint32_t col_max, double aspect, uint32_t charset_no);

int main(int argc, char **argv){

    if(argc < 2){
        printf("Usage: %s [-vvv] <filename/pattern>\n", argv[0]);
        return 1;
    }

    const int VERBOSE =
        strncmp(argv[1], "-vvv", 5) == 0 ? 3 :
        strncmp(argv[1], "-vv", 4) == 0 ? 2 :
        strncmp(argv[1], "-v", 3) == 0 ? 1 : 0
    ;

    switch(VERBOSE){
        case 0: log_set_verbosity(0); break;
        case 3: log_set_verbosity(4); break;
        default: break;
    }
    
    if(VERBOSE == 0){
        log_set_verbosity(0);
    }

    for(int arg_i = 1; arg_i < argc; arg_i++){

        if(argv[arg_i][0] == '-'){
            continue;
        }

        glob_t globbuf;    
        
        if(glob(argv[arg_i], 0, NULL, &globbuf) == 0){
            char ** pathv = globbuf.gl_pathv;

            for(; *pathv; pathv++){
                FILE * file_p = fopen(*pathv, "rb");

                if(file_p){

                    const char *ext = strrchr(*pathv, '.');

                    int use_lbm = 0;
                    if(ext != NULL && strncasecmp(ext + 1, "LBM", 4) == 0){
                        use_lbm = 1;
                    }

                    ilbm_image * p_img = ilbm_read(file_p, use_lbm ? ILBM_FORMAT_PBM : ILBM_FORMAT_AUTO);

                    fclose(file_p);
                    
                    switch(p_img->error){
                        case ILBM_OK:
                            printf("%s: %dx%d [%c%c%c%c]\n", *pathv, p_img->width, p_img->height, p_img->form_chunk->name[0], p_img->form_chunk->name[1], p_img->form_chunk->name[2], p_img->form_chunk->name[3]);                    
                            if(VERBOSE >= 2){
                                print_img(p_img, 120, 4.0 / 2.0, 0);                    
                            }
                            break;
                        case ILBM_ERROR_BODY_SHORT_LITERAL:
                        case ILBM_ERROR_BODY_SHORT_REPEAT:
                            printf("%s: %dx%d [magic: %c%c%c%c] Compression error\n", *pathv, p_img->width, p_img->height, p_img->form_chunk->name[0], p_img->form_chunk->name[1], p_img->form_chunk->name[2], p_img->form_chunk->name[3]);                    
                            break;
                        case ILBM_ERROR_BMHD_MISSING:
                        case ILBM_ERROR_CMAP_MISSING:
                        case ILBM_ERROR_BODY_MISSING:
                            printf("%s: %dx%d [magic: %c%c%c%c] Mandatory chunk missing\n", *pathv, p_img->width, p_img->height, p_img->form_chunk->name[0], p_img->form_chunk->name[1], p_img->form_chunk->name[2], p_img->form_chunk->name[3]);                    
                            break;
                        case ILBM_ERROR_ILLEGAL_HEIGHT:
                        case ILBM_ERROR_ILLEGAL_WIDTH:                        
                            printf("%s: %dx%d [magic: %c%c%c%c] Illegal header value(s)\n", *pathv, p_img->width, p_img->height, p_img->form_chunk->name[0], p_img->form_chunk->name[1], p_img->form_chunk->name[2], p_img->form_chunk->name[3]);                    
                            break;
                        default:
                            char err_str[64];
                            snprintf(err_str, sizeof(err_str), "%s", ilbm_error_strs[p_img->error]);
                            log_error("%s: parsing failed: %s\n", *pathv, err_str);
                            break;
                    }                    

                    ilbm_free(p_img);        
                }else{
                    log_error("%s: failed to open file\n", *pathv);
                }
            }                         
        }

        globfree(&globbuf);
    }

   return 0;
}

void print_img(ilbm_image * p_img, uint32_t col_max, double aspect, uint32_t charset_no) {
    double   fac = col_max > p_img->width ? 1.0 : (double)col_max / p_img->width;
    double   fac_y = fac / aspect;
    
    const char * charsets[] = {
        /* Source: https://paulbourke.net/dataformats/asciiart/ */
        " .:-=+*#%@",
        " .\'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$",        
        " .`:,;\'_^\"></-!~\\=)(|j?}{ ][ti+l7v1%yrfcJ32uIC$zwo96sngaT5qpkYVOL40&mG8*xhedbZUSAQPFDXWK#RNEHBM@"
    };

    const char * charset = charsets[charset_no % 3];
    const uint32_t charset_len = strlen(charset);

    printf(".-");
    for(uint32_t col = 0; col < p_img->width * fac; col++) printf("-");
    printf("-.\n");
    for(uint32_t row = 0; row < p_img->height * fac_y; row++){
        printf(": ");
        const uint32_t row_i = ((uint32_t)(row / fac_y)) * p_img->width;            
        for(uint32_t col = 0; col < p_img->width * fac; col++){
            uint32_t p_i = row_i + (uint32_t)(col / fac);                
            uint32_t c_i = p_img->pixels[p_i];
            uint8_t * color = &p_img->palette[c_i * 3];
            uint32_t intensity = (color[0] + color[1] + color[2]) / 3;
            if(p_img->alpha != NULL && p_img->alpha[p_i] == 0) intensity = 0;
            
            printf("%c", charset[((charset_len - 1) * intensity / 255)]);
        }
        printf(" :\n");
    }
    printf("`-");
    for(uint32_t col = 0; col < p_img->width * fac; col++) printf("-");
    printf("-'\n");
}