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
#ifndef _LORA_APP_H
    #define _LORA_APP_H

    #include "gui/icon.h"

    void lora_app_setup( void );
    uint32_t lora_app_get_app_main_tile_num( void );

#endif // _LORA_APP_H
