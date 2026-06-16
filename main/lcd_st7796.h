/**
 * @file    lcd_st7796.h
 * @brief   ST7796S LCD 驱动 — ESP-IDF v5.4 GDMA 版
 *
 *  架构:
 *    SPI2_HOST (FSPI) @ 40MHz Mode3, CS/DC 手动控制
 *    spi_device_transmit        → GDMA 批量像素 (32768 B/chunk)
 *    spi_device_polling_transmit → 命令/数据小传输 (1~2 字节)
 *    Cache_WriteBack_Addr       → DMA 前刷 CPU 缓存，确保一致性
 *
 *  背光:
 *    LEDC PWM @ 5kHz, 8-bit 精度
 *    初始化时锁死在 0 (OFF), 首帧绘制完毕后由应用层点亮
 *    避免启动花屏和 GPIO 悬空导致的微亮
 *
 *  颜色:
 *    ST7796 BGR 模式 (MADCTL=0x48) + ESP32 SPI little-endian 字节序
 *    → SWAP16 宏预交换 RGB565 高低字节, 确保颜色正确
 *
 *  引脚: CS=16 SCK=6 MOSI=15 DC=7 RST=4 BL=5
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

// ========== 引脚定义 ==========
#define PIN_CS   16   // SPI 片选 (LOW 有效, 手动控制)
#define PIN_DC   7    // 数据/命令选择 (HIGH=数据)
#define PIN_RST  4    // 硬件复位 (LOW 复位, ≥1ms 脉宽)
#define PIN_BL   5    // 背光 (LEDC PWM, 初始化时 OFF, 首帧后 ON)
#define PIN_MOSI 15   // SPI 主机数据输出
#define PIN_SCLK 6    // SPI 时钟 (40MHz, Mode 3)

// ========== RGB565 颜色 (预字节翻转) ==========
// ST7796 BGR 模式 (MADCTL bit3=1) + ESP32 SPI little-endian
// → 需 SWAP16 翻转高低字节, 否则 R↔B 互换
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

// ========== 初始化与硬件控制 ==========
void lcd_init(void);                         // 冷启动安全: 400ms 等电源 + 软件复位 + 完整寄存器序列
void lcd_set_backlight(uint8_t level);       // 0=OFF, 255=最亮 (LEDC PWM)

// ========== 绘制 API ==========
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void lcd_fill_screen(uint16_t color);
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

// ========== 底层 SPI 操作 ==========
void lcd_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);   // CASET+RASET+RAMWR
void lcd_write_pixels_dma(const uint8_t *data, size_t len);                // GDMA 批量传输 (自动分块)
void lcd_write_pixels(const uint8_t *data, size_t len);                    // 同 DMA

// ========== 高级绘制 ==========
void lcd_draw_battery(uint16_t x, uint16_t y, uint8_t level);              // 电池图标 (0~4 格)
void lcd_draw_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *data);
void lcd_draw_image_shifted(uint16_t x_old, uint16_t x_new, uint16_t y,   // 原子滑动 (零闪烁)
                            uint16_t w, uint16_t h, const uint8_t *data, uint16_t bg);
int64_t lcd_get_last_cpu_us(void);                                         // 获取上次 CPU 耗时

// ========== 文字渲染 (直接写屏) ==========
void lcd_show_char(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg, bool bg_flag);
void lcd_show_gb1616(uint16_t x, uint16_t y, const uint8_t code[2], uint16_t fg, uint16_t bg, bool bg_flag);
void lcd_put_string(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, bool bg_flag);

// ========== 文字渲染 (缓冲模式, 用于性能覆层) ==========
void lcd_render_char_buf(uint16_t *buf, int bw, int x, int y, char ch, uint16_t fg, uint16_t bg);
void lcd_render_string_buf(uint16_t *buf, int bw, int x, int y, const char *s, uint16_t fg, uint16_t bg);
void lcd_render_gb1616_buf(uint16_t *buf, int bw, int x, int y, const uint8_t code[2], uint16_t fg, uint16_t bg);
void lcd_render_mixed_buf(uint16_t *buf, int bw, int x, int y, const char *s, uint16_t fg, uint16_t bg);

#ifdef __cplusplus
}
#endif
