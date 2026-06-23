/****************************************************************************
 *   Copyright  2024
 *
 *   Settings launcher app - see settings_app.h for the rationale.
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "config.h"

#include "settings_app.h"

#include "gui/mainbar/mainbar.h"
#include "gui/mainbar/setup_tile/setup_tile.h"
#include "gui/app.h"

#ifdef NATIVE_64BIT
    #include "utils/logging.h"
#else
    #include <Arduino.h>
#endif
/*
 * settings launcher app icon
 */
icon_t *settings_app = NULL;
/*
 * the gear icon already used by the quickbar
 */
LV_IMG_DECLARE(setup_64px);
/*
 * declare callback functions
 */
static void enter_settings_app_event_cb( lv_obj_t * obj, lv_event_t event );
/*
 * automatic register the app setup function
 */
static int registed = app_autocall_function( &settings_app_setup, 1 );           /** @brief app autocall function */
/*
 * setup routine for the settings launcher app
 */
void settings_app_setup( void ) {
    /*
     * check if app already registered for autocall
     */
    if( !registed ) {
        return;
    }
    /*
     * register app icon on the app tile - it just jumps to the setup tile,
     * so it does not need its own tile.
     */
    settings_app = app_register( "settings", &setup_64px, enter_settings_app_event_cb );
}
/**
 * @brief callback function to enter the settings ( setup ) tile
 *
 * @param obj           pointer to the object
 * @param event         the event
 */
static void enter_settings_app_event_cb( lv_obj_t * obj, lv_event_t event ) {
    switch( event ) {
        case( LV_EVENT_CLICKED ):       mainbar_jump_to_tilenumber( setup_get_tile_num(), LV_ANIM_OFF, true );
                                        break;
    }
}
