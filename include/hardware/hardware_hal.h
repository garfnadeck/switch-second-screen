#ifndef HARDWARE_HAL_H
#define HARDWARE_HAL_H

#include <lvgl.h>

#ifdef ARDUINO
    #include <Arduino.h>

    // =========================================================================
    // ESP32-S3 (ES3C35P): tela ST77922 via esp_lcd, ver hardware/es3c35p_lvgl.h
    // =========================================================================
    #if defined(ARDUINO_ARCH_ESP32)
        #include "hardware/es3c35p_lvgl.h"
        inline void init_hardware_display_touch() { es3c35p_lvgl_init(); }

    // =========================================================================
    // RASPBERRY PI PICO W (RP2040 - SPI0 e I2C0). Legado: sai com o Pico na F3.
    // =========================================================================
    #elif defined(ARDUINO_ARCH_RP2040)
        #define LGFX_USE_V1
        #include <LovyanGFX.hpp>
    class LGFX_Display : public lgfx::LGFX_Device {
        lgfx::Panel_ST7796  _panel_instance;
        lgfx::Bus_SPI       _bus_instance;
        lgfx::Light_PWM     _light_instance;
        lgfx::Touch_FT5x06  _touch_instance;

    public:
        LGFX_Display(void) {
            auto cfg_bus = _bus_instance.config();
            cfg_bus.spi_host   = 0; // SPI0 do RP2040
            cfg_bus.spi_mode   = 0;
            cfg_bus.freq_write = 40000000;
            cfg_bus.freq_read  = 16000000;
            cfg_bus.pin_sclk   = 18; // GP18 (SPI0 SCK)
            cfg_bus.pin_mosi   = 19; // GP19 (SPI0 TX)
            cfg_bus.pin_miso   = 16; // GP16 (SPI0 RX)
            cfg_bus.pin_dc     = 20; // GP20 (Data/Command)
            _bus_instance.config(cfg_bus);
            _panel_instance.setBus(&_bus_instance);

            auto cfg_panel = _panel_instance.config();
            cfg_panel.pin_cs   = 17; // GP17 (SPI0 CSn)
            cfg_panel.pin_rst  = 21; // GP21 (Reset)
            cfg_panel.pin_busy = -1;
            cfg_panel.memory_width  = 320;
            cfg_panel.memory_height = 480;
            cfg_panel.panel_width   = 320;
            cfg_panel.panel_height  = 480;
            cfg_panel.offset_x      = 0;
            cfg_panel.offset_y      = 0;
            cfg_panel.readable      = true;
            cfg_panel.invert        = false;
            cfg_panel.rgb_order     = false;
            _panel_instance.config(cfg_panel);

            auto cfg_light = _light_instance.config();
            cfg_light.pin_bl = 22; // GP22 (Backlight PWM)
            cfg_light.invert = false;
            cfg_light.freq   = 44100;
            _light_instance.config(cfg_light);
            _panel_instance.setLight(&_light_instance);

            auto cfg_touch = _touch_instance.config();
            cfg_touch.x_min      = 0;
            cfg_touch.x_max      = 480;
            cfg_touch.y_min      = 0;
            cfg_touch.y_max      = 320;
            cfg_touch.pin_int    = 26; // GP26 (Touch INT)
            cfg_touch.pin_sda    = 4;  // GP4 (I2C0 SDA)
            cfg_touch.pin_scl    = 5;  // GP5 (I2C0 SCL)
            cfg_touch.freq       = 400000;
            cfg_touch.i2c_port   = 0;
            cfg_touch.i2c_addr   = 0x38;
            _touch_instance.config(cfg_touch);
            _panel_instance.setTouch(&_touch_instance);

            setPanel(&_panel_instance);
        }
    };
    static LGFX_Display lcd;

    // Alocação inteligente de Buffer:
    // No RP2040 (264KB SRAM sem PSRAM), usamos um buffer estático compacto de 15 linhas (~14.4 KB).
    // No ESP32-S3, usamos 25 linhas (~24 KB) em RAM interna rápida (DMA-capable).
    #if defined(ARDUINO_ARCH_RP2040)
        #define LV_BUF_LINES 15
    #else
        #define LV_BUF_LINES 25
    #endif

    static lv_color_t lv_disp_buf1[480 * LV_BUF_LINES];

    static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
        uint32_t w = (area->x2 - area->x1 + 1);
        uint32_t h = (area->y2 - area->y1 + 1);
        lcd.startWrite();
        lcd.setAddrWindow(area->x1, area->y1, w, h);
        lcd.writePixels((lgfx::rgb565_t *)&color_p->full, w * h, true);
        lcd.endWrite();
        lv_disp_flush_ready(disp);
    }

    static void my_touch_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
        (void)indev_driver;
        uint16_t touchX, touchY;
        bool touched = lcd.getTouch(&touchX, &touchY);
        if (touched) {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = touchX;
            data->point.y = touchY;
        } else {
            data->state = LV_INDEV_STATE_REL;
        }
    }

    inline void init_hardware_display_touch() {
        lcd.init();
        lcd.setRotation(1); // Paisagem (480x320)
        lcd.setBrightness(200);

        lv_init();

        static lv_disp_draw_buf_t draw_buf;
        lv_disp_draw_buf_init(&draw_buf, lv_disp_buf1, NULL, 480 * LV_BUF_LINES);

        static lv_disp_drv_t disp_drv;
        lv_disp_drv_init(&disp_drv);
        disp_drv.hor_res = 480;
        disp_drv.ver_res = 320;
        disp_drv.flush_cb = my_disp_flush;
        disp_drv.draw_buf = &draw_buf;
        lv_disp_drv_register(&disp_drv);

        static lv_indev_drv_t indev_drv;
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = my_touch_read;
        lv_indev_drv_register(&indev_drv);
    }


    #endif
#else
    inline void init_hardware_display_touch() {}
#endif

#endif // HARDWARE_HAL_H
