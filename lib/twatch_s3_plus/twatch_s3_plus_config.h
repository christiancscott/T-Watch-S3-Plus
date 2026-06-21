/****************************************************************************
 *   LilyGo T-Watch S3 Plus ( ESP32-S3 ) hardware pin / address map
 *
 *   Pin assignments taken from the official LilyGo documentation for the
 *   T-Watch S3 Plus. Mirrors the layout of lib/twatch2021/twatch2021_config.h
 *   so the hardware modules can pull a single board header.
 *
 *   Confirmed for this unit: SX1262 LoRa, no microSD slot, 940 mAh battery.
 ****************************************************************************/
#ifndef _TWATCH_S3_PLUS_CONFIG_H
    #define _TWATCH_S3_PLUS_CONFIG_H

    /**
     * Main I2C bus ( Wire ) : AXP2101 PMU, PCF8563 RTC, BMA423 IMU, DRV2605 haptic
     */
    #define IICSDA              10
    #define IICSCL              11

    /**
     * Touch I2C bus ( Wire1 ) : FocalTech FT6336U ( separate bus from the main one )
     */
    #define TOUCH_IICSDA        39
    #define TOUCH_IICSCL        40
    #define TOUCH_INT           16
    #define TOUCH_RST           -1      /* not broken out / shared - verify */

    /**
     * Peripheral interrupt pins
     */
    #define AXP2101_INT         21      /* PMU IRQ ( PWRKEY / charge / vbus )   */
    #define RTC_INT             17      /* PCF8563 alarm / timer IRQ            */
    #define BMA_INT_1           14      /* BMA423 INT1                          */
    #define BMA_INT_2           -1      /* BMA423 INT2 not broken out           */

    /**
     * Display ( ST7789V3, SPI ) - mirrored into TFT_eSPI build flags
     */
    #define DISP_CS             12
    #define DISP_MOSI           13
    #define DISP_SCK            18
    #define DISP_DC             38
    #define DISP_RST            -1      /* verify against LilyGoLib pin header  */
    #define DISP_BL             45      /* backlight, ledc PWM                  */

    /**
     * Audio ( MAX98357A I2S class-D amplifier )
     */
    #define I2S_BCLK            48
    #define I2S_WCLK            15      /* a.k.a. LRCLK                         */
    #define I2S_DOUT            46

    /**
     * Microphone ( PDM )
     */
    #define MIC_SCK             44
    #define MIC_DATA            47

    /**
     * GPS ( u-blox MIA-M10Q, UART )
     */
    #define GPS_TX              42      /* MCU TX -> GPS RX                     */
    #define GPS_RX              41      /* MCU RX <- GPS TX                     */

    /**
     * LoRa ( Semtech SX1262, SPI - confirmed radio for this unit )
     */
    #define LORA_SCK            3
    #define LORA_MISO           4
    #define LORA_MOSI           1
    #define LORA_CS             5
    #define LORA_IRQ            9       /* DIO1                                 */
    #define LORA_RST            8
    #define LORA_BUSY           7

    /**
     * Infrared emitter
     */
    #define IR_PIN              2

    /**
     * I2C device addresses
     */
    /* AXP2101_SLAVE_ADDRESS (0x34) is provided by XPowersLib, do not redefine here */
    #define PCF8563_SLAVE_ADDRESS   0x51
    #define BMA423_SLAVE_ADDRESS    0x18
    #define FT6336_SLAVE_ADDRESS    0x38
    #define DRV2605_SLAVE_ADDRESS   0x5A

    /**
     * Battery ( confirmed 940 mAh cell )
     */
    #define DESIGNED_BATTERY_CAP    940

#endif // _TWATCH_S3_PLUS_CONFIG_H
