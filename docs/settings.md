# 智能家居项目当前配置

本文档同步当前仓库代码实现，重点记录已经生效的硬件连接、控制参数和 C3 联网逻辑。

## 1. 系统拓扑

当前系统由 U1 主控统一调度：

- `S2`：光照传感器，I2C
- `S8`：温湿度传感器，I2C
- `E1`：RGB LED + 数码管，I2C
- `E2`：风扇驱动，I2C
- `S1`：按键输入，I2C
- `C3`：ESP32 Wi-Fi / BLE 子板，UART AT

当前不是“只做本地控制再预留 C3”，而是“本地控制 + C3 联网代码已接入主循环”。

## 2. 硬件地址与接口

来自 [board_config.h](/abs/path/d:/xiaomi/Smart_Home/demo/Project/GD32F450Z_BSP/inc/board_config.h)：

```c
#define S2_LIGHT_ADDR        0x46
#define S8_TEMP_HUMI_ADDR    0x88
#define E1_LED_ADDR          0xC0
#define E1_DISPLAY_ADDR      0xE0
#define E2_FAN_ADDR          0xC8
#define S1_KEY_ADDR          0xE8
```

接口说明：

- `S2 / S8 / E1 / E2 / S1` 走 I2C
- `C3` 走 `USART5`
- `USART0` 用于调试日志输出

## 3. 主循环当前行为

当前主循环位于 [main.c](/abs/path/d:/xiaomi/Smart_Home/demo/Project/Application/src/main.c)：

1. 初始化 I2C 子板。
2. 初始化 `USART5` 并调用 `Wifi_Init()`。
3. 每个周期执行：
   - 读取按键
   - 读取传感器
   - 应用自动控制
   - 更新数码管显示
   - 打印调试信息
   - 上报状态到后端
   - 轮询后端命令
   - 执行 Wi-Fi 命令

## 4. 本地控制参数

来自 [smart_home_config.h](/abs/path/d:/xiaomi/Smart_Home/demo/Project/GD32F450Z_BSP/inc/smart_home_config.h)：

```c
#define LIGHT_THRESHOLD      500U
#define TEMP_THRESHOLD       28.0f
#define FAN_ON_DUTY          80U
#define FAN_TEMP_LEVEL_1     24.0f
#define FAN_TEMP_LEVEL_2     26.0f
#define FAN_TEMP_LEVEL_3     28.0f
#define FAN_TEMP_LEVEL_4     30.0f
#define FAN_DUTY_LEVEL_1     30U
#define FAN_DUTY_LEVEL_2     50U
#define FAN_DUTY_LEVEL_3     70U
#define FAN_DUTY_LEVEL_4     90U
#define LED_ON_DUTY          100U
#define SENSOR_UPDATE_MS     200U
```

当前自动控制规则：

- 光照 `< 500 lx` 时自动开灯，否则关灯。
- 温度四档风扇控制：
  - `24°C` -> `30%`
  - `26°C` -> `50%`
  - `28°C` -> `70%`
  - `30°C` -> `90%`

## 5. 按键与显示逻辑

当前按键定义：

- `K1`：自动 / 手动模式切换
- `K2`：手动模式下切换 LED
- `K3`：手动模式下切换风扇

当前显示规则：

- 数码管前两位显示温度整数部分
- 数码管后两位显示 `light / 100`

示例：

- `26°C`、`1200 lx` 显示为 `2612`

## 6. C3 当前实现

当前 C3 固件已经实现：

- `AT` 握手
- 首次启动 `AT+RESTORE`
- 清理旧 Wi-Fi 自动连接配置
- 连接目标热点
- 校验当前 SSID
- 查询 IP
- TCP 建连
- HTTP 上报状态
- HTTP 轮询命令

当前默认联网配置来自 [c3_wifi.c](/abs/path/d:/xiaomi/Smart_Home/demo/Project/GD32F450Z_BSP/src/c3_wifi.c)：

```c
#define C3_WIFI_SSID                    "ozy"
#define C3_WIFI_PASSWORD                "12345678"
#define C3_SERVER_HOST                  "192.168.137.1"
#define C3_SERVER_PORT                  5000U
#define C3_DEVICE_ID                    "smart-home-001"
```

当前支持的云端命令：

- `LED_ON`
- `LED_OFF`
- `FAN_ON`
- `FAN_OFF`
- `AUTO_MODE`
- `MANUAL_MODE`
- `READ_STATUS`

## 7. C3 与后端交互频率

根据当前实现：

- 主循环周期：`200 ms`
- 状态上报：每 5 个周期一次，约 `1 s`
- 命令轮询：每 5 个周期一次，约 `1 s`
- 断线重连：每 25 个周期重试一次，约 `5 s`

## 8. 当前验证状态

必须区分“代码实现”和“实机结果”：

- 已完成：代码、接口、后端联动路径
- 未完成：C3 实机 Wi-Fi 联调验证

因此当前项目状态应表述为：

- `S2 / S8 / E1 / E2 / S1` 本地控制逻辑已落地
- `C3` 联网控制代码已落地
- `C3` 与目标热点 / 电脑后端的实机连接问题仍在排查
