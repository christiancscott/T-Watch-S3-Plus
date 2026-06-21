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

#include <string.h>
#include <stdio.h>

#include "lora_app.h"
#include "lora_app_main.h"
#include "hardware/lora.h"

#include "gui/mainbar/mainbar.h"
#include "gui/statusbar.h"
#include "gui/widget_styles.h"
#include "gui/widget_factory.h"

#ifdef NATIVE_64BIT
    #include "utils/logging.h"
#else
    #include <Arduino.h>
#endif

static lv_obj_t *lora_app_main_tile = NULL;
static lv_obj_t *lora_status_label = NULL;
static lv_obj_t *lora_log = NULL;
static lv_task_t *_lora_app_task = NULL;

static volatile bool lora_rx_pending = false;
static char lora_rx_line[ 80 ] = "";
static uint32_t lora_tx_counter = 0;

static void exit_lora_app_main_event_cb( lv_obj_t * obj, lv_event_t event );
static void lora_send_event_cb( lv_obj_t * obj, lv_event_t event );
static bool lora_app_receive_cb( EventBits_t event, void *arg );
static void lora_app_task( lv_task_t * task );
static void lora_app_activate_cb( void );
static void lora_app_hibernate_cb( void );

void lora_app_main_setup( uint32_t tile_num ) {
    lora_app_main_tile = mainbar_get_tile_obj( tile_num );

    /*
     * title / status
     */
    lora_status_label = lv_label_create( lora_app_main_tile, NULL );
    lv_label_set_text( lora_status_label, "LoRa: init..." );
    lv_obj_align( lora_status_label, NULL, LV_ALIGN_IN_TOP_MID, 0, 8 );

    /*
     * received-packet log
     */
    lora_log = lv_textarea_create( lora_app_main_tile, NULL );
    lv_textarea_set_text( lora_log, "" );
    lv_textarea_set_cursor_hidden( lora_log, true );
    lv_obj_set_size( lora_log, lv_disp_get_hor_res( NULL ) - 20, lv_disp_get_ver_res( NULL ) - 110 );
    lv_obj_align( lora_log, NULL, LV_ALIGN_IN_TOP_MID, 0, 36 );

    /*
     * send-ping button
     */
    lv_obj_t *send_btn = lv_btn_create( lora_app_main_tile, NULL );
    lv_obj_set_size( send_btn, 100, 40 );
    lv_obj_align( send_btn, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -THEME_ICON_PADDING );
    lv_obj_set_event_cb( send_btn, lora_send_event_cb );
    lv_obj_t *send_label = lv_label_create( send_btn, NULL );
    lv_label_set_text( send_label, "Send Ping" );

    /*
     * exit button
     */
    lv_obj_t *exit_btn = wf_add_exit_button( lora_app_main_tile, exit_lora_app_main_event_cb );
    lv_obj_align( exit_btn, lora_app_main_tile, LV_ALIGN_IN_BOTTOM_LEFT, THEME_ICON_PADDING, -THEME_ICON_PADDING );

    /*
     * receive packets via the LoRa callback table
     */
    lora_register_cb( LORACTL_RECEIVE, lora_app_receive_cb, "lora app" );

    mainbar_add_tile_activate_cb( tile_num, lora_app_activate_cb );
    mainbar_add_tile_hibernate_cb( tile_num, lora_app_hibernate_cb );
}

static void exit_lora_app_main_event_cb( lv_obj_t * obj, lv_event_t event ) {
    switch( event ) {
        case( LV_EVENT_CLICKED ):   mainbar_jump_back();
                                    break;
    }
}

static void lora_send_event_cb( lv_obj_t * obj, lv_event_t event ) {
    switch( event ) {
        case( LV_EVENT_CLICKED ): {
            char payload[ 32 ];
            snprintf( payload, sizeof( payload ), "ping %u", (unsigned)( ++lora_tx_counter ) );
            bool ok = lora_send( (const uint8_t *)payload, strlen( payload ) );
            if ( lora_log ) {
                lv_textarea_add_text( lora_log, ok ? "TX " : "TX FAIL " );
                lv_textarea_add_text( lora_log, payload );
                lv_textarea_add_text( lora_log, "\n" );
            }
            break;
        }
    }
}

/*
 * runs in the powermgm task context - keep it minimal, just stage a line
 */
static bool lora_app_receive_cb( EventBits_t event, void *arg ) {
    lora_data_t *data = (lora_data_t *)arg;
    if ( data == NULL )
        return( true );

    int n = data->len < 32 ? data->len : 32;
    snprintf( lora_rx_line, sizeof( lora_rx_line ), "RX %dB rssi %.0f snr %.0f: %.*s\n",
              (int)data->len, data->rssi, data->snr, n, (const char *)data->data );
    lora_rx_pending = true;
    return( true );
}

static void lora_app_task( lv_task_t * task ) {
    if ( lora_rx_pending ) {
        lora_rx_pending = false;
        if ( lora_log )
            lv_textarea_add_text( lora_log, lora_rx_line );
    }
}

static void lora_app_activate_cb( void ) {
    if ( lora_status_label ) {
        if ( lora_get_available() )
            lv_label_set_text( lora_status_label, "LoRa: ready (listening)" );
        else
            lv_label_set_text( lora_status_label, "LoRa: no radio" );
    }
    if ( _lora_app_task == NULL )
        _lora_app_task = lv_task_create( lora_app_task, 200, LV_TASK_PRIO_LOW, NULL );
}

static void lora_app_hibernate_cb( void ) {
    if ( _lora_app_task != NULL ) {
        lv_task_del( _lora_app_task );
        _lora_app_task = NULL;
    }
}
