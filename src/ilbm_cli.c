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

void print_img(ilbm_image * p_img, uint32_t col_max, double aspect, uint32_t charset_no);

int main(int argv, char **args){
    
    if(argv != 2){
        printf("Usage: %s <filename>", args[0]);
        return 1;
    }
    
    FILE * file_p = fopen(args[1], "rb");

    ilbm_image * p_img = ilbm_read(file_p);

    fclose(file_p);
    
    if(p_img->error == ILBM_OK){
        log_info("successfully parsed");

        print_img(p_img, 120, 4.0 / 2.0, 0);

        return 0;
    }else{
        char err_str[64];
        ilbm_error_snprint(err_str, sizeof(err_str), p_img);
        log_error("parsing failed: %s", err_str);
        return 1;
    }

    ilbm_free(p_img);
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