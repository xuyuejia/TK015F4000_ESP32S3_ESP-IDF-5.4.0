/**
 * @file    lcd_st7796.c
 * @brief   ST7796S LCD 驱动实现 — ESP-IDF GDMA
 */
#include "lcd_st7796.h"
#include "ascii_font.h"
#include "gb1616_font.h"
#include <string.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LCD";
static spi_device_handle_t spi = NULL;

// ---- DMA 传输 ----
void lcd_write_pixels_dma(const uint8_t *data, size_t len) {
    gpio_set_level(PIN_DC, 1); gpio_set_level(PIN_CS, 0);
    // ESP32-S3 SPI DMA max: 262144 bits = 32768 bytes per transaction
    const size_t MAX_CHUNK = 32768;
    size_t remain = len;
    const uint8_t *ptr = data;
    while (remain > 0) {
        size_t chunk = remain > MAX_CHUNK ? MAX_CHUNK : remain;
        spi_transaction_t t = { .tx_buffer = ptr, .length = chunk * 8 };
        esp_err_t ret = spi_device_transmit(spi, &t);
        if (ret != ESP_OK) ESP_LOGE(TAG, "DMA err 0x%x", ret);
        ptr    += chunk;
        remain -= chunk;
    }
    gpio_set_level(PIN_CS, 1);
}
void lcd_write_pixels(const uint8_t *data, size_t len) { lcd_write_pixels_dma(data, len); }

// ---- 小传输 (polling, 无 DMA) ----
static void write_cmd(uint8_t cmd) {
    gpio_set_level(PIN_DC, 0); gpio_set_level(PIN_CS, 0);
    spi_transaction_t t = { .tx_buffer = &cmd, .length = 8 };
    spi_device_polling_transmit(spi, &t);
    gpio_set_level(PIN_CS, 1);
}
static void write_data8(uint8_t dat) {
    gpio_set_level(PIN_DC, 1); gpio_set_level(PIN_CS, 0);
    spi_transaction_t t = { .tx_buffer = &dat, .length = 8 };
    spi_device_polling_transmit(spi, &t);
    gpio_set_level(PIN_CS, 1);
}
static void write_data16(uint16_t dat) {
    // LCD expects MSB-first; ESP32 little-endian → manually send high then low
    write_data8(dat >> 8);
    write_data8(dat & 0xFF);
}
void lcd_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    write_cmd(0x2A); write_data16(x1); write_data16(x2);
    write_cmd(0x2B); write_data16(y1); write_data16(y2);
    write_cmd(0x2C);
}

// ---- 初始化 ----
void lcd_init(void) {
    ESP_LOGI(TAG, "Init GPIO...");
    gpio_set_direction(PIN_CS,  GPIO_MODE_OUTPUT); gpio_set_level(PIN_CS, 1);
    gpio_set_direction(PIN_DC,  GPIO_MODE_OUTPUT); gpio_set_level(PIN_DC, 1);
    gpio_set_direction(PIN_RST, GPIO_MODE_OUTPUT); gpio_set_level(PIN_RST, 1);
    gpio_set_direction(PIN_BL,  GPIO_MODE_OUTPUT); gpio_set_level(PIN_BL, 0);

    ESP_LOGI(TAG, "HW reset...");
    gpio_set_level(PIN_RST, 1); vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_RST, 0); vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_RST, 1); vTaskDelay(pdMS_TO_TICKS(150));

    ESP_LOGI(TAG, "SPI2_HOST DMA init...");
    spi_bus_config_t bcfg = {
        .mosi_io_num = PIN_MOSI, .miso_io_num = -1, .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = 32768,  // ESP32-S3 SPI DMA max
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bcfg, SPI_DMA_CH_AUTO));
    spi_device_interface_config_t dcfg = {
        .mode = 3, .clock_speed_hz = 40 * 1000 * 1000,  // ST7796S max 62.5MHz, safe @40MHz
        .spics_io_num = -1, .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dcfg, &spi));

    ESP_LOGI(TAG, "ST7796 init sequence...");
    // Sleep out (datasheet: ≥120ms)
    write_cmd(0x11); vTaskDelay(pdMS_TO_TICKS(150));
    // MADCTL
    write_cmd(0x36); write_data8(0x48);
    // COLMOD
    write_cmd(0x3A); write_data8(0x77);
    // Panel settings
    write_cmd(0xF0); write_data8(0xC3);
    write_cmd(0xF0); write_data8(0x96);
    write_cmd(0xB4); write_data8(0x01);
    write_cmd(0xB7); write_data8(0xC6);
    write_cmd(0xB9); write_data8(0x02);
    write_cmd(0xC0); write_data8(0xF0); write_data8(0x65);
    write_cmd(0xC1); write_data8(0x15);
    write_cmd(0xC2); write_data8(0xAF);
    write_cmd(0xC5); write_data8(0x22);
    // Display timing
    write_cmd(0xE8); write_data8(0x40); write_data8(0x8A); write_data8(0x00);
    write_data8(0x00); write_data8(0x29); write_data8(0x19); write_data8(0xA5); write_data8(0x33);
    // Gamma
    write_cmd(0xE0);
    write_data8(0xD0); write_data8(0x04); write_data8(0x08); write_data8(0x09);
    write_data8(0x08); write_data8(0x15); write_data8(0x2F); write_data8(0x42);
    write_data8(0x46); write_data8(0x28); write_data8(0x15); write_data8(0x16);
    write_data8(0x29); write_data8(0x2D);
    write_cmd(0xE1);
    write_data8(0xD0); write_data8(0x04); write_data8(0x09); write_data8(0x09);
    write_data8(0x08); write_data8(0x15); write_data8(0x2E); write_data8(0x46);
    write_data8(0x46); write_data8(0x28); write_data8(0x15); write_data8(0x15);
    write_data8(0x29); write_data8(0x2D);
    write_cmd(0xF0); write_data8(0x3C);
    write_cmd(0xF0); write_data8(0x69);
    vTaskDelay(pdMS_TO_TICKS(5));
    // Display ON (datasheet: ≥120ms to stabilize)
    write_cmd(0x29); write_cmd(0x21);
    vTaskDelay(pdMS_TO_TICKS(150));
    // Final override: 16-bit RGB565
    write_cmd(0x3A); write_data8(0x55);
    write_cmd(0x36); write_data8(0x48);

    lcd_set_backlight(255);
    ESP_LOGI(TAG, "Init done.");
}

void lcd_set_backlight(uint8_t level) {
    static bool inited = false;
    if (!inited) {
        ledc_timer_config_t tc = { .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LEDC_TIMER_8_BIT, .timer_num = LEDC_TIMER_0, .freq_hz = 5000, .clk_cfg = LEDC_AUTO_CLK };
        ledc_channel_config_t cc = { .gpio_num = PIN_BL, .speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_0, .timer_sel = LEDC_TIMER_0, .duty = 0, .hpoint = 0 };
        ledc_timer_config(&tc); ledc_channel_config(&cc);
        inited = true;
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, level);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

// ---- 绘制 ----
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (!w || !h) return;
    lcd_set_window(x, y, x + w - 1, y + h - 1);
    uint32_t n = (uint32_t)w * h;
    static uint16_t buf[512]; static uint16_t last = 0xFFFF;
    if (color != last) { for (int i = 0; i < 512; i++) buf[i] = color; last = color; }
    gpio_set_level(PIN_DC, 1); gpio_set_level(PIN_CS, 0);
    while (n) { uint32_t c = n > 512 ? 512 : n; spi_device_transmit(spi, &(spi_transaction_t){ .tx_buffer = (const uint8_t *)buf, .length = c * 16 }); n -= c; }
    gpio_set_level(PIN_CS, 1);
}
void lcd_fill_screen(uint16_t color) { lcd_fill_rect(0, 0, 320, 320, color); }
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    lcd_set_window(x, y, x, y); gpio_set_level(PIN_DC, 1); gpio_set_level(PIN_CS, 0);
    spi_transaction_t t = { .tx_buffer = (const uint8_t *)&color, .length = 16 };
    spi_device_polling_transmit(spi, &t);
    gpio_set_level(PIN_CS, 1);
}

// ---- 电池 ----
void lcd_draw_battery(uint16_t x, uint16_t y, uint8_t lv) {
    lcd_fill_rect(x, y, 1, 6, COLOR_BLACK); lcd_fill_rect(x, y+14, 1, 6, COLOR_BLACK);
    lcd_fill_rect(x+28, y, 1, 6, COLOR_BLACK); lcd_fill_rect(x+28, y+14, 1, 6, COLOR_BLACK);
    lcd_fill_rect(x, y, 29, 1, COLOR_BLACK); lcd_fill_rect(x, y+20, 29, 1, COLOR_BLACK);
    lcd_fill_rect(x+28, y+5, 4, 1, COLOR_BLACK); lcd_fill_rect(x+28, y+14, 4, 1, COLOR_BLACK);
    lcd_fill_rect(x+31, y+5, 1, 11, COLOR_BLACK);
    if (lv > 4) lv = 4;
    int cx = x;
    for (int j = 0; j < lv; j++) { lcd_fill_rect(cx+3, y+3, 5, 15, COLOR_BLACK); cx += 6; }
}

// ---- ASCII 8x16 ----
void lcd_show_char(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg, bool bf) {
    if (ch < 0x20 || ch > 0x7E) return;
    int idx = (ch - 0x20) * 16;
    for (int r = 0; r < 16; r++) {
        uint8_t t = nAsciiDot[idx + r];
        for (int c = 0; c < 8; c++, t <<= 1) {
            if (t & 0x80) lcd_draw_pixel(x + c, y, fg);
            else if (bf)  lcd_draw_pixel(x + c, y, bg);
        }
        y++;
    }
}
void lcd_show_gb1616(uint16_t x, uint16_t y, const uint8_t code[2], uint16_t fg, uint16_t bg, bool bf) {
    for (int k = 0; k < codeGB_16_count; k++) {
        if (codeGB_16[k].Index[0] != code[0] || codeGB_16[k].Index[1] != code[1]) continue;
        for (int i = 0; i < 32; i++) {
            uint16_t m = codeGB_16[k].Msk[i];
            for (int j = 0; j < 8; j++, m <<= 1) {
                if (m & 0x80) lcd_draw_pixel(x+j, y, fg);
                else if (bf)  lcd_draw_pixel(x+j, y, bg);
            }
            if (i % 2) { y++; x -= 8; } else { x += 8; }
        }
        return;
    }
}
void lcd_put_string(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, bool bf) {
    int cw = 0;
    while (*s) {
        if ((uint8_t)*s < 0x80) { lcd_show_char(x + cw * 8, y, *s++, fg, bg, bf); cw++; }
        else { uint8_t gb[2] = {(uint8_t)s[0], (uint8_t)s[1]}; lcd_show_gb1616(x + cw * 8, y, gb, fg, bg, bf); s += 2; cw += 2; }
    }
}

// ---- 图片 ----
void lcd_draw_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *data) {
    lcd_set_window(x, y, x + w - 1, y + h - 1);
    lcd_write_pixels_dma(data, (uint32_t)w * h * 2);
}

// 返回 DMA 传输的字节数 (0 = 纯 memcpy, 未发 SPI)
static size_t _dma_bytes = 0;  // 供外部读取 DMA 字节量
static int64_t _cpu_us  = 0;   // 供外部读取 CPU 耗时 (memcpy)
size_t lcd_get_last_dma_bytes(void) { return _dma_bytes; }
int64_t lcd_get_last_cpu_us(void) { return _cpu_us; }

// ---- 原子滑动 (DMA) ----
void lcd_draw_image_shifted(uint16_t xo, uint16_t xn, uint16_t y, uint16_t w, uint16_t h, const uint8_t *data, uint16_t bg) {
    _cpu_us = 0; _dma_bytes = 0;
    int dx = (int)xn - (int)xo;
    if (dx == 0) { lcd_draw_image(xn, y, w, h, data); return; }
    uint16_t gap = dx > 0 ? (uint16_t)dx : (uint16_t)(-dx);
    uint16_t xs  = dx > 0 ? xo : xn;
    uint16_t dw  = w + gap;
    uint32_t tb  = (uint32_t)dw * h * 2;
    uint32_t rb  = (uint32_t)w * 2;
    uint16_t gb  = gap * 2;

    lcd_set_window(xs, y, xs + dw - 1, y + h - 1);

    // Build frame buffer into static DMA-capable memory
    static uint8_t *fb = NULL; static uint32_t fbsz = 0;
    if (tb > fbsz) { if (fb) free(fb); fb = malloc(tb); fbsz = tb; }
    if (!fb) return;

    int64_t cpu_t0 = esp_timer_get_time();
    // bg is already byte-swapped via COLOR_xxx macros
    static uint16_t gap_tpl[16]; for (int i = 0; i < gap; i++) gap_tpl[i] = bg;

    uint8_t *dst = fb;
    if (dx > 0) {
        for (uint16_t r = 0; r < h; r++) {
            memcpy(dst, gap_tpl, gb); dst += gb;
            memcpy(dst, &data[r * rb], rb); dst += rb;
        }
    } else {
        for (uint16_t r = 0; r < h; r++) {
            memcpy(dst, &data[r * rb], rb); dst += rb;
            memcpy(dst, gap_tpl, gb); dst += gb;
        }
    }
    _cpu_us = esp_timer_get_time() - cpu_t0;

    lcd_write_pixels_dma(fb, tb);
    _dma_bytes = tb;
}

// ---- 缓冲渲染 (用于性能条) ----
void lcd_render_char_buf(uint16_t *b, int bw, int x, int y, char ch, uint16_t fg, uint16_t bg) {
    if (ch < 0x20 || ch > 0x7E) return;
    int idx = (ch - 0x20) * 16;
    for (int r = 0; r < 16; r++) {
        uint8_t t = nAsciiDot[idx + r]; uint16_t *d = &b[(y + r) * bw + x];
        for (int c = 0; c < 8; c++, t <<= 1) d[c] = (t & 0x80) ? fg : bg;
    }
}
void lcd_render_string_buf(uint16_t *b, int bw, int x, int y, const char *s, uint16_t fg, uint16_t bg) {
    while (*s) { lcd_render_char_buf(b, bw, x, y, *s++, fg, bg); x += 8; }
}
void lcd_render_gb1616_buf(uint16_t *b, int bw, int x, int y, const uint8_t code[2], uint16_t fg, uint16_t bg) {
    for (int k = 0; k < codeGB_16_count; k++) {
        if (codeGB_16[k].Index[0] != code[0] || codeGB_16[k].Index[1] != code[1]) continue;
        for (int i = 0; i < 32; i++) {
            uint8_t  m = codeGB_16[k].Msk[i]; uint16_t *d = &b[(y + (i/2)) * bw + x + (i%2)*8];
            for (int j = 0; j < 8; j++, m <<= 1) d[j] = (m & 0x80) ? fg : bg;
        }
        return;
    }
}
void lcd_render_mixed_buf(uint16_t *b, int bw, int x, int y, const char *s, uint16_t fg, uint16_t bg) {
    int cw = 0;
    while (*s) {
        if ((uint8_t)*s < 0x80) { lcd_render_char_buf(b, bw, x + cw*8, y, *s++, fg, bg); cw++; }
        else { uint8_t gb[2]={(uint8_t)s[0],(uint8_t)s[1]}; lcd_render_gb1616_buf(b, bw, x + cw*8, y, gb, fg, bg); s+=2; cw+=2; }
    }
}
