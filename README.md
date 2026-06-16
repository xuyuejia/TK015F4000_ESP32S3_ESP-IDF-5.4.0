# TK015F4000 ESP32-S3 ESP-IDF GDMA 工程

> 基于 ESP32-S3 原生 ESP-IDF v5.4 + GDMA 驱动 TK015F4000 (ST7796S) 320×320 LCD  
> CPU 占用仅 ~2%，SPI 纯 DMA 传输，零撕裂原子动画  
> 从 TK499 (Keil MDK) → Arduino → ESP-IDF 三次迭代的最终版本

---

## 1. 项目概述

| 项目 | 说明 |
|------|------|
| **显示屏** | TK015F4000，320×320 像素 |
| **驱动 IC** | ST7796S |
| **主控** | ESP32-S3 (Xtensa LX7 @ 240MHz) |
| **接口** | 4-Wire SPI (CS / SCK / MOSI / DC) |
| **颜色** | 16-bit RGB565 |
| **SPI 总线** | SPI2_HOST (FSPI)，40MHz，Mode 3 |
| **DMA** | SPI_DMA_CH_AUTO (GDMA) |
| **开发环境** | VS Code + ESP-IDF v5.4 |
| **语言** | C (ESP-IDF 原生) |

### 功能清单

| 功能 | 状态 |
|------|:----:|
| ST7796S 硬件初始化 | ✅ |
| 320×320 全屏填充 | ✅ |
| 电池电量图标 | ✅ |
| ASCII 8×16 文字 | ✅ |
| GB2312 16×16 中文（22字） | ✅ |
| 中英文混排文本 | ✅ |
| 128×128 RGB565 图标 | ✅ |
| 图标滑动动画 (60FPS) | ✅ |
| 背光 PWM 控制 (LEDC) | ✅ |
| GDMA 零 CPU 像素传输 | ✅ |
| 原子 SPI 零闪烁动画 | ✅ |
| 屏幕 FPS/CPU 统计覆层 | ✅ |

---

## 2. 硬件连接

```
ESP32-S3 底板 (H5+H6)        TK015F4000 转接板
┌──────────────────────┐     ┌──────────────────┐
│ H6 pin1: IO16  ────────────► CS               │
│ H6 pin2: IO6   ────────────► SCK              │
│ H6 pin3: IO7   ────────────► DC (复用 MISO)    │
│ H6 pin4: IO15  ────────────► MOSI             │
│ H5:      IO4   ────────────► RST              │
│ H5:      IO5   ────────────► BL (背光 PWM)     │
│ H5:      3.3V  ────────────► VCC              │
│         GND    ────────────► GND              │
└──────────────────────┘     └──────────────────┘
```

| ESP32 引脚 | LCD 信号 | 功能 |
|:---:|:---:|---|
| IO16 | CS | 片选 (LOW 有效) |
| IO6 | SCK | SPI 时钟 (40MHz) |
| IO7 | DC | 数据/命令 (HIGH=数据) |
| IO15 | MOSI | SPI 数据输出 |
| IO4 | RST | 硬件复位 (LOW 复位) |
| IO5 | BL | 背光 LEDC PWM @5kHz |

---

## 3. 开发环境

| 工具 | 版本/说明 |
|------|-----------|
| **IDE** | VS Code + ESP-IDF 扩展 |
| **框架** | ESP-IDF v5.4 |
| **编译工具链** | xtensa-esp-elf-gcc 14.2.0 |
| **目标芯片** | esp32s3 |
| **构建系统** | CMake + Ninja |

### 安装

```bash
# 1. 安装 ESP-IDF v5.4
git clone -b v5.4 --recursive https://github.com/espressif/esp-idf.git

# 2. 安装工具链
cd esp-idf && ./install.ps1 esp32s3   # Windows PowerShell

# 3. 激活环境
. ./export.ps1                       # 每次打开终端都要执行

# 4. 设置目标芯片
idf.py set-target esp32s3
```

---

## 4. 工程文件结构

```
TK015F4000_ESPIDF/
├── CMakeLists.txt              # 项目声明 (ESP-IDF v5.4)
├── sdkconfig                   # ESP32-S3 配置 (Kconfig)
├── .gitignore                  # Git 忽略规则
├── README.md                   # 本文档
├── DEVELOPMENT_LOG.md          # 完整开发历程
└── main/
    ├── CMakeLists.txt          # 源文件注册 (依赖: driver, esp_timer)
    ├── main.c                  # 应用入口 (Demo + 60FPS 动画 + 统计)
    ├── lcd_st7796.h            # LCD 驱动头文件 (引脚/颜色/API)
    ├── lcd_st7796.c            # LCD 驱动实现 (DMA/初始化/绘制)
    ├── ascii_font.h            # ASCII 8×16 点阵字库
    ├── gb1616_font.h           # GB2312 16×16 中文字库 (22字)
    └── icon_128x128.h          # 128×128 RGB565 图标
```

---

## 5. 快速开始

```bash
# 1. 激活 ESP-IDF 环境
. /path/to/esp-idf/export.ps1    # Windows
# . /path/to/esp-idf/export.sh   # Linux / macOS

# 2. 编译
idf.py build

# 3. 烧录 (将 COMx 替换为实际串口)
idf.py -p COMx flash

# 4. 查看日志
idf.py -p COMx monitor
```

烧录后 LCD 依次显示：蓝色背景 → 电池图标 → 中文信息 → 图标滑动动画。

---

## 6. 核心架构

### 6.1 SPI 驱动分层

```
┌─────────────────────────────────┐
│   应用层 (main.c)                │
│   full_demo() / anim_task()     │
├─────────────────────────────────┤
│   LCD 驱动层 (lcd_st7796.c)      │
│   set_window / fill_rect / DMA  │
├─────────────────────────────────┤
│   ESP-IDF SPI 驱动               │
│   spi_device_transmit (DMA)     │
│   spi_device_polling_transmit   │
├─────────────────────────────────┤
│   硬件                            │
│   ESP32-S3 SPI2 + GDMA           │
└─────────────────────────────────┘
```

### 6.2 双 SPI 传输模式

| 函数 | 用途 | 数据量 |
|------|------|:---:|
| `spi_device_polling_transmit()` | 命令/寄存器 (CS 手动控制) | 1~2 bytes |
| `spi_device_transmit()` | 像素数据 (GDMA 自动) | ≤ 32KB/chunk |

### 6.3 原子滑动动画

`lcd_draw_image_shifted()` 将"擦旧 + 绘新"合并为单次 CS 事务：

```
逐行数据 (右移 3px):
[3px 背景] [128px 图标行] × 128 行
全程 CS=LOW → 零间隙 → 无闪烁
```

### 6.4 GDMA 工作原理

- ESP-IDF 原生 `spi_device_transmit` 自动使用 GDMA
- CPU 仅负责 `memcpy` 构建帧缓冲 (~0.02ms/帧)
- DMA 异步传输像素数据到 SPI 外设
- **CPU 占用 ~2% @ 60FPS**（Arduino 版 ~40%）

---

## 7. API 参考

```c
// === 初始化 ===
void lcd_init(void);                              // GPIO + SPI + ST7796 寄存器序列
void lcd_set_backlight(uint8_t level);            // 0~255, LEDC PWM

// === 绘制 ===
void lcd_set_window(uint16_t x1, uint16_t y1,     // 设置写入窗口
                    uint16_t x2, uint16_t y2);
void lcd_fill_rect(x, y, w, h, color);            // 矩形填充 (512px 缓冲)
void lcd_fill_screen(color);                      // 全屏填充
void lcd_draw_pixel(x, y, color);                 // 单像素

// === DMA 传输 ===
void lcd_write_pixels_dma(data, len);             // DMA 批量像素 (≤32KB/chunk)
void lcd_write_pixels(data, len);                 // 同 DMA 传输

// === 图片 ===
void lcd_draw_image(x, y, w, h, data);            // 绘制 RGB565 图片
void lcd_draw_image_shifted(xo, xn, y, w, h,      // 原子滑动 (零闪烁)
                            data, bg);
int64_t lcd_get_last_cpu_us(void);                // 获取上次 CPU 耗时

// === 文字 ===
void lcd_show_char(x, y, ch, fg, bg, bf);         // ASCII 8×16
void lcd_show_gb1616(x, y, code, fg, bg, bf);     // GB2312 16×16
void lcd_put_string(x, y, s, fg, bg, bf);         // 中英文混排

// === 缓冲渲染 (用于性能覆层) ===
void lcd_render_char_buf(buf, bw, x, y, ch, fg, bg);
void lcd_render_string_buf(buf, bw, x, y, s, fg, bg);
void lcd_render_mixed_buf(buf, bw, x, y, s, fg, bg);
```

---

## 8. 性能数据

| 指标 | Arduino 版 | ESP-IDF 版 (当前) |
|------|:---:|:---:|
| CPU 占用 @60FPS | ~40% | **~2%** |
| SPI 频率 | 40 MHz | 40 MHz |
| DMA | ❌ | ✅ GDMA |
| 每帧 CPU 耗时 | ~6.7ms | ~0.02ms |
| 动画闪烁 | 无 | 无 |

---

## 9. 故障排除

| 现象 | 原因 | 解决 |
|------|------|------|
| **雪花/噪点** | SPI 通信未建立 | 检查 CS/SCK/MOSI/DC 接线 |
| **黑屏 (背光亮)** | DC/RST 映射错误 | 互换 DC 和 RST 引脚 |
| **白屏** | RST 未正常工作 | 检查 RST 连接，手动复位 |
| **花屏/颜色错** | SPI 频率过高 | 降低至 20MHz |
| **中文显示空白** | 汉字不在字库中 | 添加 GB2312 字模 |
| **编译 ESP-IDF 失败** | 环境未激活 | 先执行 `export.ps1` |

---

## 10. 参考资料

| 文件 | 说明 |
|------|------|
| `DEVELOPMENT_LOG.md` | 完整开发历程（Arduino → ESP-IDF） |
| ST7796S 规格书 | 驱动 IC 手册 |
| ESP32-S3 数据手册 | 中文/英文 |
| 底板原理图 PDF | H5/H6 连接器引脚 |

---

## 11. 更新记录

| 日期 | 内容 |
|------|------|
| 2026-06-08 | 项目创建，Arduino 版移植完成 |
| 2026-06-09 | Arduino 版性能优化，60FPS 动画 |
| 2026-06-16 | 迁移至 ESP-IDF v5.4，启用 GDMA |
| 2026-06-16 | **修复**: SPI 80→40MHz，补全 Sleep Out/Display ON 延迟 |

---

*移植自 TK499 参考工程 • 最终版本: ESP-IDF v5.4 GDMA*
