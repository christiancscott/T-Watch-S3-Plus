/****************************************************************************
 *   LovyanGFX panel definition for the LilyGo T-Watch S3 Plus ( ESP32-S3 ).
 *
 *   The stock TFT_eSPI driver faults on the S3 ( null SPI bus in
 *   begin_tft_write ) and its HSPI workaround never binds the bus cleanly,
 *   so the S3 display is driven with LovyanGFX instead. Pins come from the
 *   board header so there is a single source of truth.
 ****************************************************************************/
#ifndef _LGFX_TWATCH_S3_PLUS_H
    #define _LGFX_TWATCH_S3_PLUS_H

    #define LGFX_USE_V1
    #include <LovyanGFX.hpp>
    #include "twatch_s3_plus_config.h"

    class LGFX_TWatchS3Plus : public lgfx::LGFX_Device {
            lgfx::Panel_ST7789  _panel;
            lgfx::Bus_SPI       _bus;

        public:
            LGFX_TWatchS3Plus( void ) {
                {   /* SPI bus ( ST7789 is write only, MISO unused ) */
                    auto cfg = _bus.config();
                    cfg.spi_host    = SPI2_HOST;
                    cfg.spi_mode    = 0;
                    cfg.freq_write  = 40000000;
                    cfg.freq_read   = 16000000;
                    cfg.spi_3wire   = true;
                    cfg.use_lock    = true;
                    cfg.dma_channel = SPI_DMA_CH_AUTO;
                    cfg.pin_sclk    = DISP_SCK;     /* 18 */
                    cfg.pin_mosi    = DISP_MOSI;    /* 13 */
                    cfg.pin_miso    = -1;
                    cfg.pin_dc      = DISP_DC;      /* 38 */
                    _bus.config( cfg );
                    _panel.setBus( &_bus );
                }
                {   /* ST7789 240x240 panel */
                    auto cfg = _panel.config();
                    cfg.pin_cs          = DISP_CS;  /* 12 */
                    cfg.pin_rst         = DISP_RST; /* -1, soft reset */
                    cfg.pin_busy        = -1;
                    cfg.panel_width     = 240;
                    cfg.panel_height    = 240;
                    cfg.offset_x        = 0;
                    cfg.offset_y        = 0;
                    cfg.offset_rotation = 0;
                    cfg.readable        = false;
                    cfg.invert          = true;     /* ST7789 needs inversion on */
                    cfg.rgb_order       = false;
                    cfg.dlen_16bit      = false;
                    cfg.bus_shared      = true;
                    _panel.config( cfg );
                }
                setPanel( &_panel );
            }
    };

#endif // _LGFX_TWATCH_S3_PLUS_H
