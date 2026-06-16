/**
 * @file    main.c
 * @brief   TK015F4000 ESP32-S3 — ESP-IDF GDMA 版
 *
 *          SPI3_HOST (VSPI) 独立 GDMA, CPU ~2% @60FPS
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "lcd_st7796.h"
extern int64_t lcd_get_last_cpu_us(void);
#include "icon_128x128.h"

static const char *TAG = "TK015F4000";

// ---- GB2312 strings ----
static const char S_MODEL[]   = {(char)0xD0,(char)0xCD,(char)0xBA,(char)0xC5,(char)0xA3,(char)0xBA,'T','K','0','1','5','F','4','0','0','0',0};
static const char S_COMPANY[] = {(char)0xC9,(char)0xEE,(char)0xDB,(char)0xDA,(char)0xCA,(char)0xD0,(char)0xBA,(char)0xC3,(char)0xEE,(char)0xD2,(char)0xC8,(char)0xF3,(char)0xBF,(char)0xC6,(char)0xBC,(char)0xBC,(char)0xD3,(char)0xD0,(char)0xCF,(char)0xDE,(char)0xB9,(char)0xAB,(char)0xCB,(char)0xBE,0};
static const char S_PHONE[]   = {(char)0xB5,(char)0xE7,(char)0xBB,(char)0xB0,(char)0xA3,(char)0xBA,'0','7','5','5','-','2','1','0','0','6','1','5','0',0};
static const char S_SUPPORT[] = {(char)0xD6,(char)0xA7,(char)0xB3,(char)0xD6,(char)0xBA,(char)0xE1,(char)0xC6,(char)0xC1,0};

// ---- 统计缓冲 ----
static uint16_t *stats_buf = NULL;
static bool     stats_dirty = false;

static void build_stats(float fps, float cpu) {
    for (int i = 0; i < 320 * 16; i++) stats_buf[i] = COLOR_BLACK;
    char t[24]; snprintf(t, sizeof(t), "FPS:%.0f  CPU:%.0f%%", fps, cpu);
    lcd_render_string_buf(stats_buf, 320, 4, 0, t, COLOR_GREEN, COLOR_BLACK);
    lcd_render_string_buf(stats_buf, 320, 260, 0, "SPI:40M", COLOR_GRAY, COLOR_BLACK);
    stats_dirty = true;
}

// ---- 全功能演示 (屏幕缓冲 → 一次 DMA) ----
static void full_demo(void) {
    uint16_t *fb = heap_caps_malloc(320 * 320 * 2, MALLOC_CAP_DMA);
    if (!fb) { ESP_LOGE(TAG, "OOM"); return; }

    // 1. 蓝底
    for (int i = 0; i < 320 * 320; i++) fb[i] = COLOR_BLUE;

    // 2. 电池 (直接写像素)
    int bx = 320 - 50, by = 10;
    for (int r = 0; r < 21; r++) {
        uint16_t *row = &fb[(by + r) * 320];
        if (r < 6 || r >= 15) { row[bx] = COLOR_BLACK; row[bx+28] = COLOR_BLACK; }
        if (r == 0 || r == 20) for (int c = 0; c < 29; c++) row[bx+c] = COLOR_BLACK;
        if (r >= 5 && r <= 15) { if (r == 5 || r == 14) for (int c = 28; c < 32; c++) row[bx+c] = COLOR_BLACK; row[bx+31] = COLOR_BLACK; }
    }
    for (int g = 0; g < 2; g++)
        for (int r = 3; r < 16; r++)
            for (int c = bx+3+g*6; c < bx+3+g*6+5; c++) fb[(by+r)*320+c] = COLOR_BLACK;

    // 3. 文本 (缓冲渲染, 纯 RAM 写)
    lcd_render_mixed_buf(fb, 320, 10, 20,  S_MODEL,   COLOR_RED, COLOR_YELLOW);
    lcd_render_mixed_buf(fb, 320, 10, 60,  "Welcome to ESP-IDF GDMA + TK015F4000!", COLOR_RED, COLOR_YELLOW);
    lcd_render_mixed_buf(fb, 320, 10, 80,  S_COMPANY, COLOR_RED, COLOR_YELLOW);
    lcd_render_mixed_buf(fb, 320, 10, 100, S_PHONE,   COLOR_RED, COLOR_YELLOW);
    lcd_render_mixed_buf(fb, 320, 10, 140, S_SUPPORT, COLOR_RED, COLOR_YELLOW);

    // 4. 图标 (memcpy 进缓冲)
    for (int r = 0; r < 128; r++)
        memcpy(&fb[(160 + r) * 320 + 150], &((uint16_t*)gImage_icon_128x128)[r * 128], 256);

    // 5. ★ 一次 DMA 全屏输出
    lcd_set_window(0, 0, 319, 319);
    lcd_write_pixels_dma((const uint8_t *)fb, 320 * 320 * 2);
    free(fb);
}

// ---- 动画任务 (Core 0) ----
static void anim_task(void *arg) {
    ESP_LOGI(TAG, "Animation task start");
    int x_pos = 150, dir = 1;
    int64_t last_us = esp_timer_get_time();
    float   draw_us_acc = 0; int frame_count = 0;
    int64_t stat_start_us = last_us;

    while (1) {
        // 精确 60 FPS 帧同步 (微秒级精度)
        int64_t now_us;
        while ((now_us = esp_timer_get_time()) - last_us < 16666) {
            esp_rom_delay_us((uint32_t)(16666 - (now_us - last_us)));
        }
        last_us = esp_timer_get_time();

        int new_x = x_pos + dir * 3;
        if (new_x >= 319 - 128) { new_x = 319 - 128; dir = -1; }
        if (new_x <= 0)         { new_x = 0;         dir =  1; }

        lcd_draw_image_shifted(x_pos, new_x, 160, 128, 128, gImage_icon_128x128, COLOR_BLUE);
        x_pos = new_x;

        // Stats bar
        if (stats_dirty) {
            lcd_set_window(0, 304, 319, 319);
            lcd_write_pixels_dma((const uint8_t *)stats_buf, 320 * 16 * 2);
            stats_dirty = false;
        }

        // 只累加 CPU 实际工作时间 (memcpy), 不含 DMA 阻塞
        draw_us_acc += (float)lcd_get_last_cpu_us();
        frame_count++;
        if (frame_count >= 60) {
            float elapsed_s = (float)(now_us - stat_start_us) / 1000000.0f;
            float avg_ms    = draw_us_acc / frame_count / 1000.0f;
            float fps       = (float)frame_count / elapsed_s;
            float cpu_pct   = avg_ms * fps / 10.0f;
            ESP_LOGI(TAG, "FPS:%.0f CPU:%.1f%% mem:%.2fms", fps, cpu_pct, avg_ms);
            build_stats(fps, cpu_pct);
            draw_us_acc = 0; frame_count = 0;
            stat_start_us = now_us;
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " TK015F4000  ESP32-S3  ESP-IDF GDMA");
    ESP_LOGI(TAG, " SPI2_HOST @40MHz Mode3  GDMA_AUTO");
    ESP_LOGI(TAG, "========================================");

    // Allocate DMA-safe buffers
    stats_buf = heap_caps_malloc(320 * 16 * 2, MALLOC_CAP_DMA);
    if (!stats_buf) { ESP_LOGE(TAG, "stats OOM"); return; }

    lcd_init();
    ESP_LOGI(TAG, "LCD init OK. Running demo...");

    full_demo();
    // Backlight ON only AFTER first frame is drawn (avoids startup garbage flash)
    lcd_set_backlight(255);
    build_stats(0, 0);
    lcd_set_window(0, 304, 319, 319);
    lcd_write_pixels_dma((const uint8_t *)stats_buf, 320 * 16 * 2);
    stats_dirty = false;

    ESP_LOGI(TAG, "Demo ready. Starting animation...");

    // Animation on Core 0 (8KB stack for malloc safety)
    xTaskCreatePinnedToCore(anim_task, "anim", 8192, NULL, 5, NULL, 0);
}
