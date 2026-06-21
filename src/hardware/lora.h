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
#ifndef _LORA_H
    #define _LORA_H

    #include "callback.h"

    /**
     * @brief lora event mask (delivered through the callback table)
     */
    #define LORACTL_RECEIVE     _BV(0)      /** @brief packet received, arg points to lora_data_t */
    #define LORACTL_TX_DONE     _BV(1)      /** @brief transmit finished */

    /**
     * @brief received packet payload passed with LORACTL_RECEIVE
     */
    #define LORA_MAX_PAYLOAD    255
    typedef struct {
        uint8_t data[ LORA_MAX_PAYLOAD ];   /** @brief raw payload */
        size_t len;                         /** @brief payload length */
        float rssi;                         /** @brief received signal strength (dBm) */
        float snr;                          /** @brief signal-to-noise ratio (dB) */
    } lora_data_t;

    /**
     * @brief setup the LoRa radio (no-op on boards without an SX1262)
     */
    void lora_setup( void );
    /**
     * @brief true when an SX1262 was found and initialised
     */
    bool lora_get_available( void );
    /**
     * @brief transmit a raw payload (blocking). returns true on success.
     */
    bool lora_send( const uint8_t *data, size_t len );
    /**
     * @brief register a callback for LoRa events (LORACTL_*)
     */
    bool lora_register_cb( EventBits_t event, CALLBACK_FUNC callback_func, const char *id );

#endif // _LORA_H
