可以，把这几个子板整理成一份 **“Agent 可读取的硬件知识库 + 编程规则”**。核心思路是：**不要只把 PDF 丢给 Agent，让它自己猜；而是把子板资料整理成结构化知识，让 Agent 明确知道：这块板是什么、怎么通信、地址怎么选、初始化怎么做、读写哪些寄存器、什么时候需要扫描确认。**

下面这份可以直接作为你项目里的：

```text
docs/AGENT_BOARD_KNOWLEDGE.md
```

---

# 小米 AIoT 实训箱子板 Agent 知识整理

## 0. Agent 总体规则

Agent 在控制子板前，必须知道四类信息：

| 类型   | Agent 需要知道什么                                         |
| ---- | ---------------------------------------------------- |
| 子板身份 | 例如 E1 是 LED+数码管，E2 是风扇，S2 是光照，S8 是温湿度，C3 是 Wi-Fi/BLE |
| 通信方式 | E1/E2/S2/S8 使用 I2C；C3 使用 UART 串口 AT 指令               |
| 地址配置 | 由拨码开关决定 I2C 地址；但资料没有明确 ON/OFF 与 0/1 的物理对应关系          |
| 编程方法 | 初始化命令、寄存器地址、读写流程、数据换算公式                              |

Agent 不应该直接“猜地址”。正确流程是：

```text
1. 读取子板知识库
2. 根据子板类型确定通信方式
3. 如果是 I2C 子板，先根据拨码推测可能地址
4. 运行 I2C 扫描确认实际地址
5. 初始化芯片
6. 执行读传感器或控制执行器操作
7. 记录当前硬件状态
```

尤其注意：资料里 E1、E2、S2、S8 给出的地址多为 **8 位 I2C 地址**。如果使用 Linux `smbus`、Arduino Wire、很多 HAL 库，通常需要使用 **7 位地址**，即：

```text
7位地址 = 8位地址 >> 1
```

---

# 1. E1 子板：LED + 四位数码管

## 1.1 子板功能

E1 是执行器子板，主要功能有两个：

1. RGB 三色灯显示；
2. 四位数码管显示。

资料说明 E1 使用 **PCA9685** 驱动 RGB 三色灯，使用 **HT16K33** 驱动 4 位共阴极红色数码管，输入电压 5V，板上转换为 3.3V。

## 1.2 通信方式

```text
通信协议：I2C
拨码开关数量：2 个
芯片：
- PCA9685：控制 RGB 灯
- HT16K33：控制四位数码管
```

## 1.3 地址关系

E1 有两个 I2C 设备，所以同一块子板会占用两个地址：

| 拨码状态编码 | RGB 灯 PCA9685 8位地址 | RGB 灯 7位地址 | 数码管 HT16K33 8位地址 | 数码管 7位地址 |
| ------ | -----------------: | ---------: | ---------------: | -------: |
| 00     |               0xC0 |       0x60 |             0xE0 |     0x70 |
| 01     |               0xC2 |       0x61 |             0xE2 |     0x71 |
| 10     |               0xC4 |       0x62 |             0xE4 |     0x72 |
| 11     |               0xC6 |       0x63 |             0xE6 |     0x73 |

Agent 规则：

```text
如果用户说“E1 拨码为 00”，Agent 应优先选择：
- RGB 地址：0x60，若库使用 8 位地址则用 0xC0
- 数码管地址：0x70，若库使用 8 位地址则用 0xE0

如果用户没有说明拨码状态，Agent 应先执行 I2C 扫描，在 0x60~0x63 和 0x70~0x73 中匹配。
```

## 1.4 RGB 灯编程方法

RGB 灯由 PCA9685 控制。资料给出相关寄存器：`MODE1 = 0x00`，`LED0_ON/LED0_OFF = 0x06~0x09`，`LED1_ON/LED1_OFF = 0x0A~0x0D`，`LED2_ON/LED2_OFF = 0x0E~0x11`，`ALL_LED_ON/ALL_LED_OFF = 0xFA~0xFD`。初始化时向 `0x00` 写入 `0x00`，再通过 LED 寄存器写不同值控制颜色。

### RGB 控制抽象

Agent 不应该直接让用户操作底层寄存器，而应该暴露高级接口：

```text
set_rgb(red, green, blue)
```

参数范围：

```text
red:   0~100，表示红色亮度百分比
green: 0~100，表示绿色亮度百分比
blue:  0~100，表示蓝色亮度百分比
```

PWM 计算规则：

```text
PCA9685 一个周期为 4096
占空比 = 亮度百分比 / 100
LEDn_ON = 0
LEDn_OFF = int(4095 * 占空比)
```

例如：

```text
红色 100%：LED0_OFF ≈ 4095
绿色 50%： LED1_OFF ≈ 2048
蓝色 0%：  LED2_OFF = 0
```

### Agent 操作流程

```text
1. 确认 E1 RGB 地址
2. 向 MODE1 寄存器 0x00 写入 0x00
3. 将 ALL_LED_ON/ALL_LED_OFF 相关寄存器清零
4. 根据 red/green/blue 计算 PWM 占空比
5. 分别写入 LED0、LED1、LED2 对应寄存器
```

---

## 1.5 数码管编程方法

E1 的数码管由 HT16K33 控制。资料中给出数码管寄存器 `COM1~COM4` 分别对应 `0x02~0x09`，初始化命令包括 `0x21`、`0xA0` 和显示开启命令 `0x81`。

### 四位数码管寄存器

| 数码管位       | 寄存器范围     |
| ---------- | --------- |
| 第 1 位 COM1 | 0x02~0x03 |
| 第 2 位 COM2 | 0x04~0x05 |
| 第 3 位 COM3 | 0x06~0x07 |
| 第 4 位 COM4 | 0x08~0x09 |

### 数字显示映射

| 显示值 |  低字节 |  高字节 |
| --: | ---: | ---: |
|   0 | 0xF8 | 0x01 |
|   1 | 0x30 | 0x00 |
|   2 | 0xD8 | 0x02 |
|   3 | 0x78 | 0x02 |
|   4 | 0x30 | 0x03 |
|   5 | 0x68 | 0x03 |
|   6 | 0xE8 | 0x03 |
|   7 | 0x38 | 0x00 |
|   8 | 0xF8 | 0x03 |
|   9 | 0x78 | 0x03 |

### Agent 操作流程

```text
1. 确认 E1 数码管地址
2. 发送 0x21，开启系统振荡
3. 发送 0xA0，设置 ROW/INT 输出
4. 将 0x02~0x09 写入 0x00，清空显示
5. 发送 0x81，打开显示
6. 根据要显示的数字，查询数字映射表
7. 写入对应 COM 寄存器
8. 再次发送 0x81 刷新显示
```

---

# 2. E2 子板：风扇子板

## 2.1 子板功能

E2 是风扇执行器子板，使用 **PCA9685** 产生 PWM 信号驱动微型直流电机 RC300C，通过调节占空比控制风扇转速。资料说明该子板可以通过 I2C 控制 PWM 频率和占空比。

## 2.2 通信方式

```text
通信协议：I2C
拨码开关数量：2 个
驱动芯片：PCA9685
功能：PWM 控制风扇转速
```

## 2.3 地址关系

| 拨码状态编码 | E2 风扇 8位地址 | E2 风扇 7位地址 |
| ------ | ---------: | ---------: |
| 00     |       0xC8 |       0x64 |
| 01     |       0xCA |       0x65 |
| 10     |       0xCC |       0x66 |
| 11     |       0xCE |       0x67 |

资料中写作 `0xC8, 0x0xCA, 0xCC, 0xCE`，其中 `0x0xCA` 应视为排版错误，实际应为 `0xCA`。

## 2.4 风扇控制寄存器

资料给出的寄存器包括：

| 寄存器       | 作用                       |
| --------- | ------------------------ |
| 0x00      | MODE1                    |
| 0x06~0x09 | LED0_ON / LED0_OFF       |
| 0xFA~0xFD | ALL_LED_ON / ALL_LED_OFF |

虽然名字叫 `LED0`，但在 E2 中它实际对应风扇 PWM 输出。

## 2.5 Agent 控制抽象

Agent 应暴露高级接口：

```text
set_fan_speed(percent)
```

参数：

```text
percent: 0~100
```

含义：

| percent | 含义   |
| ------: | ---- |
|       0 | 关闭风扇 |
|      30 | 低速   |
|      60 | 中速   |
|     100 | 高速   |

PWM 计算：

```text
PWM周期 = 4096
LED0_ON = 0
LED0_OFF = int(4095 * percent / 100)
```

## 2.6 Agent 操作流程

```text
1. 确认 E2 地址
2. 向 MODE1 寄存器 0x00 写入 0x00
3. 向 0xFA~0xFD 写入 0x00，关闭全部输出
4. 根据 percent 计算 PWM 占空比
5. 写入 0x06~0x09，控制风扇转速
```

---

# 3. S2 子板：光照传感器

## 3.1 子板功能

S2 是光照传感器子板，集成光照传感器，可检测 `1~65535 lx` 范围内的亮度，并通过 I2C 接口把光照数据传给 U 板。资料中说明 S2 使用 BH1750 光照传感器，并有一个地址拨码开关。

## 3.2 通信方式

```text
通信协议：I2C
拨码开关数量：1 个
传感器：BH1750
数据单位：lx
```

## 3.3 地址关系

| 拨码状态编码 | S2 8位地址 | S2 7位地址 |
| ------ | ------: | ------: |
| 0      |    0x46 |    0x23 |
| 1      |    0xB8 |    0x5C |

Agent 规则：

```text
如果不确定拨码状态，Agent 应扫描 0x23 和 0x5C。
```

## 3.4 指令表

资料中给出了 BH1750 的主要命令。

|   命令 | 功能        |
| ---: | --------- |
| 0x00 | 断电        |
| 0x01 | 上电        |
| 0x07 | 复位        |
| 0x10 | 连续高精度模式   |
| 0x11 | 连续高精度模式 2 |
| 0x13 | 连续低精度模式   |
| 0x20 | 单次高精度模式   |
| 0x21 | 单次高精度模式 2 |
| 0x23 | 单次低精度模式   |

## 3.5 光照读取流程

推荐 Agent 使用连续高精度模式：

```text
1. 确认 S2 地址
2. 写入 0x01，上电
3. 写入 0x10，进入连续高精度模式
4. 等待约 120~180 ms
5. 读取 2 个字节
6. raw = high_byte << 8 | low_byte
7. lux = raw / 1.2
```

## 3.6 Agent 控制抽象

```text
read_light_lux()
```

返回：

```text
{
  "lux": 1234.5,
  "level": "bright" | "normal" | "dark"
}
```

建议 Agent 内置一个简化判断规则：

|               光照值 | 状态        |
| ----------------: | --------- |
|         lux < 100 | dark，偏暗   |
| 100 <= lux < 1000 | normal，正常 |
|       lux >= 1000 | bright，偏亮 |

实际阈值可以根据实验环境修改。

---

# 4. S8 子板：温湿度传感器

## 4.1 子板功能

S8 是温湿度传感器子板，集成 **SHT35-DIS-B** 温湿度传感器，可采集温度和湿度数据。资料说明温度测量范围为 `-40℃~125℃`，湿度测量范围为 `0%RH~100%RH`。

## 4.2 通信方式

```text
通信协议：I2C
拨码开关数量：1 个
传感器：SHT35-DIS-B
数据：温度、湿度
```

## 4.3 地址关系

| 拨码状态编码 | S8 8位地址 | S8 7位地址 |
| ------ | ------: | ------: |
| 0      |    0x88 |    0x44 |
| 1      |    0x8A |    0x45 |

## 4.4 指令表

|     指令 | 功能        |
| -----: | --------- |
| 0x30A2 | 软复位 SHT35 |
| 0x2C0D | 连续读模式     |

资料中说明初始化时向传感器地址发送复位指令，等待 200ms；读数据时写入连续读模式命令，等待 100ms，然后读取 6 个字节。

## 4.5 数据格式

S8 一次读取 6 个字节：

|  字节序号 | 含义     |
| ----: | ------ |
| byte0 | 温度高字节  |
| byte1 | 温度低字节  |
| byte2 | 温度 CRC |
| byte3 | 湿度高字节  |
| byte4 | 湿度低字节  |
| byte5 | 湿度 CRC |

## 4.6 计算公式

资料给出的计算方法如下：

```text
温度值 = (温度高字节 << 8) + 温度低字节
温度实际值 = 温度值 * 175 / 65535 - 45

湿度值 = (湿度高字节 << 8) + 湿度低字节
湿度实际值 = 湿度值 * 100 / 65535
```

## 4.7 Agent 控制抽象

```text
read_temperature_humidity()
```

返回：

```text
{
  "temperature_c": 26.5,
  "humidity_rh": 52.3
}
```

建议 Agent 内置简化判断规则：

|      温度 | 状态          |
| ------: | ----------- |
|   < 18℃ | cold        |
| 18℃~28℃ | comfortable |
|   > 28℃ | hot         |

---

# 5. C3 子板：Wi-Fi + BLE 子板

## 5.1 子板功能

C3 是通用 Wi-Fi + BLE 子板，集成 **ESP-WROOM-32-N4 / ESP32** 模组，可通过 Wi-Fi 和蓝牙实现远程控制与通信。资料中的功能框图显示 C3 通过 **UART** 与外部接口通信，不是 I2C 子板。

## 5.2 通信方式

```text
通信协议：UART 串口
控制方式：AT 指令
不是 I2C 子板
不需要 I2C 地址
```

Agent 必须牢记：

```text
C3 不能使用 I2C 扫描。
C3 应使用串口发送 AT 指令。
```

## 5.3 C3 板上资源

资料中 C3 有：

| 编号 | 资源        |
| -: | --------- |
|  1 | 串口波特率复位按键 |
|  2 | 电源指示灯     |
|  3 | 复位按键      |
|  4 | 拨码开关 1    |
|  5 | 拨码开关 2    |

资料没有详细说明两个拨码开关的功能定义，因此 Agent 不应擅自假设。需要结合 U1 底板说明或实验指导手册确认。

## 5.4 Wi-Fi 连接流程

Agent 可使用如下 AT 流程：

```text
AT+CWMODE=1
AT+CWJAP="WiFi名称","WiFi密码"
AT+CIPMUX=1
AT+CIPSTART=0,"TCP","服务器IP",端口
AT+CIPSEND=0,数据长度
发送数据
```

资料中给出了连接 Wi-Fi、连接 TCP 服务器并发送数据的 AT 示例。

## 5.5 蓝牙 SPP 流程

资料中给出了蓝牙初始化和透传示例：

```text
AT+BTINIT=1
AT+BTSPPINIT=2
AT+BTNAME="ESP32_SPP"
AT+BTSCANMODE=2
AT+BTSPPSTART
AT+BTSPPSEND
```

Agent 规则：

```text
如果用户需求是“连接手机蓝牙”，走蓝牙 SPP 流程。
如果用户需求是“联网发送数据到服务器”，走 Wi-Fi + TCP 流程。
```

---

# 6. 推荐给 Agent 的板级配置文件

可以在项目中建立：

```text
config/boards.yaml
```

内容示例：

```yaml
i2c_address_mode:
  note: "PDF 中地址多为 8 位地址；Linux smbus / Arduino Wire 通常使用 7 位地址"
  convert_rule: "addr_7bit = addr_8bit >> 1"

boards:
  E1:
    name: "LED + 4位数码管子板"
    protocol: "i2c"
    dips: 2
    devices:
      rgb:
        chip: "PCA9685"
        addresses_8bit: ["0xC0", "0xC2", "0xC4", "0xC6"]
        addresses_7bit: ["0x60", "0x61", "0x62", "0x63"]
        registers:
          MODE1: "0x00"
          LED0_ON_OFF: "0x06-0x09"
          LED1_ON_OFF: "0x0A-0x0D"
          LED2_ON_OFF: "0x0E-0x11"
          ALL_LED_ON_OFF: "0xFA-0xFD"
        actions:
          - "init_rgb"
          - "set_rgb"
      display:
        chip: "HT16K33"
        addresses_8bit: ["0xE0", "0xE2", "0xE4", "0xE6"]
        addresses_7bit: ["0x70", "0x71", "0x72", "0x73"]
        init_commands: ["0x21", "0xA0", "0x81"]
        com_registers:
          COM1: ["0x02", "0x03"]
          COM2: ["0x04", "0x05"]
          COM3: ["0x06", "0x07"]
          COM4: ["0x08", "0x09"]
        actions:
          - "display_number"
          - "clear_display"

  E2:
    name: "风扇子板"
    protocol: "i2c"
    dips: 2
    devices:
      fan:
        chip: "PCA9685"
        addresses_8bit: ["0xC8", "0xCA", "0xCC", "0xCE"]
        addresses_7bit: ["0x64", "0x65", "0x66", "0x67"]
        registers:
          MODE1: "0x00"
          FAN_PWM: "0x06-0x09"
          ALL_LED_ON_OFF: "0xFA-0xFD"
        actions:
          - "set_fan_speed"
          - "fan_off"

  S2:
    name: "光照传感器子板"
    protocol: "i2c"
    dips: 1
    devices:
      light:
        chip: "BH1750"
        addresses_8bit: ["0x46", "0xB8"]
        addresses_7bit: ["0x23", "0x5C"]
        commands:
          power_down: "0x00"
          power_on: "0x01"
          reset: "0x07"
          continuous_high_resolution: "0x10"
          one_time_high_resolution: "0x20"
        data_formula: "lux = raw / 1.2"
        actions:
          - "read_light_lux"

  S8:
    name: "温湿度传感器子板"
    protocol: "i2c"
    dips: 1
    devices:
      temperature_humidity:
        chip: "SHT35-DIS-B"
        addresses_8bit: ["0x88", "0x8A"]
        addresses_7bit: ["0x44", "0x45"]
        commands:
          soft_reset: "0x30A2"
          read_mode: "0x2C0D"
        data_length: 6
        temperature_formula: "temperature = raw_temp * 175 / 65535 - 45"
        humidity_formula: "humidity = raw_hum * 100 / 65535"
        actions:
          - "read_temperature_humidity"

  C3:
    name: "Wi-Fi + BLE 子板"
    protocol: "uart"
    chip: "ESP-WROOM-32-N4 / ESP32"
    note: "C3 不是 I2C 子板，不使用 I2C 地址"
    actions:
      - "send_at"
      - "connect_wifi"
      - "connect_tcp"
      - "send_tcp_data"
      - "init_bluetooth_spp"
```

---

# 7. Agent 应该拥有的工具函数

Agent 最好不要直接生成一堆寄存器代码，而是调用你封装好的工具函数。

## 7.1 底层工具

```text
i2c_scan(bus)
i2c_write_byte(addr, data)
i2c_write_register(addr, reg, data)
i2c_write_block(addr, reg, data[])
i2c_read(addr, length)
uart_send(command)
uart_read_until(expected, timeout)
```

## 7.2 板级工具

```text
e1_init_rgb()
e1_set_rgb(red, green, blue)
e1_init_display()
e1_display_number(value)

e2_init_fan()
e2_set_fan_speed(percent)

s2_read_lux()

s8_read_temperature_humidity()

c3_connect_wifi(ssid, password)
c3_connect_tcp(host, port)
c3_send_tcp_data(data)
c3_init_bluetooth()
```

## 7.3 智能家居任务级工具

结合你的项目，可以再封装成：

```text
auto_light_control()
auto_fan_control()
manual_light_on()
manual_light_off()
manual_fan_on()
manual_fan_off()
energy_saving_mode()
```

这样 Agent 处理自然语言时，就不需要每次都直接改寄存器，而是把意图映射为高级动作。

例如：

```text
用户说：“现在有点暗，帮我开灯”

Agent 推理：
1. 调用 s2_read_lux()
2. 如果 lux < 阈值，调用 e1_set_rgb(100, 100, 100)
3. 调用 e1_display_number(lux)
4. 返回：“当前光照较低，已打开灯光。”
```

---

# 8. Agent 系统提示词示例

可以把下面这段作为硬件 Agent 的系统提示词或项目说明：

```text
你是一个小米 AIoT 实训箱硬件编程助手，负责根据用户需求生成子板配置方案、控制逻辑和代码。

你必须遵守以下规则：

1. E1、E2、S2、S8 是 I2C 子板；C3 是 UART 串口 AT 指令子板。
2. PDF 中给出的 I2C 地址多为 8 位地址；如果使用 Linux smbus、Arduino Wire 或常见 7 位 I2C API，需要将地址右移一位。
3. 不允许凭空猜测拨码开关 ON/OFF 与 0/1 的物理对应关系。若用户没有明确说明，必须建议先运行 I2C 扫描。
4. E1 包含两个 I2C 设备：
   - PCA9685 控制 RGB 灯；
   - HT16K33 控制四位数码管。
5. E2 使用 PCA9685 控制风扇 PWM。
6. S2 使用 BH1750 读取光照，推荐连续高精度模式 0x10，读取 2 字节后 lux = raw / 1.2。
7. S8 使用 SHT35-DIS-B 读取温湿度，先发送 0x30A2 软复位，再发送 0x2C0D 读模式，读取 6 字节后计算温湿度。
8. C3 使用 ESP32 AT 指令进行 Wi-Fi、TCP 和蓝牙通信，不参与 I2C 扫描。
9. 生成代码前必须明确：
   - 使用的开发板或主控；
   - I2C 总线编号；
   - 串口编号；
   - 使用的 SDK 或 HAL 库；
   - 子板拨码状态；
   - 是否需要 7 位地址还是 8 位地址。
10. 若资料不足，应明确指出缺失信息，不得编造。
```

---

# 9. Agent 面向用户的配置流程

用户接入子板时，Agent 应引导用户按这个流程：

```text
第一步：确认使用哪些子板
例如：E1、E2、S2、S8、C3

第二步：确认拨码开关
例如：
E1 = 00
E2 = 01
S2 = 0
S8 = 0

第三步：确认通信接口
I2C 子板：
- 使用哪一路 I2C？
- SDA/SCL 是否接好？
- 电源是否正常？

C3 子板：
- 使用哪个串口？
- 波特率是多少？
- 是否能返回 AT OK？

第四步：运行扫描或测试
I2C：
- 扫描 0x03~0x77
- 检查是否出现目标地址

UART：
- 发送 AT
- 检查是否返回 OK

第五步：生成初始化代码

第六步：生成具体功能代码
例如：
- 读取光照
- 显示光照值
- 温度高时打开风扇
- 光照低时打开 LED
```

---

# 10. 针对你智能家居项目的 Agent 行为设计

你的项目中可以让 Agent 按如下规则工作：

## 自动模式

```text
1. S2 读取光照 lux
2. S8 读取温度 temperature
3. 如果 lux < 光照阈值：
      E1 打开 LED
   否则：
      E1 关闭 LED
4. 如果 temperature > 温度阈值：
      E2 打开风扇
   否则：
      E2 关闭风扇
5. E1 数码管显示：
      前两位显示温度
      后两位显示光照等级或简化光照值
```

## 手动模式

```text
按键 1：切换自动 / 手动模式
按键 2：手动控制 LED 开关
按键 3：手动控制风扇开关
```

## 预留 LLM 自然语言控制

```text
“现在有点暗，帮我开灯”
=> 调用 e1_set_rgb()

“温度太高，打开风扇”
=> 调用 e2_set_fan_speed()

“切换为节能模式”
=> 降低 LED 亮度，调低风扇占空比，增大自动控制阈值
```

---
