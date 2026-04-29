# 基于 U1 的智能家居项目工程实施计划

## 1. 实施目标

本计划用于指导现有 `demo` 工程从录音与超声波测距示例改造为智能家居环境监测与控制项目。

改造完成后，主业务应围绕以下流程运行：

```text
读取 S1 按键
读取 S2 光照
读取 S8 温湿度
根据自动/手动模式控制 E1 LED 和 E2 风扇
刷新 E1 数码管显示
预留 C3 Wi-Fi/LLM 扩展接口
```

## 2. 当前工程现状

### 2.1 已有能力

当前工程已有以下基础代码：

| 模块 | 路径 | 说明 |
| --- | --- | --- |
| I2C | `demo/Project/GD32F450Z_BSP/src/i2c.c` | 已有 I2C0/I2C1 初始化和读写能力 |
| S1 | `demo/Project/GD32F450Z_BSP/src/s1.c` | 已有 HT16K33 按键读取能力 |
| E1 | `demo/Project/GD32F450Z_BSP/src/e1.c` | 已有 E1 初始化、LED/数码管相关能力 |
| main | `demo/Project/Application/src/main.c` | 当前为旧 demo 主流程 |

### 2.2 需要新增的能力

PRD 要求但当前工程缺失的模块：

| 模块 | 目标文件 | 说明 |
| --- | --- | --- |
| S2 光照 | `s2.c/h` | BH1750 初始化、读 lux |
| S8 温湿度 | `s8.c/h` | SHT35 初始化、读温湿度 |
| E2 风扇 | `e2.c/h` | PCA9685 PWM 控制风扇 |
| C3 预留 | `c3_wifi.c/h` | Wi-Fi/LLM 空桩接口 |
| 地址配置 | `board_config.h` | 统一管理 I2C 地址 |
| 系统配置 | `smart_home_config.h` | 统一管理阈值和周期 |

### 2.3 需要替换的旧业务

当前 `main.c` 中的以下业务与智能家居目标不匹配，应从主流程移除：

- S3 录音相关逻辑。
- S6 超声波测距逻辑。
- `ring_voice.h` 大数组播放逻辑。
- 录音、播放、倒计时触发播放等状态变量。

旧驱动文件可以暂时保留在工程目录中，但不应再由智能家居主流程引用。

## 3. 目标文件结构

建议保持现有工程结构，只在 BSP 和 Application 内增加必要文件。

```text
demo/Project/
├── Application/
│   ├── inc/
│   │   └── main.h
│   └── src/
│       └── main.c
└── GD32F450Z_BSP/
    ├── inc/
    │   ├── board_config.h
    │   ├── smart_home_config.h
    │   ├── s2.h
    │   ├── s8.h
    │   ├── e2.h
    │   └── c3_wifi.h
    └── src/
        ├── s2.c
        ├── s8.c
        ├── e2.c
        └── c3_wifi.c
```

## 4. 配置文件设计

### 4.1 `board_config.h`

职责：统一管理本项目使用的子板地址，避免散落在多个驱动中。

建议内容：

```c
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#define S2_LIGHT_ADDR        0x46
#define S8_TEMP_HUMI_ADDR    0x88
#define E1_LED_ADDR          0xC0
#define E1_DISPLAY_ADDR      0xE0
#define E2_FAN_ADDR          0xC8
#define S1_KEY_ADDR          0xE8

#endif
```

### 4.2 `smart_home_config.h`

职责：统一管理控制阈值、采样周期和执行器输出参数。

建议内容：

```c
#ifndef SMART_HOME_CONFIG_H
#define SMART_HOME_CONFIG_H

#define LIGHT_THRESHOLD      500
#define TEMP_THRESHOLD       28
#define FAN_ON_DUTY          80
#define LED_ON_DUTY          100
#define SENSOR_UPDATE_MS     500

#endif
```

## 5. BSP 模块实施清单

### 5.1 S2 光照驱动

文件：

- `demo/Project/GD32F450Z_BSP/inc/s2.h`
- `demo/Project/GD32F450Z_BSP/src/s2.c`

目标接口：

```c
i2c_addr_def s2_init(uint8_t addr);
uint8_t s2_read_light(uint32_t periph, uint8_t addr, uint16_t *lux);
```

实现要点：

- 使用 BH1750。
- 初始化时发送 power on 命令 `0x01`。
- 可使用连续高精度模式 `0x10`，或单次高精度模式 `0x20`。
- 读取 2 字节原始数据。
- lux 换算可先使用 `raw / 1.2` 的整数近似。
- 读取失败返回 0，成功返回 1。

### 5.2 S8 温湿度驱动

文件：

- `demo/Project/GD32F450Z_BSP/inc/s8.h`
- `demo/Project/GD32F450Z_BSP/src/s8.c`

目标接口：

```c
i2c_addr_def s8_init(uint8_t addr);
uint8_t s8_read_temp_humi(uint32_t periph, uint8_t addr, float *temperature, float *humidity);
```

实现要点：

- 使用 SHT35。
- 初始化时可发送软复位命令 `0x30A2`。
- 测量命令使用 `0x2C0D`。
- 读取 6 字节：温度高低字节、温度 CRC、湿度高低字节、湿度 CRC。
- MVP 阶段 CRC 可作为增强项，先保留接口或注释说明。
- 温度换算：`temperature = raw * 175.0f / 65535.0f - 45.0f`。
- 湿度换算：`humidity = raw * 100.0f / 65535.0f`。

### 5.3 E2 风扇驱动

文件：

- `demo/Project/GD32F450Z_BSP/inc/e2.h`
- `demo/Project/GD32F450Z_BSP/src/e2.c`

目标接口：

```c
i2c_addr_def e2_init(uint8_t addr);
void e2_fan_on(uint32_t periph, uint8_t addr);
void e2_fan_off(uint32_t periph, uint8_t addr);
void e2_fan_set_duty(uint32_t periph, uint8_t addr, uint8_t duty);
```

实现要点：

- E2 使用 PCA9685 PWM。
- 初始化 MODE1 寄存器。
- 优先封装 `e2_fan_set_duty()`，`on/off` 只是固定 duty 的包装。
- duty 输入范围限制在 0 到 100。
- 关闭时输出 0%。
- 打开时默认使用 `FAN_ON_DUTY`。

### 5.4 C3 Wi-Fi 预留接口

文件：

- `demo/Project/GD32F450Z_BSP/inc/c3_wifi.h`
- `demo/Project/GD32F450Z_BSP/src/c3_wifi.c`

目标接口：

```c
void Wifi_Init(void);
void Wifi_SendSensorData(float temperature, float humidity, uint16_t light);
void Wifi_ReceiveCommand(void);
```

实现要点：

- 本阶段函数可以为空实现。
- 需要保证主工程可编译。
- 后续接入 UART AT 指令或自定义串口协议时再扩展。

## 6. 应用层设计

### 6.1 状态结构

在 `main.c` 或 `main.h` 中定义：

```c
typedef enum {
    MODE_AUTO = 0,
    MODE_MANUAL = 1
} SystemMode;

typedef struct {
    float temperature;
    float humidity;
    uint16_t light;
    uint8_t led_on;
    uint8_t fan_on;
    SystemMode mode;
} SmartHomeState;
```

### 6.2 初始化流程

目标顺序：

```text
设置中断优先级
初始化调试串口
初始化 LED GPIO
初始化 I2C0/I2C1
初始化 E1 数码管/LED
初始化 S1 按键
初始化 S2 光照
初始化 S8 温湿度
初始化 E2 风扇
初始化 C3 预留接口
关闭 LED 和风扇
进入主循环
```

### 6.3 主循环流程

建议主循环：

```text
while (1)
{
    读取 S1 按键

    如果 K1 被按下：
        切换自动/手动模式

    读取 S2 光照
    如果读取成功：
        更新 state.light

    读取 S8 温湿度
    如果读取成功：
        更新 state.temperature 和 state.humidity

    如果当前为自动模式：
        根据光照阈值控制 LED
        根据温度阈值控制风扇
    否则：
        如果 K2 被按下，切换 LED 状态
        如果 K3 被按下，切换风扇状态

    刷新 E1 数码管显示
    调用 C3 预留接口
    延时 SENSOR_UPDATE_MS
}
```

### 6.4 按键处理

按键建议采用边沿触发或简单消抖，避免长按时连续切换。

最低要求：

- K1 每次有效按下只切换一次模式。
- K2 每次有效按下只切换一次 LED。
- K3 每次有效按下只切换一次风扇。

若现有 S1 驱动只返回当前键值，可在 `main.c` 中保存上一次键值：

```c
static uint8_t last_key = 0;
uint8_t key = get_key_value(...);
uint8_t key_pressed = (key != 0 && last_key == 0);
last_key = key;
```

### 6.5 显示处理

显示值规则：

```text
display_value = temperature_integer * 100 + light / 100
```

边界：

- 温度显示限制在 0 到 99。
- 光照缩放值限制在 0 到 99。

示例辅助逻辑：

```c
uint8_t temp_display = clamp((int)state.temperature, 0, 99);
uint8_t light_display = clamp(state.light / 100, 0, 99);
uint16_t display_value = temp_display * 100 + light_display;
```

## 7. 工程接入步骤

### 阶段 0：基线整理

目标：明确现有工程可以作为改造基础。

任务：

- 记录当前可用工程文件。
- 确认 `i2c.c/h`、`s1.c/h`、`e1.c/h` 的函数命名和调用方式。
- 明确 `main.c` 旧业务需要替换。

验收：

- 能说明当前工程中哪些文件复用，哪些文件不再作为主流程依赖。

### 阶段 1：配置文件与驱动补齐

目标：补齐智能家居项目所需 BSP 能力。

任务：

- 新增 `board_config.h`。
- 新增 `smart_home_config.h`。
- 新增并实现 `s2.c/h`。
- 新增并实现 `s8.c/h`。
- 新增并实现 `e2.c/h`。
- 新增并实现 `c3_wifi.c/h` 空桩。

验收：

- 新增文件能被工程引用。
- 驱动接口与现有 `i2c_addr_def` 风格一致。
- 编译不出现头文件或符号缺失。

### 阶段 2：主程序改造

目标：将 `main.c` 主流程切换为智能家居状态机。

任务：

- 移除旧业务头文件引用：`s3.h`、`s6.h`、`ring_voice.h`。
- 移除录音、播放、超声波测距相关全局变量和中断业务。
- 保留必要的串口、I2C、定时和 LED 基础能力。
- 定义 `SystemMode` 和 `SmartHomeState`。
- 实现初始化流程。
- 实现主循环控制逻辑。

验收：

- 主循环不再执行录音/测距逻辑。
- 自动/手动模式逻辑清晰可读。
- LED、风扇和显示的状态来自同一个 `SmartHomeState`。

### 阶段 3：联调与容错

目标：在硬件上验证传感器、执行器和显示。

任务：

- 验证 S2 光照读数。
- 验证 S8 温湿度读数。
- 验证 E1 LED 开关。
- 验证 E1 数码管显示。
- 验证 E2 风扇 PWM。
- 验证 S1 按键。
- 增加读取失败保留上次有效值的保护逻辑。

验收：

- 遮光能触发 LED。
- 升温或模拟温度阈值能触发风扇。
- K1/K2/K3 行为符合 PRD。
- I2C 单次读取失败不导致执行器反复抖动。

### 阶段 4：参数调优与文档同步

目标：根据实际硬件表现调整参数并同步文档。

任务：

- 调整 `LIGHT_THRESHOLD`。
- 调整 `TEMP_THRESHOLD`。
- 调整 `FAN_ON_DUTY`。
- 根据实际显示效果调整数码管刷新逻辑。
- 更新 PRD 和 plan 中的最终参数。

验收：

- 参数与实际硬件表现匹配。
- 文档和代码配置一致。

## 8. 验证清单

### 8.1 编译验证

- 工程无 undefined reference。
- 工程无头文件找不到错误。
- 新增 `.c` 文件已加入 Keil/VS Code 工程。

### 8.2 I2C 地址验证

- S2 使用 `0x46`。
- S8 使用 `0x88`。
- E1 LED 使用 `0xC0`。
- E1 数码管使用 `0xE0`。
- E2 使用 `0xC8`。
- S1 使用 `0xE8`。

### 8.3 功能验证

| 验证项 | 操作 | 预期 |
| --- | --- | --- |
| 自动开灯 | 遮挡 S2 | LED 打开 |
| 自动关灯 | 恢复光照 | LED 关闭 |
| 自动开风扇 | 温度达到阈值 | 风扇打开 |
| 自动关风扇 | 温度低于阈值 | 风扇关闭 |
| 模式切换 | 按 K1 | 自动/手动切换 |
| 手动 LED | 手动模式按 K2 | LED 状态切换 |
| 手动风扇 | 手动模式按 K3 | 风扇状态切换 |
| 数码管显示 | 正常运行 | 前两位温度，后两位光照缩放值 |

## 9. 风险与对策

| 风险 | 影响 | 对策 |
| --- | --- | --- |
| I2C 地址与文档不一致 | 设备无响应 | 所有地址集中放入 `board_config.h` |
| SHT35 CRC 未校验 | 偶发异常数据 | MVP 可先保留上次有效值，后续补 CRC |
| 风扇启动占空比不足 | 风扇不转 | 将 `FAN_ON_DUTY` 调整到 60% 到 80% |
| 按键长按重复触发 | 模式来回切换 | 使用边沿触发或消抖 |
| 数码管显示溢出 | 显示异常 | 对温度和光照显示值做 0 到 99 限制 |
| 旧 demo 代码残留 | 主流程混乱或编译冲突 | `main.c` 中移除旧业务引用，旧驱动仅保留文件 |

## 10. 交付物

代码交付：

- `demo/Project/GD32F450Z_BSP/inc/board_config.h`
- `demo/Project/GD32F450Z_BSP/inc/smart_home_config.h`
- `demo/Project/GD32F450Z_BSP/inc/s2.h`
- `demo/Project/GD32F450Z_BSP/src/s2.c`
- `demo/Project/GD32F450Z_BSP/inc/s8.h`
- `demo/Project/GD32F450Z_BSP/src/s8.c`
- `demo/Project/GD32F450Z_BSP/inc/e2.h`
- `demo/Project/GD32F450Z_BSP/src/e2.c`
- `demo/Project/GD32F450Z_BSP/inc/c3_wifi.h`
- `demo/Project/GD32F450Z_BSP/src/c3_wifi.c`
- `demo/Project/Application/src/main.c`

文档交付：

- `docs/prd.md`
- `docs/plan.md`
- 如硬件地址或参数变化，同步更新 `docs/settings.md`

## 11. 后续开发建议

建议先完成“本地闭环控制 MVP”，再扩展联网能力：

1. 先实现 S2/S8/E2 驱动和主循环。
2. 在硬件上完成自动/手动控制验证。
3. 调整阈值和风扇占空比。
4. 再接入 C3 Wi-Fi。
5. 最后接入后端服务和 LLM 自然语言控制。
