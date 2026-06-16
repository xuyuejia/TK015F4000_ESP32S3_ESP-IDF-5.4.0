/**
 * @file    lcd_st7796.h
 * @brief   ST7796S LCD 驱动 — ESP-IDF GDMA 版
 *
 *          SPI2_HOST (FSPI) 独立 GDMA, 零冲突
 *          spi_device_transmit → DMA (帧缓冲)
 *          spi_device_polling_transmit → 小传输 (命令/数据)
 *
 * 引脚: CS=16 SCK=6 MOSI=15 DC=7 RST=4 BL=5
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- 引脚 ----
#define PIN_CS   16
#define PIN_DC   7
#define PIN_RST  4
#define PIN_BL   5
#define PIN_MOSI 15
#define PIN_SCLK 6

// ---- 颜色 (RGB565, 预字节翻转) ----
// ST7796 BGR 模式 + SPI3 DMA little-endian → 需 byte-swap
#define SWAP16(c) ((uint16_t)(((uint16_t)(c)<<8)|((uint16_t)(c)>>8)))
#define COLOR_WHITE   0xFFFF
#define COLOR_BLACK   0x0000
#define COLOR_BLUE    SWAP16(0x001F)
#define COLOR_RED     SWAP16(0xF800)
#define COLOR_GREEN   SWAP16(0x07E0)
#define COLOR_YELLOW  SWAP16(0xFFE0)
#define COLOR_CYAN    SWAP16(0x7FFF)
#define COLOR_MAGENTA SWAP16(0xF81F)
#define COLOR_GRAY    SWAP16(0x8430)

// ---- API ----
void lcd_init(void);
void lcd_set_backlight(uint8_t level);

void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void lcd_fill_screen(uint16_t color);

void lcd_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void lcd_write_pixels(const uint8_t *data, size_t len);
void lcd_write_pixels_dma(const uint8_t *data, size_t len);

void lcd_draw_battery(uint16_t x, uint16_t y, uint8_t level);
void lcd_show_char(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg, bool bg_flag);
void lcd_show_gb1616(uint16_t x, uint16_t y, const uint8_t code[2], uint16_t fg, uint16_t bg, bool bg_flag);
void lcd_put_string(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, bool bg_flag);
void lcd_draw_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *data);
void lcd_draw_image_shifted(uint16_t x_old, uint16_t x_new, uint16_t y,
                            uint16_t w, uint16_t h, const uint8_t *data, uint16_t bg);

void lcd_render_char_buf(uint16_t *buf, int bw, int x, int y, char ch, uint16_t fg, uint16_t bg);
void lcd_render_string_buf(uint16_t *buf, int bw, int x, int y, const char *s, uint16_t fg, uint16_t bg);
void lcd_render_gb1616_buf(uint16_t *buf, int bw, int x, int y, const uint8_t code[2], uint16_t fg, uint16_t bg);
void lcd_render_mixed_buf(uint16_t *buf, int bw, int x, int y, const char *s, uint16_t fg, uint16_t bg);
int64_t lcd_get_last_cpu_us(void);

#ifdef __cplusplus
}
#endif
