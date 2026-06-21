/****************************************************************************
 *   LoRa (Semtech SX1262) hardware abstraction for the T-Watch S3 Plus
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
#include "lora.h"
#include "powermgm.h"
#include "callback.h"

#ifdef NATIVE_64BIT
    #include "utils/logging.h"
#else
    #include <Arduino.h>
    #if defined( LILYGO_WATCH_S3_PLUS )
        #include <twatch_s3_plus_config.h>
        #include <RadioLib.h>
        #include <SPI.h>

        /**
         * Default LoRa frequency for this unit: US915 (902-928 MHz ISM).
         * Override per region: EU868 = 868.0, AS923 = 923.0, 433 MHz = 433.0.
         */
        #ifndef LORA_FREQUENCY
            #define LORA_FREQUENCY  915.0
        #endif

        static SPIClass loraSPI( HSPI );
        static SX1262 radio = new Module( LORA_CS, LORA_IRQ, LORA_RST, LORA_BUSY, loraSPI );

        volatile bool DRAM_ATTR lora_irq_flag = false;
        static void IRAM_ATTR lora_irq( void ) {
            lora_irq_flag = true;
        }
    #endif
#endif

static bool lora_available = false;
callback_t *lora_callback = NULL;

static bool lora_powermgm_loop_cb( EventBits_t event, void *arg );

void lora_setup( void ) {
#ifdef NATIVE_64BIT
    return;
#else
    #if defined( LILYGO_WATCH_S3_PLUS )
        /**
         * the SX1262 sits on its own SPI bus (SCK/MISO/MOSI = 3/4/1)
         */
        loraSPI.begin( LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS );

        log_i("init SX1262 LoRa radio @ %.1f MHz", LORA_FREQUENCY );
        int state = radio.begin( LORA_FREQUENCY );
        if ( state != RADIOLIB_ERR_NONE ) {
            log_e("SX1262 init failed, code %d", state );
            lora_available = false;
            return;
        }
        /**
         * LilyGo SX1262 modules route the antenna switch through DIO2
         */
        radio.setDio2AsRfSwitch( true );
        radio.setOutputPower( 22 );             /* SX1262 max +22 dBm */
        radio.setDio1Action( lora_irq );
        /**
         * start listening
         */
        state = radio.startReceive();
        if ( state != RADIOLIB_ERR_NONE ) {
            log_e("SX1262 startReceive failed, code %d", state );
            lora_available = false;
            return;
        }
        lora_available = true;
        log_i("SX1262 LoRa radio ready");

        powermgm_register_loop_cb( POWERMGM_WAKEUP | POWERMGM_SILENCE_WAKEUP | POWERMGM_STANDBY, lora_powermgm_loop_cb, "lora loop" );
    #else
        return;
    #endif
#endif
}

bool lora_get_available( void ) {
    return( lora_available );
}

bool lora_send( const uint8_t *data, size_t len ) {
#ifdef NATIVE_64BIT
    return( false );
#else
    #if defined( LILYGO_WATCH_S3_PLUS )
        if ( !lora_available )
            return( false );

        int state = radio.transmit( (uint8_t *)data, len );
        /**
         * return to receive mode after the blocking transmit
         */
        radio.startReceive();

        if ( state == RADIOLIB_ERR_NONE ) {
            callback_send( lora_callback, LORACTL_TX_DONE, NULL );
            return( true );
        }
        log_e("LoRa transmit failed, code %d", state );
        return( false );
    #else
        return( false );
    #endif
#endif
}

static bool lora_powermgm_loop_cb( EventBits_t event, void *arg ) {
#ifndef NATIVE_64BIT
    #if defined( LILYGO_WATCH_S3_PLUS )
        if ( !lora_available )
            return( true );

        if ( lora_irq_flag ) {
            lora_irq_flag = false;

            static lora_data_t lora_data;
            int state = radio.readData( lora_data.data, LORA_MAX_PAYLOAD );
            if ( state == RADIOLIB_ERR_NONE ) {
                lora_data.len = radio.getPacketLength();
                lora_data.rssi = radio.getRSSI();
                lora_data.snr = radio.getSNR();
                log_i("LoRa rx: %d bytes, RSSI %.1f dBm, SNR %.1f dB", lora_data.len, lora_data.rssi, lora_data.snr );
                callback_send( lora_callback, LORACTL_RECEIVE, (void *)&lora_data );
            }
            /**
             * resume listening
             */
            radio.startReceive();
        }
    #endif
#endif
    return( true );
}

bool lora_register_cb( EventBits_t event, CALLBACK_FUNC callback_func, const char *id ) {
    if ( lora_callback == NULL ) {
        lora_callback = callback_init( "lora" );
        if ( lora_callback == NULL ) {
            log_e("lora_callback alloc failed");
            return( false );
        }
    }
    return( callback_register( lora_callback, event, callback_func, id ) );
}
