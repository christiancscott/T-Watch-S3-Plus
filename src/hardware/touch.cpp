/****************************************************************************
 *   Tu May 22 21:23:51 2020
 *   Copyright  2020  Dirk Brosswick
 *   Email: dirk.brosswick@googlemail.com
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

#include "motor.h"
#include "button.h"
#include "display.h"
#include "touch.h"
#include "powermgm.h"
#include "callback.h"
#include "gui/mainbar/mainbar.h"

#include "utils/alloc.h"

touch_config_t touch_config;

#ifdef NATIVE_64BIT
    #include "utils/logging.h"
    #include "indev/mouse.h"
    #include "indev/mousewheel.h"
#else
    #include <Arduino.h>
    #if defined( M5PAPER )
        #include <M5EPD.h>
    #elif defined( M5CORE2 )
        #include <M5Core2.h>
    #elif defined( LILYGO_WATCH_2020_V1 ) || defined( LILYGO_WATCH_2020_V2 ) || defined( LILYGO_WATCH_2020_V3 )
        #include <TTGO.h>
    #elif defined( LILYGO_WATCH_2021 )    
        #include <Wire.h>
        #include <twatch2021_config.h>
        #include <CST8165/CST816S.h>

        CST816S_Class TouchSensor;
    #elif defined( WT32_SC01 )
        #include <Adafruit_FT6206.h>

        Adafruit_FT6206 ctp = Adafruit_FT6206();
    #elif defined( LILYGO_WATCH_S3_PLUS )
        #include <Wire.h>
        #include <twatch_s3_plus_config.h>

        /**
         * Minimal FocalTech FT6336U reader on the dedicated touch bus ( Wire1 ).
         * Avoids pulling in a second Bosch-driver library ( SensorLib ) that would
         * clash with BMA423_Library used by the accelerometer.
         */
        static bool ft6336_read_point( int16_t &x, int16_t &y ) {
            Wire1.beginTransmission( FT6336_SLAVE_ADDRESS );
            Wire1.write( 0x02 );                                     /* TD_STATUS register */
            if ( Wire1.endTransmission( false ) != 0 )
                return( false );
            if ( Wire1.requestFrom( (uint8_t)FT6336_SLAVE_ADDRESS, (uint8_t)5 ) != 5 )
                return( false );
            uint8_t td_status = Wire1.read();                       /* 0x02 number of points */
            uint8_t xh = Wire1.read();                              /* 0x03 */
            uint8_t xl = Wire1.read();                              /* 0x04 */
            uint8_t yh = Wire1.read();                              /* 0x05 */
            uint8_t yl = Wire1.read();                              /* 0x06 */
            if ( ( td_status & 0x0F ) == 0 )
                return( false );
            x = ( ( xh & 0x0F ) << 8 ) | xl;
            y = ( ( yh & 0x0F ) << 8 ) | yl;
            return( true );
        }
    #else
        #error "no hardware driver for touch, please setup minimal drivers ( display/framebuffer/touch )"
    #endif
    volatile bool DRAM_ATTR touch_irq_flag = false;
    portMUX_TYPE DRAM_ATTR Touch_IRQ_Mux = portMUX_INITIALIZER_UNLOCKED;
    #if defined( LILYGO_WATCH_S3_PLUS ) && !defined( NATIVE_64BIT )
        /* set by touch_read() when a swipe-up-from-the-bottom is recognised; acted
         * on from an lv_task ( safe context ) to jump back to the watchface */
        static volatile bool s3_swipe_home_request = false;
        static void touch_gesture_task( lv_task_t *task ) {
            if ( s3_swipe_home_request ) {
                s3_swipe_home_request = false;
                mainbar_jump_to_maintile( LV_ANIM_OFF );
            }
        }
    #endif
    void IRAM_ATTR touch_irq( void );

    void IRAM_ATTR touch_irq( void ) {
        /*
        * enter critical section and set interrupt flag
        */
        portENTER_CRITICAL_ISR(&Touch_IRQ_Mux);
        touch_irq_flag = true;
        /*
        * leave critical section
        */
        portEXIT_CRITICAL_ISR(&Touch_IRQ_Mux);
        powermgm_resume_from_ISR();
    }

    static SemaphoreHandle_t xSemaphores = NULL;

    bool touch_lock_take( void ) {
        return xSemaphoreTake( xSemaphores, portMAX_DELAY ) == pdTRUE;
    }
    void touch_lock_give( void ) {
        xSemaphoreGive( xSemaphores );
    }
#endif

callback_t *touch_callback = NULL;
lv_indev_t *touch_indev = NULL;
bool touched = false;

static bool touch_read(lv_indev_drv_t * drv, lv_indev_data_t*data);
bool touch_powermgm_loop_event_cb( EventBits_t event, void *arg );
bool touch_powermgm_event_cb( EventBits_t event, void *arg );
bool touch_send_event_cb( EventBits_t event, void *arg );

void touch_setup( void ) {
    /**
     * load touch config
     */
    touch_config.load();
#ifdef NATIVE_64BIT
    /**
     * init SDL mouse
     */
    mouse_init();
#else
    #if defined( M5PAPER )
        /**
         * rotate toucscreen
         */
        M5.TP.SetRotation(90);
    #elif defined( M5CORE2 )
        M5.Touch.begin();
        pinMode( GPIO_NUM_39, INPUT );
        attachInterrupt( GPIO_NUM_39, &touch_irq, FALLING );
    #elif defined( LILYGO_WATCH_2020_V1 ) || defined( LILYGO_WATCH_2020_V2 ) || defined( LILYGO_WATCH_2020_V3 )
        TTGOClass *ttgo = TTGOClass::getWatch();
        /*
        * reset/wakeup touch controller
        */
        #if defined( LILYGO_WATCH_2020_V2 ) || defined( LILYGO_WATCH_2020_V3 )
            ttgo->touchWakup();
        #endif
        /*
        * This changes to polling mode.
        * The touch sensor holds the line low for the duration of the touch.
        * The level change can still trigger an interrupt, which we
        * use to start polling, and we don't stop polling till the level is high again.
        */
        ttgo->touch->enableAutoCalibration();
        ttgo->touch->disableINT();
        detachInterrupt( TOUCH_INT );
        /*
        * This doesn't appear to change anything,
        * the sensor doesn't automatically switch to monitor mode till after 30 seconds
        */
        ttgo->touch->setMonitorTime(0x01);
        /*
        * This is supposed to control how often the sensor checks for touch while in monitor mode
        * The units is ms between checks, so 250 checks only 4 times a second. 
        */
        ttgo->touch->setMonitorPeriod(125);
        /*
        * create semaphore and register interrupt function
        */
        attachInterrupt( TOUCH_INT, &touch_irq, GPIO_INTR_NEGEDGE );
    #elif defined( LILYGO_WATCH_2021 )    
        ASSERT( TouchSensor.begin( Wire, Touch_Res, Touch_Int, CTP_SLAVER_ADDR ), "touch controler failed" );
    #elif defined( WT32_SC01 )
        pinMode( GPIO_NUM_39, INPUT );
        ASSERT( ctp.begin(40), "Couldn't start FT6206 touchscreen controller");
    #elif defined( LILYGO_WATCH_S3_PLUS )
        /*
        * FT6336U lives on its own I2C bus ( Wire1, GPIO39/40 ), already begun in hardware_setup().
        * Touch is polled; just configure the interrupt pin as input.
        */
        pinMode( TOUCH_INT, INPUT );
    #else
        #error "no touch init implemented, please setup minimal drivers ( display/framebuffer/touch )"
    #endif
    xSemaphores = xSemaphoreCreateMutex();
#endif
    /**
     * setup lvgl pointer driver
     */
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init( &indev_drv );
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register( &indev_drv );
    #if defined( LILYGO_WATCH_S3_PLUS ) && !defined( NATIVE_64BIT )
        /* poll the swipe-up-from-bottom gesture flag in a safe ( non-indev ) context */
        lv_task_create( touch_gesture_task, 80, LV_TASK_PRIO_LOW, NULL );
    #endif
    /*
     * register powermgm callback function
     */
    powermgm_register_cb( POWERMGM_SILENCE_WAKEUP | POWERMGM_STANDBY | POWERMGM_WAKEUP | POWERMGM_ENABLE_INTERRUPTS | POWERMGM_DISABLE_INTERRUPTS , touch_powermgm_event_cb, "touch" );
    powermgm_register_loop_cb( POWERMGM_SILENCE_WAKEUP | POWERMGM_STANDBY | POWERMGM_WAKEUP , touch_powermgm_loop_event_cb, "touch powermgm loop" );
}

bool touch_register_cb( EventBits_t event, CALLBACK_FUNC callback_func, const char *id ) {
  /*
    * check if an callback table exist, if not allocate a callback table
    */
  if ( touch_callback == NULL ) {
      touch_callback = callback_init( "touch" );
      if ( touch_callback == NULL ) {
          log_e("touch callback alloc failed");
          while(true);
      }
  }
  /*
    * register an callback entry and return them
    */
  return( callback_register( touch_callback, event, callback_func, id ) );
}

bool touch_send_event_cb( EventBits_t event, void *arg ) {
  /*
    * call all callbacks with her event mask
    */
  return( callback_send_no_log( touch_callback, event, arg ) );
}

bool touch_powermgm_loop_event_cb( EventBits_t event, void *arg ) {
    bool retval = false;
    
    #ifdef NATIVE_64BIT

    #else
        /*
        * handle IRQ event
        */
        portENTER_CRITICAL(&Touch_IRQ_Mux);
        bool temp_pmu_irq_flag = touch_irq_flag;
        touch_irq_flag = false;
        portEXIT_CRITICAL(&Touch_IRQ_Mux);

        #if defined( M5PAPER )
            retval = true;
        #elif defined ( M5CORE2 )
            switch( event ) {
                case POWERMGM_STANDBY:
                    if( M5.Touch.ispressed() )
                        powermgm_set_event( POWERMGM_WAKEUP_REQUEST );
                    retval = true;
                    break;
                case POWERMGM_WAKEUP:
                    retval = true;
                    break;
                case POWERMGM_SILENCE_WAKEUP:
                    if ( M5.Touch.ispressed() )
                        powermgm_set_event( POWERMGM_WAKEUP_REQUEST );
                    retval = true;        
                    break;
            }
        #elif defined( LILYGO_WATCH_2020_V1 ) || defined( LILYGO_WATCH_2020_V2 ) || defined( LILYGO_WATCH_2020_V3 )
            retval = true;
        #elif defined( LILYGO_WATCH_2021 ) || defined( LILYGO_WATCH_S3_PLUS )
            retval = true;
        #elif defined( WT32_SC01 )
            switch( event ) {
                case POWERMGM_STANDBY:
                    if( ctp.touched() )
                        powermgm_set_event( POWERMGM_WAKEUP_REQUEST );
                    retval = true;
                    break;
                case POWERMGM_WAKEUP:
                    retval = true;
                    break;
                case POWERMGM_SILENCE_WAKEUP:
                    if ( ctp.touched() )
                        powermgm_set_event( POWERMGM_WAKEUP_REQUEST );
                    retval = true;        
                    break;
            }    
        #else
            #error "no touch powermgm loop event implemented, please setup minimal drivers ( display/framebuffer/touch )"
        #endif
    #endif
    return( retval );
}

bool touch_powermgm_event_cb( EventBits_t event, void *arg ) {
    bool retval = false;

    #ifdef NATIVE_64BIT
        switch( event ) {
            case POWERMGM_STANDBY:              log_d("go standby");
                                                retval = true;
                                                break;
            case POWERMGM_WAKEUP:               log_d("go wakeup");
                                                retval = true;
                                                break;
            case POWERMGM_SILENCE_WAKEUP:       log_d("go silence wakeup");
                                                retval = true;
                                                break;
            case POWERMGM_ENABLE_INTERRUPTS:    retval = true;
                                                break;
            case POWERMGM_DISABLE_INTERRUPTS:   retval = true;
                                                break;
        }
    #else
        #if defined( M5PAPER ) || defined( LILYGO_WATCH_2021 ) || defined( LILYGO_WATCH_S3_PLUS )
            switch( event ) {
                case POWERMGM_STANDBY:          log_d("go standby");
                                                retval = true;
                                                break;
                case POWERMGM_WAKEUP:           log_d("go wakeup");
                                                retval = true;
                                                break;
                case POWERMGM_SILENCE_WAKEUP:   log_d("go silence wakeup");
                                                retval = true;
                                                break;
                case POWERMGM_ENABLE_INTERRUPTS:
                                                retval = true;
                                                break;
                case POWERMGM_DISABLE_INTERRUPTS:
                                                retval = true;
                                                break;
            }
        #elif defined( M5CORE2 ) || defined( WT32_SC01 )
            switch( event ) {
                case POWERMGM_STANDBY:          log_d("go standby");
                                                /**
                                                 * enable GPIO in lightsleep for wakeup
                                                 */
                                                gpio_wakeup_enable( (gpio_num_t)GPIO_NUM_39, GPIO_INTR_LOW_LEVEL );
                                                esp_sleep_enable_gpio_wakeup ();
                                                retval = true;
                                                break;
                case POWERMGM_WAKEUP:           log_d("go wakeup");
                                                retval = true;
                                                break;
                case POWERMGM_SILENCE_WAKEUP:   log_d("go silence wakeup");
                                                retval = true;
                                                break;
                case POWERMGM_ENABLE_INTERRUPTS:
                                                retval = true;
                                                break;
                case POWERMGM_DISABLE_INTERRUPTS:
                                                retval = true;
                                                break;
            }
        #elif defined( LILYGO_WATCH_2020_V1 ) || defined( LILYGO_WATCH_2020_V2 ) || defined( LILYGO_WATCH_2020_V3 )
            TTGOClass *ttgo = TTGOClass::getWatch();
            switch( event ) {
                case POWERMGM_STANDBY:          log_d("go standby");
                                                if ( touch_lock_take() ) {
                                                    ttgo->touchToMonitor();
                                                    touch_lock_give();
                                                }
                                                retval = true;
                                                break;
                case POWERMGM_WAKEUP:           log_d("go wakeup");

                                                if ( touch_lock_take() ) {
                                                    #if defined( LILYGO_WATCH_2020_V2 ) || defined( LILYGO_WATCH_2020_V3 )
                                                        ttgo->touchWakup();
                                                    #endif    
                                                    ttgo->touchToMonitor();
                                                    touch_lock_give();
                                                }
                                                retval = true;
                                                break;
                case POWERMGM_SILENCE_WAKEUP:   log_d("go silence wakeup");
                                                retval = true;
                                                break;
                case POWERMGM_ENABLE_INTERRUPTS:
                                                attachInterrupt( TOUCH_INT, &touch_irq, FALLING );
                                                retval = true;
                                                break;
                case POWERMGM_DISABLE_INTERRUPTS:
                                                detachInterrupt( TOUCH_INT );
                                                retval = true;
                                                break;
            }
        #else
            #error "no touch powermgm event implemented, please setup minimal drivers ( display/framebuffer/touch )"
        #endif
    #endif
    return( retval );
}

float touch_get_x_scale( void ) {
    return( touch_config.x_scale );
}

float touch_get_y_scale( void ) {
    return( touch_config.y_scale );
}

void touch_set_x_scale( float value ) {
    touch_config.x_scale = value;
    touch_config.save();
}

void touch_set_y_scale( float value ) {
    touch_config.y_scale = value;
    touch_config.save();
}

bool touch_getXY( int16_t &x, int16_t &y ) {
    
    /**
     * disable touch when we are in standby or silence wakeup
     */
    if ( powermgm_get_event( POWERMGM_STANDBY | POWERMGM_SILENCE_WAKEUP ) ) {
        return( false );
    }

    #ifdef NATIVE_64BIT
    #else
        #if defined( M5PAPER )
            if ( M5.TP.avaliable() ) {
                if( !M5.TP.isFingerUp() ) {
                    touched = true;
                    M5.TP.update();
                    tp_finger_t FingerItem = M5.TP.readFinger( 0 );
                    x = FingerItem.x;
                    y = FingerItem.y;
                }
                else {
                    touched = false;
                    return( false );
                }
            }
        #elif defined( M5CORE2 )
            M5.Touch.update();

            if ( M5.Touch.ispressed() ) {
                Point coordinate;
                coordinate = M5.Touch.getPressPoint();            
                x = coordinate.x;
                y = coordinate.y;
                touched = true;
            }
            else {
                touched = false;
                return( false );
            }
        #elif defined( LILYGO_WATCH_2020_V1 ) || defined( LILYGO_WATCH_2020_V2 ) || defined( LILYGO_WATCH_2020_V3 )
            TTGOClass *ttgo = TTGOClass::getWatch();
            static bool touch_press = false;

            // disable touch when we are in standby or silence wakeup
            if ( powermgm_get_event( POWERMGM_STANDBY | POWERMGM_SILENCE_WAKEUP ) ) {
                return( false );
            }
            /*
            * get touchstate from touchcontroller if not taken
            * by other task/thread
            */
            bool getTouchResult = false;
            if ( touch_lock_take() ) {
                getTouchResult = ttgo->getTouch( x, y );
                touch_lock_give();
            }
            /*
            * if touched?
            */
            if ( !getTouchResult ) {
                touch_press = false;
                return( false );
            }
            /*
            * wibe if touched
            */
            if ( !touch_press ) {
                touch_press = true;
                if ( display_get_vibe() )
                    motor_vibe( 3 );
            }
        #elif defined( LILYGO_WATCH_2021 )
            if ( TouchSensor.read() ) {
                x = TouchSensor.getX();
                y = TouchSensor.getY();
                return( true );
            }
            else {
                return( false );
            }
        #elif defined( WT32_SC01 )
            if ( ctp.touched() ) {
                TS_Point p = ctp.getPoint();
                x = TFT_WIDTH - map( p.y, 0, TFT_WIDTH, TFT_WIDTH, 0 );
                y = map( p.x, 0, TFT_HEIGHT, TFT_HEIGHT, 0 );
                return( true );
            }
            else {
                return( false );
            }
        #elif defined( LILYGO_WATCH_S3_PLUS )
            if ( ft6336_read_point( x, y ) ) {
                return( true );
            }
            else {
                return( false );
            }
        #else
            #error "no touch getXY function implemented, please setup minimal drivers ( display/framebuffer/touch )"
        #endif
    #endif
    return( true );
}

static bool touch_read(lv_indev_drv_t * drv, lv_indev_data_t*data) {
    bool retval = false;
    #if defined( LILYGO_WATCH_S3_PLUS ) && !defined( NATIVE_64BIT )
        /* last reported (processed) point, so a release reports a clean end point
         * for swipe/gesture detection instead of a stale/garbage sample */
        static lv_coord_t s3_last_x = 0;
        static lv_coord_t s3_last_y = 0;
    #endif

    #ifdef NATIVE_64BIT
        retval = mouse_read( drv, data );
    #else
        #if defined( M5PAPER )
            if ( M5.TP.avaliable() ) {
                if( !M5.TP.isFingerUp() ) {
                    M5.TP.update();
                    tp_finger_t FingerItem = M5.TP.readFinger( 0 );
                    data->point.x = FingerItem.x;
                    data->point.y = FingerItem.y;
                    data->state = LV_INDEV_STATE_PR;
                }
                else {
                    M5.TP.update();
                    data->state = LV_INDEV_STATE_REL;
                }
            }
        #elif defined( M5CORE2 )
            static int16_t last_x = 0;
            static int16_t last_y = 0;

            M5.Touch.update();

            if ( M5.Touch.ispressed() ) {
                Point coordinate;
                coordinate = M5.Touch.getPressPoint();
                if ( coordinate.x == -1 || coordinate.y == -1 ) {
                    data->state = LV_INDEV_STATE_REL;
                    data->point.x = last_x;
                    data->point.y = last_y;
                }
                else {
                    data->point.x = coordinate.x;
                    data->point.y = coordinate.y;
                    last_x = coordinate.x;
                    last_y = coordinate.y;
                    data->state = LV_INDEV_STATE_PR;
                }
            }
            else {
                data->state = LV_INDEV_STATE_REL;
            }

        #elif defined( LILYGO_WATCH_2020_V1 ) || defined( LILYGO_WATCH_2020_V2 ) || defined( LILYGO_WATCH_2020_V3 )
            /*
            * We use two flags, one changes in the interrupt handler
            * the other controls whether we poll the sensor,
            * and gets cleared when the level is no longer low,
            * meaning the touch has finished
            */
            portENTER_CRITICAL( &Touch_IRQ_Mux );
            bool temp_touch_irq_flag = touch_irq_flag;
            touch_irq_flag = false;
            portEXIT_CRITICAL( &Touch_IRQ_Mux );
            touched |= temp_touch_irq_flag;
            /*
             * get touch event
             */
            GesTrue_t gesture = FOCALTECH_NO_GESTRUE;
            if ( touch_lock_take() ) {
                TTGOClass::getWatch()->touchToMonitor();
                gesture = TTGOClass::getWatch()->touch->getGesture();
                touch_lock_give();
            }
            /*
             * check touch event
             */
            switch( gesture ) {
                case FOCALTECH_MOVE_UP:
                    break;
                case FOCALTECH_MOVE_LEFT:
                    break;
                case FOCALTECH_MOVE_DOWN:
                    break;
                case FOCALTECH_MOVE_RIGHT:
                    break;
                case FOCALTECH_ZOOM_IN:
                    break;
                case FOCALTECH_ZOOM_OUT:
                    break;
                default:
                    data->state = touch_getXY( data->point.x, data->point.y ) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
                    touched = digitalRead( TOUCH_INT ) == LOW;
                    if ( !touched ) {
                        /*
                        * Save power by switching to monitor mode now instead of waiting for 30 seconds.
                        */
                        if ( touch_lock_take() ) {
                            TTGOClass::getWatch()->touchToMonitor();
                            touch_lock_give();
                        }
                    }
                    break;
            }
        #elif defined( LILYGO_WATCH_2021 )
            data->state = touch_getXY( data->point.x, data->point.y ) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
        #elif defined( LILYGO_WATCH_S3_PLUS )
            /* raw start/last point for the swipe-up-from-bottom gesture */
            static lv_coord_t g_x0 = 0, g_y0 = 0, g_x1 = 0, g_y1 = 0;
            static bool g_down = false;
            if ( touch_getXY( data->point.x, data->point.y ) ) {
                if ( !g_down ) { g_x0 = data->point.x; g_y0 = data->point.y; g_down = true; }
                g_x1 = data->point.x; g_y1 = data->point.y;
                data->state = LV_INDEV_STATE_PR;
            }
            else {
                if ( g_down ) {
                    g_down = false;
                    /* swipe up starting from the bottom edge -> go home ( phone-style ) */
                    lv_coord_t dy = g_y0 - g_y1;                        /* up = positive */
                    lv_coord_t dx = ( g_x0 > g_x1 ) ? ( g_x0 - g_x1 ) : ( g_x1 - g_x0 );
                    if ( g_y0 >= RES_Y_MAX - 60 && dy >= 60 && dx < 80 )
                        s3_swipe_home_request = true;
                }
                /* report the last pressed point on release so LVGL sees a clean
                 * swipe vector ( prevents swipes being misread as taps ) */
                data->point.x = s3_last_x;
                data->point.y = s3_last_y;
                data->state = LV_INDEV_STATE_REL;
            }
        #elif defined( WT32_SC01 )
            data->state = touch_getXY( data->point.x, data->point.y ) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
        #else
            #error "no LVGL Input driver function implemented, please setup minimal drivers ( display/framebuffer/touch )"
        #endif
    #endif
    if( data->state == LV_INDEV_STATE_PR ) {
        /*
         * issue https://github.com/sharandac/My-TTGO-Watch/issues/18 fix
         */
        float temp_x = ( data->point.x - ( lv_disp_get_hor_res( NULL ) / 2 ) ) * touch_config.x_scale;
        float temp_y = ( data->point.y - ( lv_disp_get_ver_res( NULL ) / 2 ) ) * touch_config.y_scale;
        /**
         * store correct value into data stucture
         */
        data->point.x = temp_x + ( lv_disp_get_hor_res( NULL ) / 2 );
        data->point.y = temp_y + ( lv_disp_get_ver_res( NULL ) / 2 );
    }
    /**
     * check limits
     */
    if( data->point.x < 0 ) data->point.x = 0;
    if( data->point.x >= lv_disp_get_hor_res( NULL ) ) data->point.x = lv_disp_get_hor_res( NULL ) - 1;
    if( data->point.y < 0 ) data->point.y = 0;
    if( data->point.y >= lv_disp_get_ver_res( NULL ) ) data->point.y = lv_disp_get_ver_res( NULL ) - 1;
    #if defined( LILYGO_WATCH_S3_PLUS ) && !defined( NATIVE_64BIT )
        /* remember the processed point while pressed for the release report above */
        if ( data->state == LV_INDEV_STATE_PR ) {
            s3_last_x = data->point.x;
            s3_last_y = data->point.y;
        }
    #endif
    /**
     * send touch event
     */
    touch_t touch;
    touch.touched = data->state;
    touch.x_coor = data->point.x;
    touch.y_coor = data->point.y;
    /**
     * discard when callback return true
     */
    if ( touch_send_event_cb( TOUCH_UPDATE, (void*)&touch ) ) {
        data->state = LV_INDEV_STATE_REL;
    }

    return( retval );
}
