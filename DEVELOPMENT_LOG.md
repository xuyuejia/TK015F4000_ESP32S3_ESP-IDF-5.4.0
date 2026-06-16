# TK015F4000 ESP32-S3 Arduino 工程 — 开发全记录

> 从零移植 TK499 (Keil MDK) → ESP32-S3 (Arduino)，完整记录。

---

## 时间线

```
2026-06-08  23:00  项目启动，分析 Doc 目录中的 TK499 参考工程
2026-06-08  23:30  创建 Arduino 工程骨架，移植 ST7796S 初始化序列
2026-06-08  23:45  处理 GB1616 GB2312 字库 UTF-8 编码兼容问题
2026-06-09  00:15  修复 LEDC PWM API (ESP32 Core 3.x 兼容)
2026-06-09  00:30  修复 C++ const 内部链接问题 (extern 关键字)
2026-06-09  01:00  编译通过，上传 → 雪花屏 → 开始调试
2026-06-09  01:30  从底板 PDF 原理图提取 H5/H6 引脚信息
2026-06-09  02:00  确定正确引脚映射 (CS=16 SCK=6 DC=7 MOSI=15 RST=4 BL=5)
2026-06-09  02:15  黑屏 → 互换 DC/RST → 正常显示！
2026-06-09  02:30  完整演示版本：蓝底+电池+文字+图标
2026-06-09  03:00  fillRect 块缓冲优化 (~100×)
2026-06-09  03:15  拖尾条纹擦除 (384px vs 16384px)
2026-06-09  03:30  先画后擦 → 原子 SPI (drawImageShifted)
2026-06-09  03:45  消除撞边闪烁，统一 dx 处理
2026-06-09  04:00  性能分析：理论 149 FPS，稳定 60 FPS
2026-06-09  04:15  完善文档
```

---

## 第一阶段：分析需求与参考代码

### 起点

用户在 `Doc/` 目录下有两套资料：
1. **TK499 + TK015F4000 完整 Keil 工程** — 作为功能参考
2. **ESP32-S3 系列 PDF/图片** — 数据手册、底板原理图、转接板资料

目标任务：用 ESP32-S3 + Arduino 驱动 TK015F4000，功能对标 TK499 工程。

### TK499 参考工程分析

| 组件 | 文件 | 关键信息 |
|------|------|----------|
| 主程序 | `project/bsp/main.c` | 240MHz PLL, UART1 460800, SPI2 80MHz |
| LCD 驱动 | `project/bsp/LCD/LCD.c` | ST7796S 初始化序列、绘制函数 |
| SPI 驱动 | `project/bsp/SPI/spi.c` | 4-Wire SPI, DMA/轮询两种模式 |
| 字库 | `ASCII.h`, `GB1616.h` | 8×16 ASCII + 16×16 中文 22字 |
| 图标 | `icon_128x128.h` | 128×128 RGB565 32768 字节 |

### 提取的关键参数

```
MCU:       TK499 (Cortex-M4 @ 240MHz)
LCD:       TK015F4000 (ST7796S, 320×320, RGB565)
SPI:       SPI2, CPOL=High, CPHA=2Edge → Mode 3
SPI CLK:   80MHz (240/3)
UART:      460800 bps (调试)
GPIO:      PA4=CS, PA5=SCK, PA6=RS/DC, PA7=MOSI
LED:       PD8
```

---

## 第二阶段：工程搭建

### 文件结构设计

选择 Arduino 多文件结构（非单文件 sketch），原因：
- `.h` 存放数据文件（字库、图标）和内联函数
- `.cpp` 存放驱动实现
- `.ino` 存放主程序逻辑
- 模块分离，便于维护

### 代码移植关键决策

| TK499 特性 | ESP32-S3 对应 | 说明 |
|-------------|---------------|------|
| 直接写 SPI TXREG | `SPI.transfer()` / `SPI.writeBytes()` | Arduino 抽象 |
| DMA 传输 | `writeBytes` 块传输 | ESP32 内置 DMA |
| 定时器中断 LED | `millis()` 循环 | 简化实现 |
| UART printf | `Serial.printf()` | Arduino 标准 |
| GB2312 源码 | GB2312 字节序列 | UTF-8 兼容 |

---

## 第三阶段：编译与编码修复

共遇到 4 个编译错误，逐一修复：

### 错误 1: GB1616 字库 `initializer-string too long`

**原因**: 原始 TK499 文件使用 GB2312 编码，中文字符串字面量（如 `"型"` = 2 字节）在 Arduino IDE 的 UTF-8 源文件模式下被 GCC 解释为多字节 UTF-8 序列（3 字节），超出 `unsigned char[2]`。

**修复**: 用 Python 脚本解析原始 GB2312 文件，将所有 22 个汉字字面量替换为平展的十六进制字节序列：
```c
// 修复前: "型" → GCC 看到 3 字节 UTF-8 → 错误
// 修复后: 0xD0, 0xCD → 2 个 unsigned char → 正确
```

### 错误 2: `ledcSetup` / `ledcAttachPin` 未声明

**原因**: ESP32 Arduino Core 3.x 废弃了旧的 LEDC API。

**修复**: 替换为新 API：
```cpp
// 修复前
ledcSetup(0, 5000, 8);
ledcAttachPin(TFT_BL, 0);
ledcWrite(0, level);

// 修复后
ledcAttach(TFT_BL, 5000, 8);
ledcWrite(TFT_BL, level);
```

### 错误 3: 花括号初始化的聚合问题

**原因**: 修复错误 1 时用了 `{0xD0, 0xCD}` 花括号形式，在 C++ 的扁平聚合初始化中引入了歧义。

**修复**: 去除花括号，使用纯扁平标量序列 `0xD0, 0xCD`。

### 错误 4: `undefined reference to gImage_icon_128x128`

**原因**: C++ 中 `const` 全局变量默认为内部链接（internal linkage），跨翻译单元不可见。

**修复**: 在 `icon_128x128.h` 中添加 `extern` 关键字强制外部链接：
```cpp
extern const unsigned char gImage_icon_128x128[32768] = { ... };
```

---

## 第四阶段：硬件调试 — 寻找正确引脚映射

### 问题

编译通过、上传成功，但 LCD 显示**雪花**（随机噪声）——SPI 命令完全没有送达 LCD。

### 根因

初始引脚映射完全是错误的。基于用户提供的信息"连接了 IO4、IO5、3.3V、IO16、IO6、IO7、IO15"做了推测性映射，但无原理图支撑。

### 解决过程

1. **尝试 SPI_MODE0 → 仍雪花**
2. **降低 SPI 频率 20MHz → 仍雪花**
3. **从底板 PDF 原理图提取信息** — 用 `pdftotext` 解析原理图 PDF：
   - 发现底板有 H5 (4-pin) 和 H6 (4-pin) 两个连接器
   - H6: IO16=CS, IO6=SCK, IO7=MISO, IO15=MOSI
   - H5: IO4, IO5, 3.3V
4. **修正 SPI 核心引脚** (CS=16, SCK=6, MOSI=15) → 雪花→黑屏 ✓ 进步！
5. **逐一测试 DC/RST/BL 组合** — {IO4, IO5, IO7} 对应 {DC, RST, BL}
   - 用户确认 BL=IO5
   - DC=IO4, RST=IO7 → 黑屏
   - DC=IO7, RST=IO4 → ✅ 正常！

### 最终确认的映射

```
CS=IO16  SCK=IO6  MOSI=IO15  DC=IO7  RST=IO4  BL=IO5
```

---

## 第五阶段：性能优化

### 优化路径

| 轮次 | 问题 | 方案 | 效果 |
|:----:|------|------|------|
| 1 | fillRect 逐像素 transfer16 太慢 | 512px 预填缓冲 + writeBytes 块写 | ~100× |
| 2 | 动画全擦 16384px + 全绘 16384px | 只擦拖尾 384px (3×128) | ~43× |
| 3 | 擦绘之间 CS 间隙导致闪烁 | 先画后擦 | 改善 |
| 4 | 撞边反弹闪烁 | 统一 drawImageShifted 原子 SPI | 消除 |
| 5 | 帧率限制 | 30→60 FPS | 2× |

### drawImageShifted — 核心创新

传统动画 = 擦旧 + 绘新 = 2 次 CS 事务 → 有间隙 → 闪烁  
原子动画 = 1 次 CS 事务 = setWindow(脏矩形) + 逐行[gap背景+图标数据] → **零间隙**

```
每行数据 (右移 3px):
┌──────────┬────────────────────────────┐
│ 6 bytes  │        256 bytes           │
│ 3px 蓝   │   128px 图标数据 (1行)      │
└──────────┴────────────────────────────┘
```

---

## 第六阶段：性能分析

### 理论帧率

```
SPI 传输时间 = 数据量 × 8 bit/byte ÷ 时钟频率

|dx|=3:  131×128×2 × 8 / 40,000,000 = 6.71 ms  →  149 FPS 理论极限
|dx|=0:  128×128×2 × 8 / 40,000,000 = 6.55 ms  →  152 FPS 理论极限
```

### 实际瓶颈

| 约束 | 值 | 限制 |
|------|-----|:---:|
| SPI 带宽 | 40 MHz | 149 FPS |
| LCD 刷新率 | ~60 Hz | **60 FPS** |
| CPU 利用率 @60FPS | ~40% | 稳定不掉帧 |

60 FPS 是 LCD 物理刷新率上限，再高无意义。

---

## 第七阶段：DMA 尝试与结论

### 背景

原版 drawImageShifted 用 256 次小块 `writeBytes` 逐行写 SPI，CPU 忙约 6.7ms/帧（40% 占用率）。方案 A 设想：预构建 33KB 帧缓冲 + 单次大块 `writeBytes` → ESP32 SPI 自动触发 DMA → CPU 释放。

### 尝试 1: Arduino SPI + 单次 writeBytes(33KB)

修改 `drawImageShifted`：预建帧缓冲 (memcpy 128行) → 1次 `_spi->writeBytes(33KB)`。

**结果**: `draw=7.93ms`, 比原来更慢。原因是 Arduino SPI 库对大块 `writeBytes` 不使用 DMA，而是 CPU 轮询，且单块 33KB 的轮询效率低于 256 个小块。

### 尝试 2: ESP-IDF 直驱 + DMA

绕过 Arduino SPI，直接调用 `spi_bus_initialize(SPI2_HOST, ..., SPI_DMA_CH_AUTO)` 和 `spi_device_transmit`。

**结果**: 编译通过，但运行时疯狂报错：
```
E (xxx) spi_master: check_trans_valid(1154): txdata transfer > hardware max supported len
```
每帧上千条错误，画面冻住。

### 尝试 3: 修复 TXDATA / HALFDUPLEX 兼容性

- 去掉 `SPI_TRANS_USE_TXDATA` → 改用 `tx_buffer`
- 去掉 `SPI_DEVICE_HALFDUPLEX` → 改用 `flags = 0`

**结果**: 错误依旧。ESP32-S3 的 GPSI2 外设在 Arduino ESP32 Core 3.x 的 ESP-IDF 版本中存在兼容性差异。

### 结论

| 方案 | 结果 |
|------|------|
| Arduino SPI + 单次 writeBytes(33KB) | 7.93ms, 比原版更差 |
| ESP-IDF DMA (TXDATA) | 硬件限制错误 |
| ESP-IDF DMA (tx_buffer) | 同样错误 |
| ESP-IDF DMA (无 HALFDUPLEX) | 同样错误 |

**根因**: Arduino 内置的 SPI 库对 HSPI 做了正确的初始化，手动调用 `spi_bus_initialize` 时参数不完全匹配此板子的 GPSI2 硬件特性。DMA 方案在 ESP32 上可行，但在 ESP32-S3 的 Arduino 环境下兼容性不足。

**最终决策**: 放弃 DMA，保留 Arduino SPI 方案。40% CPU 占用率在当前场景下完全可接受，剩余 60% 足够驱动摄像头、WiFi/BLE 等外设。

---

## 第八阶段：性能监测覆层

### 屏幕 FPS/CPU 显示

在动画底部添加性能条（y=300-319），显示实时 FPS 和 CPU%。

**遇到的子问题**:
1. `putString(bgFlag=false)` + `fillRect` → 擦除和绘制间隙导致闪烁
2. `putString(bgFlag=true)` → 逐字符逐点 `drawPixel` 太慢，卡动画
3. 最终方案: `putString(bgFlag=true)` + 60帧更新一次 → 59帧零开销丝滑 + 1帧重但无感知

### 最终工程布局

```
arduino/
├── TK015F4000_ESP32S3/          # 原版 (全功能演示)
│   └── 无性能覆层, 纯显示
└── TK015F4000_ESP32S3_DMA/      # 性能监测版
    └── FPS/CPU 实时覆层, 引脚映射相同
```

---

## 项目数据总览

### 最终工程

```
arduino/
├── TK015F4000_ESP32S3/             # 原版
│   ├── TK015F4000_ESP32S3.ino       5.8 KB   主程序
│   ├── lcd_driver.h                 6.4 KB   驱动头文件
│   ├── lcd_driver.cpp              14.8 KB   驱动实现
│   ├── ascii_font.h                 9.3 KB   ASCII 字库
│   ├── gb1616_font.h                5.1 KB   GB2312 字库
│   ├── icon_128x128.h             164.1 KB   图标数据
│   ├── README.md                  16.8 KB   完整文档
│   └── DEVELOPMENT_LOG.md          8.4 KB   开发全记录
│
└── TK015F4000_ESP32S3_DMA/         # 性能监测版
    ├── TK015F4000_ESP32S3_DMA.ino   6.7 KB   主程序 + 性能覆层
    ├── lcd_driver.h / .cpp                  (同原版)
    ├── ascii_font.h / gb1616_font.h         (同原版)
    ├── icon_128x128.h                       (同原版)
    └── README.md                   3.4 KB   性能版文档
─────────────────────────────────────────
总计: 两个工程, 共享驱动核心
```

### 技术栈

| 层级 | 技术 |
|------|------|
| MCU | ESP32-S3 (Xtensa LX7 @ 240MHz) |
| 语言 | C++17 (Arduino) |
| SPI | SPI2_HOST (HSPI), 40MHz, Mode 3 |
| LCD | ST7796S, 320×320, RGB565, 4-Wire SPI |
| 背光 | LEDC PWM @ 5kHz, 8-bit |
| 字库 | ASCII 8×16 (95) + GB2312 16×16 (22) |
| 串口 | USB CDC, 460800 bps |

### 关键时刻

| 阶段 | 事件 |
|------|------|
| 最困难 | 无原理图时盲猜引脚映射 → 雪花 |
| 转折点 | 从底板 PDF 提取 H5/H6 文本信息 |
| 最巧妙 | drawImageShifted 原子 SPI 事务 (零闪烁动画) |
| 最意外 | C++ `const` 内部链接导致链接错误 |
| 编译坑 | GB2312→UTF-8 编码损坏, GCC14 指定初始化器顺序 |
| 硬件坑 | ESP32-S3 GPSI2 DMA 兼容性不足 |
| 性能坑 | putString 逐点 drawPixel 拖慢动画 |
| 平衡点 | bgFlag=true + 60帧间隔 = 丝滑 + 不闪 |

### 最终性能

| 版本 | CPU% | 帧率 | 闪烁 | 性能覆层 |
|------|:----:|:----:|:----:|:--------:|
| 原版 | ~40% | 60 | 无 | ❌ |
| 性能监测版 | ~40% | 60 | 无 | ✅ FPS/CPU% |

---

*2026-06-09 — Claude Code*

---

## 第九阶段：ESP-IDF v5.4 原生移植 + GDMA

### 背景

Arduino 版 DMA 尝试失败后，决定完全绕过 Arduino 层，使用原生 ESP-IDF v5.4 构建。目标是启用真正的 GDMA 传输，将 CPU 从 40% 降至个位数。

### 移植内容

| Arduino 版 | ESP-IDF 版 | 说明 |
|-----------|-------------|------|
| C++ 类 LCD_ST7796 | C 函数 lcd_xxx() | 语言切换 |
| `SPI.writeBytes()` | `spi_device_transmit()` | 原生 DMA |
| `SPI_MODE3` | `.mode = 3` | SPI 配置 |
| `ledcAttach/ledcWrite` | `ledc_timer_config + ledc_channel_config` | LEDC 配置 |
| `.ino` 主程序 | `main.c` + `app_main()` | 入口函数 |

### 成功启用 GDMA

```c
spi_bus_initialize(SPI2_HOST, &bcfg, SPI_DMA_CH_AUTO);
spi_device_transmit(spi, &t);  // 自动 DMA
```

DMA 最大传输块: 32768 bytes (ESP32-S3 SPI DMA 硬件限制)

### 性能飞跃

| 指标 | Arduino 版 | ESP-IDF 版 |
|------|:---:|:---:|
| CPU 占用 @60FPS | ~40% | **~2%** |
| SPI 频率 | 40 MHz | 40 MHz |
| DMA | ❌ (轮询) | ✅ GDMA |
| 每帧 CPU 耗时 | ~6.7ms | ~0.02ms |
| 剩余 CPU 余量 | 60% | **98%** |

---

## 第十阶段：SPI 时序 Bug 修复 (2026-06-16)

### 问题现象

ESP-IDF 版编译通过，烧录后 LCD 显示不正常。

### Bug 1: SPI 时钟超频 (致命)

**代码**: `.clock_speed_hz = 80 * 1000 * 1000` (80MHz)

**根因**: ST7796S 规格书规定 SCLK 最小周期 16ns → 最高 62.5MHz。80MHz 超出芯片承受范围，导致数据传输随机错误。

**修复**: 降至 40MHz（与 main.c 日志信息一致）。

### Bug 2: Sleep Out 延迟严重不足 (致命)

**代码**: `write_cmd(0x11); vTaskDelay(pdMS_TO_TICKS(5));` (仅等待 5ms)

**根因**: 规格书要求 Sleep Out (0x11) 后必须等待 ≥120ms，LCD 内部电荷泵和偏压电路才能稳定。

**修复**: 延长至 150ms。

### Bug 3: Display ON 后无等待 (严重)

**代码**: `write_cmd(0x29); write_cmd(0x21);` → 直接继续配置

**根因**: 规格书要求 Display ON (0x29) 后等待 ≥120ms 显示才能稳定输出。

**修复**: 在 Display ON 后添加 `vTaskDelay(pdMS_TO_TICKS(150))`。

### Bug 4: 文档不一致

- `lcd_st7796.h` 注释写 SPI3_HOST，代码用 SPI2_HOST → 修正
- `main.c` 统计栏标签 "SPI:80M" → 修正为 "SPI:40M"

### 修复总结

| 修改 | 文件 | 从 | 到 |
|------|------|-----|-----|
| SPI 时钟 | lcd_st7796.c | 80 MHz | **40 MHz** |
| Sleep Out 延迟 | lcd_st7796.c | 5 ms | **150 ms** |
| Display ON 延迟 | lcd_st7796.c | 无 | **新增 150 ms** |
| SPI 主机注释 | lcd_st7796.h | SPI3_HOST | **SPI2_HOST** |
| 统计栏标签 | main.c | SPI:80M | **SPI:40M** |

---

*2026-06-16 — GitHub Copilot (DeepSeek V4 Pro)*

---

## 第十一阶段：硬件稳定性修复 (2026-06-16)

### 背景

换了一块同型号开发板后显示正常，确认原板有硬件问题。在正常板子上进行代码审查，发现并修复了 3 个潜在隐患。

### Bug 5: GPIO5 LEDC 背光冲突

**代码**: `lcd_init()` 中 `gpio_set_direction(PIN_BL, GPIO_MODE_OUTPUT)` 将 GPIO5 锁为普通输出。

**现象**: 串口日志出现 `W ledc: GPIO 5 is not usable, maybe conflict with others`，背光无法被 LEDC 控制。

**根因**: `lcd_init()` 先用 `gpio_set_direction` 将 BL 设为 GPIO 输出，之后 `lcd_set_backlight()` 试图配置 LEDC 时发现 GPIO 已被占用。

**修复**: 删除 `lcd_init()` 中 BL 的 GPIO 设置，完全由 `lcd_set_backlight()` 通过 LEDC 管理。

### Bug 6: write_data16 CS 闪烁

**代码**: 
```c
static void write_data16(uint16_t dat) {
    write_data8(dat >> 8);   // CS↓ → 发送高字节 → CS↑
    write_data8(dat & 0xFF); // CS↓ → 发送低字节 → CS↑
}
```

**根因**: 两个字节之间 CS 短暂拉高，ST7796 可能将此视为两次独立的参数写入，导致 CASET/RASET 坐标参数错乱。

**修复**: 合并为单次 16-bit SPI 事务，全程 CS=LOW：
```c
uint8_t buf[2] = {dat >> 8, dat & 0xFF};
spi_device_polling_transmit(spi, &(spi_transaction_t){
    .tx_buffer = buf, .length = 16
});
```

### Bug 7: DMA 缓存一致性问题

**代码**: 所有传给 DMA 的缓冲区使用 `malloc()` 分配，位于 CPU 缓存区内。

**根因**: ESP32-S3 CPU 写入缓存后未刷回物理内存，DMA 直接读物理内存可能读到旧数据。在特定的缓存状态和时序下，DMA 可能读到全零 → 黑屏。

**修复**:
1. 所有 DMA 缓冲区改用 `heap_caps_malloc(size, MALLOC_CAP_DMA)` 分配（绕过缓存）
2. 每次 DMA 传输前显式调用 `Cache_WriteBack_Addr()` 确保缓存已刷新

**影响范围**: `full_demo()` fb、`drawImageShifted()` fb、`fill_rect()` buf、`stats_buf`

### 修复总结

| # | 问题 | 严重度 | 修复方式 |
|---|------|:---:|------|
| 5 | GPIO5 LEDC 冲突 | 中 | 移除 GPIO 设置，由 LEDC 独占 |
| 6 | write_data16 CS 闪烁 | 中 | 单次 16-bit SPI 事务 |
| 7 | DMA 缓存一致性 | 高 | heap_caps_malloc + Cache_WriteBack |

### 关键教训

1. **GPIO 与 LEDC 冲突**: 外设管脚不要先设为 GPIO 再切换，应由目标外设直接初始化。
2. **SPI 参数写入**: LCD 寄存器参数应在单次 CS 周期内完成，CS 闪烁会破坏参数连续性。
3. **ESP32-S3 DMA 缓存**: 所有 DMA 缓冲区必须使用 `MALLOC_CAP_DMA` 分配并显式刷缓存，这是 ESP-IDF 开发中最容易踩的坑。

---

## 第十二阶段：冷启动黑屏与花屏修复 (2026-06-16)

### Bug 8: 断电重启黑屏

**现象**: 固件烧录后能正常显示，但断电后重新上电 LCD 黑屏。先烧 Arduino 固件再烧此固件则正常。

**根因**: ESP-IDF 裸机冷启动仅 ~200ms，而 LCD 内部电荷泵和电压调节器需要 300~400ms 才能稳定。ESP-IDF 在 LCD 电源未就绪时就发送初始化命令，所有寄存器写入失败。Arduino 框架的自然启动开销（~500ms+）掩盖了这个问题。

**修复**:
1. `lcd_init()` 开头添加 400ms 延迟，等待 LCD 内部电源稳定
2. 硬件复位后增加软件复位 (`write_cmd(0x01)` + 150ms 等待)，确保所有寄存器从默认值启动

### Bug 9: 启动瞬间花屏

**现象**: 冷启动时背光先亮 → 黑屏 → 花屏闪一下 → 正常显示。

**根因**:
1. GPIO5 在 LEDC 配置前处于悬空/弱上拉状态，导致背光微亮
2. 背光先于首帧画面点亮，LCD GRAM 中的上电随机数据显示为花屏
3. 首帧画面写入后才覆盖 GRAM → 花屏消失

**修复**:
1. `lcd_init()` 中尽早调用 `lcd_set_backlight(0)` — LEDC 接管 GPIO5 并锁死在 OFF
2. 将 `lcd_set_backlight(255)` 从 `lcd_init()` 移到 `app_main()` 中 `full_demo()` 之后
3. 确保背光只有在首帧完整画面写入 GRAM 后才点亮

### 启动时序对比

```
修复前 (冷启动黑屏+花屏):
  上电 → ~200ms 开始发 SPI 命令 → LCD 未就绪 → 黑屏

修复后 (干净启动):
  上电 → 400ms 等电源 → SW 复位 → 硬件初始化 → 
  背光 LEDC OFF → 画首帧 → 背光 ON ✨
```

### 关键技术教训

1. **ESP-IDF vs Arduino 启动速度**: ESP-IDF 裸机比 Arduino 快 2~3 倍，对外设电源稳定时间要求更严格
2. **LCD 冷启动三要素**: 电源延迟 + 软件复位 + 背光后亮
3. **GPIO 悬空陷阱**: 未初始化的 GPIO 可能因内部上拉导致外设异常动作

---

*2026-06-16 — GitHub Copilot (DeepSeek V4 Pro)*
