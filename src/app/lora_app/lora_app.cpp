/****************************************************************************
 *   Mini LoRa app for the T-Watch S3 Plus (SX1262)
 ****************************************************************************/
/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
#include "config.h"

#include "lora_app.h"
#include "lora_app_main.h"

#include "gui/mainbar/mainbar.h"
#include "gui/statusbar.h"
#include "gui/app.h"

#ifdef NATIVE_64BIT
    #include "utils/logging.h"
#else
    #include <Arduino.h>
#endif

uint32_t lora_app_main_tile_num;
icon_t *lora_app = NULL;

/*
 * placeholder icon - reuses an existing 64px asset until a dedicated LoRa
 * icon is added under images/.
 */
LV_IMG_DECLARE(wifimon_app_64px);

static void enter_lora_app_event_cb( lv_obj_t * obj, lv_event_t event );

/*
 * automatic register the app setup function
 */
static int registed = app_autocall_function( &lora_app_setup, 30 );           /** @brief app autocall function */

void lora_app_setup( void ) {
    if( !registed ) {
        return;
    }
    /*
     * the SX1262 only exists on the T-Watch S3 Plus, so only show the app there
     */
    #if !defined( LILYGO_WATCH_S3_PLUS )
        return;
    #endif

    lora_app_main_tile_num = mainbar_add_app_tile( 1, 1, "lora app" );
    lora_app = app_register( "LoRa", &wifimon_app_64px, enter_lora_app_event_cb );
    lora_app_main_setup( lora_app_main_tile_num );
}

uint32_t lora_app_get_app_main_tile_num( void ) {
    return( lora_app_main_tile_num );
}

static void enter_lora_app_event_cb( lv_obj_t * obj, lv_event_t event ) {
    switch( event ) {
        case( LV_EVENT_CLICKED ):       app_hide_indicator( lora_app );
                                        mainbar_jump_to_tilenumber( lora_app_main_tile_num, LV_ANIM_OFF );
                                        statusbar_hide( true );
                                        break;
    }
}
