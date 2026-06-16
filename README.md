# TK015F4000 ESP32-S3 Arduino 工程

> 基于 ESP32-S3 硬件 SPI 驱动 TK015F4000 (ST7796S) 320×320 LCD  
> 功能对标 TK499 参考工程 (Keil MDK)

---

## 目录

1. [项目概述](#1-项目概述)
2. [硬件连接](#2-硬件连接)
3. [开发环境搭建](#3-开发环境搭建)
4. [工程文件结构](#4-工程文件结构)
5. [快速开始](#5-快速开始)
6. [功能详解](#6-功能详解)
7. [API 参考](#7-api-参考)
8. [性能数据](#8-性能数据)
9. [驱动架构](#9-驱动架构)
10. [调试指南](#10-调试指南)
11. [故障排除](#11-故障排除)
12. [参考资料](#12-参考资料)

---

## 1. 项目概述

| 项目 | 说明 |
|------|------|
| **显示屏** | TK015F4000，320×320 像素 |
| **驱动 IC** | ST7796S |
| **主控** | ESP32-S3 (Xtensa LX7 @ 240MHz) |
| **接口** | 4-Wire SPI (SCK / MOSI / CS / DC) |
| **颜色** | 16-bit RGB565 |
| **SPI 总线** | SPI2_HOST (HSPI)，40MHz，Mode 3 |
| **开发环境** | Arduino IDE 2.x + ESP32 Arduino Core 3.x |
| **依赖库** | 无（仅内置 SPI 库） |

### 功能清单

| 功能 | 对标 TK499 | 状态 |
|------|:---------:|:----:|
| ST7796S 硬件初始化 | ✅ | 完整移植 |
| 320×320 蓝色背景 | ✅ | fillRect |
| 电池电量图标 | ✅ | drawBattery |
| ASCII 8×16 文字 | ✅ | showChar |
| GB1616 16×16 中文 | ✅ | showGB1616 |
| 中英文混排文本 | ✅ | putString |
| 128×128 RGB565 图标 | ✅ | drawImage |
| 图标滑动动画 (60FPS) | ✅ | drawImageShifted |
| 背光 PWM 控制 | ✅ | LEDC |
| 串口调试输出 | ✅ | 460800 bps |
| **原子 SPI 防闪烁** | 🚀 | drawImageShifted |
| **块缓冲填充优化** | 🚀 | fillRect |

---

## 2. 硬件连接

### 2.1 最终确认的引脚映射

ESP32-S3 底板 (带摄像头接口) 通过 **H5+H6 连接器** 连接到 TK015F4000 转接板：

```
ESP32-S3 底板                    TK015F4000 转接板
┌───────────────────┐           ┌──────────────────┐
│ H6 pin1: IO16  CS ─┼───────────┼─► CS  (片选)     │
│ H6 pin2: IO6  SCK ─┼───────────┼─► SCK (时钟)     │
│ H6 pin3: IO7 MISO ─┼───────────┼─► DC  (命令/数据) │  ← MISO 复用为 DC
│ H6 pin4: IO15 MOSI─┼───────────┼─► SDA (数据)     │
│ H5:      IO4     ──┼───────────┼─► RST (复位)     │
│ H5:      IO5     ──┼───────────┼─► BL  (背光PWM)  │
│ H5:      3.3V    ──┼───────────┼─► VCC            │
│         GND      ──┼───────────┼─► GND            │
└───────────────────┘           └────────┬─────────┘
                                        │
                                 ┌──────┴──────┐
                                 │ TK015F4000  │
                                 │ 320×320 LCD │
                                 └─────────────┘
```

### 2.2 引脚速查表

| ESP32 引脚 | 底板位置 | LCD 信号 | 方向 | 功能 |
|:---:|:---:|:---:|:---:|---|
| **IO16** | H6 pin1 | CS | OUT | SPI 片选 (LOW 有效) |
| **IO6** | H6 pin2 | SCK | OUT | SPI 时钟 (40MHz) |
| **IO7** | H6 pin3 | DC | OUT | 数据/命令选择 (HIGH=数据) |
| **IO15** | H6 pin4 | MOSI | OUT | SPI 主机数据输出 |
| **IO4** | H5 | RST | OUT | LCD 硬件复位 (LOW 复位) |
| **IO5** | H5 | BL | OUT | 背光 LEDC PWM (5kHz) |
| **3.3V** | H5 | VCC | PWR | 电源正极 |
| **GND** | — | GND | PWR | 电源地 |

> **注意**: H6 pin3 在底板上标注为 MISO，但由于 4-Wire SPI LCD 只需要写操作，该引脚被复用为 DC（Data/Command）。如果在其他项目中使用此底板做标准 SPI 通信，需注意此差异。

### 2.3 连接器实物参考

| 连接器 | 型号 | 引脚数 |
|--------|------|:------:|
| H5 | PZ254V-11-04P | 4 |
| H6 | PZ254V-11-04P | 4 |

---

## 3. 开发环境搭建

### 3.1 安装步骤

1. **安装 Arduino IDE 2.x**
   - 下载: https://www.arduino.cc/en/software

2. **安装 ESP32 开发板支持**
   - `File` → `Preferences` → `Additional Boards Manager URLs`:
     ```
     https://espressif.github.io/arduino-esp32/package_esp32_index.json
     ```
   - `Tools` → `Board` → `Boards Manager` → 搜索 `esp32` → 安装

3. **选择开发板**
   - `Tools` → `Board` → `esp32` → `ESP32S3 Dev Module`

### 3.2 必需配置

| 配置项 | 值 | 说明 |
|--------|-----|------|
| **USB CDC On Boot** | **Enabled** | 必须！否则串口无输出 |
| Flash Size | 根据实际 | 通常 8MB 或 16MB |
| PSRAM | 根据实际 | OPI PSRAM 或 Disabled |
| Upload Speed | 921600 | — |

---

## 4. 工程文件结构

```
arduino/TK015F4000_ESP32S3/
├── TK015F4000_ESP32S3.ino    # 主程序 (9.5 KB)
│   ├── setup()                 → 系统初始化 + 全功能演示
│   └── loop()                  → 60FPS 原子滑动动画
│
├── lcd_driver.h              # LCD 驱动头文件 (7.5 KB)
│   ├── 引脚/SPI/颜色宏定义
│   ├── LCD_ST7796 类声明
│   └── 内联底层函数 (writeCommand, setWindow, pushPixels)
│
├── lcd_driver.cpp            # LCD 驱动实现 (16.5 KB)
│   ├── begin()                 → GPIO + SPI + LCD 初始化
│   ├── initDisplay()           → ST7796S 寄存器序列 (移植自 TK499)
│   ├── fillRect()              → 块缓冲填充 (512px 缓冲)
│   ├── drawImage()             → RGB565 图片绘制
│   ├── drawImageShifted()      → ★ 原子滑动绘制
│   ├── drawBattery()           → 电池图标
│   ├── showChar()              → ASCII 8×16 字符
│   ├── showGB1616()            → GB2312 16×16 汉字 (22字库)
│   └── putString()             → 中英文混排
│
├── ascii_font.h              # 8×16 ASCII 点阵字库 (9.3 KB)
├── gb1616_font.h             # 16×16 GB2312 汉字点阵字库 (5.1 KB, 22字)
├── icon_128x128.h            # 128×128 RGB565 图标数据 (164 KB)
└── README.md                 # 本文档
```

---

## 5. 快速开始

### 5.1 编译烧录

```
1. Arduino IDE → Open → TK015F4000_ESP32S3.ino
2. Tools → Port → 选择 ESP32-S3 所在的 COM 口
3. Tools → USB CDC On Boot → Enabled
4. 点击 Upload (→)
5. 等待 "Connecting..." → "Done uploading"
6. Tools → Serial Monitor → 波特率 460800
```

### 5.2 预期效果

烧录完成后，LCD 依次显示：

| 顺序 | 显示内容 | 持续时间 |
|:----:|----------|:--------:|
| 1 | 蓝色背景 (320×320) | 静态 |
| 2 | 电池图标 (右上角，2格) | 静态 |
| 3 | 型号、公司、电话等文字 | 静态 |
| 4 | 128×128 彩色图标 | 动画 |

串口输出 (460800 bps):
```
========================================
  TK015F4000  ESP32-S3  Arduino  Demo
========================================
  MCU:      ESP32-S3 @ 240MHz
  Flash:    16 MB
  PSRAM:    8 MB
  LCD:      TK015F4000 (ST7796S) 320x320
  SPI:      Mode3, 40 MHz, 4-Wire
  Pins:     CS=16 SCK=6 MOSI=15 DC=7 RST=4 BL=5

========== LCD begin() ==========
  Pin mapping: CS=16 DC=7 RST=4 BL=5 SCK=6 MOSI=15
  [1] Hardware reset...
  [2] SPI init...
  SPI Mode=3, Freq=40 MHz
  [3] LCD init sequence...
  [3] LCD init done.
  [4] Backlight ON
========== LCD begin() OK ==========

Running full demo...
Demo ready. Icon animation running.
========================================
```

---

## 6. 功能详解

### 6.1 初始化流程

```
begin()
  ├── GPIO 配置 (CS, DC, RST, BL → OUTPUT)
  ├── 硬件复位 (RST LOW 10ms → HIGH 150ms 等待)
  ├── SPI 初始化 (SPI2_HOST, 40MHz, Mode3, MSB)
  ├── initDisplay()
  │   ├── Sleep Out (0x11)
  │   ├── MADCTL, COLMOD, 面板参数
  │   ├── Gamma 校正 正/负曲线
  │   ├── Display ON (0x29) + Inversion ON (0x21)
  │   └── 覆盖为 RGB565 16-bit 模式 (0x3A=0x55)
  └── 背光 ON (LEDC PWM)
```

### 6.2 文字系统

**ASCII 字符**: 8×16 点阵，支持 95 个可打印字符 (`0x20`–`0x7E`)

**GB1616 汉字**: 16×16 点阵，按 GB2312 双字节编码查找，共 22 个常用汉字：
> `型号：深圳市浩仁杰科技有限公司电话支持函屏`

**混排机制**: `putString()` 自动识别字节值区分 ASCII (< 0x80) 和中文 (≥ 0x80)

**使用示例**:
```cpp
// 直接 ASCII 字符串
lcd.putString(10, 60, "Hello World!", TFT_RED, TFT_BLACK, false);

// GB2312 中文 (UTF-8 源文件需用字节序列)
static const char str[] = {
    (char)0xD0, (char)0xCD, (char)0xBA, (char)0xC5, '\0'  // "型号"
};
lcd.putString(10, 20, str, TFT_RED, TFT_YELLOW, false);
```

### 6.3 动画系统 — 原子 SPI 滑动

这是本工程最核心的性能优化。

**问题**: 传统的 "擦除→绘制" 两步操作之间有 CS 间隙，产生视觉闪烁。

**解决方案**: `drawImageShifted()` 将擦除和绘制合并为单次 SPI 事务：

```
传统方式:
  setWindow → 写旧位置蓝色 → CS↑
  setWindow → 写新位置图标 → CS↑        ← 间隙产生闪烁

原子方式:
  setWindow(脏矩形) → [gap×蓝 | 图标×128]×128行 → CS↑
  └────────────────── 全程 CS=LOW ──────────────────┘  ← 零间隙
```

**逐行数据流** (右移 3px 为例):
```
第1行: [3px 蓝] [128px 图标第1行]
第2行: [3px 蓝] [128px 图标第2行]
  ⋮
第128行: [3px 蓝] [128px 图标第128行]
```

**调用方式**:
```cpp
// xOld→xNew: 自动判断方向、计算间隙宽度
lcd.drawImageShifted(xPos, newX, y, w, h, iconData, TFT_BLUE);
// dx=0 (撞边) → 自动退化为纯 drawImage
```

---

## 7. API 参考

### 7.1 初始化

```cpp
void lcd.begin()
```
初始化 GPIO、硬件复位、SPI 总线、ST7796S 寄存器序列、背光。

---

### 7.2 绘制函数

```cpp
void lcd.fillRect(x, y, w, h, color)
```
矩形填充。使用 512 像素块缓冲 + `writeBytes` 批量传输优化。

```cpp
void lcd.drawPixel(x, y, color)
```
单像素绘制。

```cpp
void lcd.drawVLine(x, y0, y1, color)
void lcd.drawHLine(x0, x1, y, color)
```
垂直/水平线。内部委托给 `fillRect`。

```cpp
void lcd.drawBattery(x, y, level)   // level: 0~4
```
电池图标 (TK499 同款)。外框 + 内部电量格。

---

### 7.3 文字

```cpp
void lcd.showChar(x, y, ch, fg, bg, bgFlag)
```
显示单个 8×16 ASCII 字符。`bgFlag=true` 时绘制背景色。

```cpp
void lcd.showGB1616(x, y, gb_code, fg, bg, bgFlag)
```
显示单个 16×16 中文 GB2312 字符。`gb_code`: 2 字节 GB2312 编码。

```cpp
void lcd.putString(x, y, str, fg, bg, bgFlag)
```
混合字符串。自动识别 ASCII (< 0x80) 和 GB2312 (≥ 0x80)。

---

### 7.4 图片

```cpp
void lcd.drawImage(x, y, w, h, data)
```
绘制 RGB565 图片。`data` 为 2 字节/像素的原始 RGB565 字节流。

```cpp
void lcd.drawImageShifted(xOld, xNew, y, w, h, data, bgColor)
```
**原子滑动绘制**。单次 SPI 事务完成图标移动 + 拖尾擦除。  
正偏移 = 右移，负偏移 = 左移，零偏移 = 纯绘制。

---

### 7.5 低级操作

```cpp
void lcd.setWindow(x1, y1, x2, y2)    // 设置写入窗口 (CASET + RASET + RAMWR)
void lcd.startPixels()                 // DC=1, CS=0
void lcd.writePixel(color)             // 写单个像素 (CS 需已 LOW)
void lcd.endPixels()                   // CS=1
void lcd.pushPixels(data, len)         // 批量写像素 (32KB 分块)
```

### 7.6 硬件控制

```cpp
void lcd.setBacklight(level)           // 0~255, LEDC PWM @ 5kHz
```

---

## 8. 性能数据

### 8.1 帧率分析

| 场景 | 每帧数据量 | SPI 传输时间 | 理论极限 FPS |
|------|-----------|-------------|:-----------:|
| 静止 (dx=0) | 32,768 B | 6.55 ms | 152 |
| 滑动 (dx=3) | 33,536 B | 6.71 ms | 149 |
| 滑动 (dx=5) | 34,048 B | 6.81 ms | 146 |

### 8.2 实际帧率

| 设定 | 帧间隔 | CPU 占用 | 效果 |
|:----:|:------:|:--------:|------|
| **60 FPS** | 16.67 ms | ~40% | 满 LCD 刷新率，零掉帧 |
| 100 FPS | 10 ms | ~68% | 超出 LCD 刷新率，视觉无差异 |
| 149 FPS | 6.71 ms | ~100% | 理论极限，CPU 无余量 |

### 8.3 优化历程

| 版本 | 技术 | 帧率 | 闪烁 |
|:----:|------|:----:|:----:|
| v1 | 逐像素 `transfer16` + 全擦 | ~30 FPS | 明显 |
| v2 | 块缓冲 fillRect + 拖尾擦 | ~60 FPS | 偶尔 |
| v3 | 先画后擦 + 动态条纹 | ~60 FPS | 撞边时 |
| **v4** | **原子 SPI + 统一 dx** | **60 FPS** | **无** |

### 8.4 关键优化技术

| 技术 | 优化前 | 优化后 | 提升 |
|------|--------|--------|:----:|
| fillRect 块缓冲 | 逐像素 `transfer16` × N | 预填 512px 缓冲 + `writeBytes` | ~100× |
| 拖尾擦除 | 全擦 16384 px/帧 | 3px × 128 = 384 px/帧 | ~43× |
| 原子 SPI | 擦+绘 两次 CS 事务 | 合并为单次 CS 事务 | 消除闪烁 |
| SPI 块大小 | 4KB 分块 | 32KB 分块 | 减少事务开销 |

---

## 9. 驱动架构

### 9.1 分层结构

```
┌──────────────────────────────────────┐
│  Application (.ino)                  │
│  setup() → fullDemo()                │
│  loop()  → drawImageShifted() 动画   │
├──────────────────────────────────────┤
│  LCD Driver (lcd_driver.h/.cpp)      │
│  ├─ fillRect / drawImage (高级API)   │
│  ├─ drawImageShifted (原子滑动)      │
│  ├─ setWindow / pushPixels (中级API) │
│  └─ showChar / putString (文字)      │
├──────────────────────────────────────┤
│  SPI Transport                       │
│  ├─ writeCommand / writeData (单字节)│
│  ├─ writeData16 (16-bit)             │
│  └─ SPI.writeBytes (批量DMA)         │
├──────────────────────────────────────┤
│  Hardware                            │
│  ├─ ESP32-S3 SPI2_HOST @ 40MHz       │
│  ├─ GPIO 矩阵 (任意引脚 → SPI功能)   │
│  └─ LEDC PWM (背光)                  │
└──────────────────────────────────────┘
```

### 9.2 4-Wire SPI 时序

```
CS   ‾‾‾\________________________/‾‾‾
DC   ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
SCK  ‾‾‾\_/‾\_/‾\_/‾\_/‾\___/‾\_/‾‾‾
MOSI ----<D7><D6><D5><D4><D3><D2><D1><D0>----

Mode 3: CPOL=1 (空闲高), CPHA=1 (第2边沿采样=上升沿)
DC=LOW → 命令字节
DC=HIGH → 数据字节(组)
```

---

## 10. 调试指南

### 10.1 引脚映射调整

如需修改引脚，编辑 `lcd_driver.h` 第 28-44 行：

```cpp
#define TFT_SCK   6
#define TFT_MOSI  15
#define TFT_CS    16
#define TFT_DC    7
#define TFT_RST   4
#define TFT_BL    5
```

### 10.2 SPI 参数调整

```cpp
#define TFT_SPI_FREQ  40000000    // 20~60 MHz
#define TFT_SPI_MODE  SPI_MODE3   // Mode 0 或 Mode 3
```

### 10.3 添加新汉字到字库

1. 找到汉字的 GB2312 编码（如 "中" = 0xD6D0）
2. 用字模工具生成 16×16 点阵数据 (32 字节)
3. 在 `gb1616_font.h` 中添加条目：
   ```c
   {0xD6, 0xD0}, 0x..., 0x..., ... // 中
   ```
4. 全局对象 `codeGB_16_count` 会自动更新

---

## 11. 故障排除

| 现象 | 原因 | 解决 |
|------|------|------|
| **雪花/噪点** | SPI 通信完全未建立 | 检查 CS/SCK/MOSI/DC 接线；尝试 SPI_MODE0 |
| **黑屏 (背光亮)** | DC/RST 映射错误 | 互换 DC 和 RST 引脚定义 |
| **白屏** | RST 引脚未正常工作 | 检查 RST 连接；手动复位 LCD |
| **花屏/颜色错** | SPI 频率过高或接线质量问题 | 降低 TFT_SPI_FREQ 至 20MHz |
| **中文显示空白** | 汉字不在字库中 | 检查 GB2312 编码，添加字模 |
| **串口无输出** | USB CDC 未启用 | Tools → USB CDC On Boot → Enabled |
| **编译 "HSPI" 错误** | 旧版 ESP32 Core | 将 HSPI 改为 SPI2_HOST |
| **图标滑动卡顿** | SPI 频率过低 | 提高至 40-60MHz |

---

## 12. 参考资料

| 文件 | 路径 | 说明 |
|------|------|------|
| TK499 参考工程 | `../Doc/TK499_LCD_TK015F4000_硬件4线SPI_DMA/` | 原始 Keil MDK 工程 |
| ST7796S 规格书 | `../TK015F4000整理资料/ST7796S_SPEC_V1.0驱动芯片手册.pdf` | 驱动 IC 完整手册 |
| 底板原理图 | `../Doc/ESP32-带摄像头底板原理图.pdf` | 底板电路图 |
| ESP32-S3 数据手册 | `../Doc/esp32-s3_datasheet_cn.pdf` | MCU 中文手册 |
| 转接板资料 | `../TK015F4000整理资料/` | PCB 及机构图 |

---

*移植自 TK499 参考工程 (原作者: xiao chen)*  
*最后更新: 2026-06-09*
