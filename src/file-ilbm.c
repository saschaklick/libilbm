/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * file-ilbm.c
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

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

int read_image(const char * filename);

const char LOAD_PROCEDURE_ILBM[] = "file-load-ilbm";

const char BINARY_NAME[] = "file-ilbm";

const char BULLET_CHAR[] = "\xE2\x80\xA2";

static void query(void);
static void run(const gchar *, gint, const GimpParam *, gint *, GimpParam **);

static const GimpParamDef load_arguments[] = {
    { GIMP_PDB_INT32,  "run-mode",     "Interactive, non-interactive" },
    { GIMP_PDB_STRING, "filename",     "The name of the file to load" },
    { GIMP_PDB_STRING, "raw-filename", "The name entered" }
};

static const GimpParamDef load_return_values[] = {
    { GIMP_PDB_IMAGE, "image", "Output image" }
};

GimpPlugInInfo PLUG_IN_INFO = {
    NULL,
    NULL,
    query,
    run
};

MAIN()

const char * ilbm_magics[] = { "FORM", "NEO!" };
char         magics[(sizeof(ilbm_magics) / sizeof(ilbm_magics[0])) * 18 + 1];

static void query(void) {
    gimp_install_procedure(LOAD_PROCEDURE_ILBM,
                           "Load ILBM images",
                           "Load ILBM images",
                           "--",
                           "Copyright Sascha Klick",
                           "2024",
                           "ILBM image",
                           NULL,
                           GIMP_PLUGIN,
                           G_N_ELEMENTS(load_arguments),
                           G_N_ELEMENTS(load_return_values),
                           load_arguments,
                           load_return_values);

    char * p_magic = magics;
    for(uint32_t i = 0; i < sizeof(ilbm_magics) / sizeof(ilbm_magics[0]); i++){
        p_magic += snprintf(p_magic, sizeof(magics) - (p_magic - &magics[0]), "0,long,0x%02x%02x%02x%02x,", ilbm_magics[i][0], ilbm_magics[i][1], ilbm_magics[i][2], ilbm_magics[i][3]);
    }        

    gimp_register_magic_load_handler(LOAD_PROCEDURE_ILBM, "lbm,ilbm,pbm", "", magics);                
}

static void run(const gchar * plugin_procedure_name, gint nparams, const GimpParam * param, gint * nreturn_vals, GimpParam ** return_vals) {
    static GimpParam return_values[2];
    GimpRunMode   run_mode;

    *nreturn_vals = 1;
    *return_vals  = return_values;
    run_mode      = param[0].data.d_int32;
    
    return_values[0].type          = GIMP_PDB_STATUS;
    return_values[0].data.d_status = GIMP_PDB_SUCCESS;
    
    if(!strcmp(plugin_procedure_name, LOAD_PROCEDURE_ILBM)) {
        int new_image_id;

        if(nparams != 3) {
            return_values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
            return;
        }
        
        gimp_ui_init(BINARY_NAME, false);

        new_image_id = read_image(param[1].data.d_string);

        if(new_image_id == -1) {
            return_values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;            
        }
        
        else {
            *nreturn_vals = 2;

            return_values[1].type         = GIMP_PDB_IMAGE;
            return_values[1].data.d_image = new_image_id;
        }
    }
            
    else {
        return_values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
    }

}

#include "libilbm.h"
#include "libilbm.c"

int read_image(const char * filename) {
    gint32 new_image_id,
           new_layer_id;
    GimpDrawable * drawable;
    GimpPixelRgn rgn;

    FILE * file_p = fopen(filename, "rb");
    if(file_p == NULL){
        return -1;
    }
    
    ilbm_image * p_first = ilbm_read(file_p, ILBM_FORMAT_AUTO);

    fclose(file_p);

    if(p_first == NULL) {
        gimp_message("Could not open ILBM image.\n");
        return -1;
    }
    
    uint32_t img_i = 0;
    uint32_t msg_i = 0;
    char msg[4096];

    new_image_id = -1;

    ilbm_image * p_img = p_first;
    while(p_img != NULL){
        if(p_img->error != ILBM_OK) {                        
            msg_i += snprintf(msg + msg_i, sizeof(msg) - msg_i, "%s ", BULLET_CHAR);
            int ret = ilbm_error_snprint(msg + msg_i, sizeof(msg) - msg_i, p_img);
            if(ret > 0){
                msg_i += ret;
                msg_i += snprintf(msg + msg_i, sizeof(msg) - msg_i, ".");    
            }else{
                msg_i += snprintf(msg + msg_i, sizeof(msg) - msg_i, "#%d", (int)(p_img->error));    
            }
            msg_i += snprintf(msg + msg_i, sizeof(msg) - msg_i, "\n\n");
        }else{
            bool alpha_as_mask = true;

            if(new_image_id == -1){
                new_image_id = gimp_image_new(p_img->width, p_img->height, GIMP_INDEXED);
            }

            new_layer_id = gimp_layer_new(new_image_id, "Image", p_img->width, p_img->height, p_img->alpha && !alpha_as_mask ? GIMP_INDEXEDA_IMAGE : GIMP_INDEXED_IMAGE, 100, GIMP_INDEXED_IMAGE);    
            drawable = gimp_drawable_get(new_layer_id);                                
            gimp_image_set_colormap(new_image_id, &(p_img->palette[0]), p_img->color_count);                
            gimp_pixel_rgn_init(&rgn, drawable, 0, 0, p_img->width, p_img->height, true, false);    
            if(p_img->alpha){
                if(alpha_as_mask){
                    gimp_pixel_rgn_set_rect(&rgn, p_img->pixels, 0, 0, p_img->width, p_img->height);
                    
                    gint32 mask_id = gimp_layer_create_mask(new_layer_id, GIMP_ADD_MASK_WHITE);

                    gimp_layer_add_mask(new_layer_id, mask_id);
                    
                    const uint8_t black[] = { 0x00, 0x00, 0x00 };
                    for(uint32_t i = 0; i < p_img->size; i++){
                        if(p_img->alpha[i] == 0){                        
                            gimp_drawable_set_pixel(mask_id, i % p_img->width, i / p_img->width, 1, black);                                                        
                        }
                    }                    
                }else{
                    uint8_t * pixels = (uint8_t *)malloc(2 * p_img->size);
                    if(pixels == NULL){
                        break;
                    }
                    for(uint32_t i = 0; i < p_img->size; i++){
                        pixels[i * 2 + 0] = p_img->pixels[i];
                        pixels[i * 2 + 1] = p_img->alpha[i];
                    }
                    gimp_pixel_rgn_set_rect(&rgn, pixels, 0, 0, p_img->width, p_img->height);
                }
            }else{
                gimp_pixel_rgn_set_rect(&rgn, p_img->pixels, 0, 0, p_img->width, p_img->height);
            }
            gimp_drawable_flush(drawable);
            gimp_drawable_detach(drawable);    
            gimp_image_insert_layer(new_image_id, new_layer_id, -1, 0);   

            if(p_img->warnings != 0){
                for(int warn_i = 0; warn_i < ILBM_WARN_EOL; warn_i++){
                    if(p_img->warnings & (1 << warn_i)){
                        msg_i += snprintf(msg + msg_i, sizeof(msg) - msg_i, "%s ", BULLET_CHAR);                            
                        int ret = ilbm_warn_snprint(msg + msg_i, sizeof(msg) - msg_i, p_img, warn_i);
                        if(ret > 0){
                            msg_i += ret;
                            msg_i += snprintf(msg + msg_i, sizeof(msg) - msg_i, ".");    
                        }else{
                            msg_i += snprintf(msg + msg_i, sizeof(msg) - msg_i, "#%d", warn_i);    
                        }
                        msg_i += snprintf(msg + msg_i, sizeof(msg) - msg_i, "\n");
                    }
                }

                if(p_img->next_image != NULL){
                    msg_i += snprintf(msg + msg_i, sizeof(msg) - msg_i, "\n\n");
                }
            }
        }

        p_img = p_img->next_image;
    }

    if(new_image_id != -1){
        gimp_image_set_filename(new_image_id, filename);
    }

    if(msg_i > 0){
        gimp_message(msg);
    }

    ilbm_free(p_img);

    return new_image_id;
}