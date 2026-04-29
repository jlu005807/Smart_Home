# Smart Home U1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Convert the existing Xiaomi AIoT U1 demo from audio/ultrasonic behavior into a local smart-home monitoring and control system.

**Architecture:** Keep the existing GD32F450Z BSP/Application layout. Add focused BSP drivers for S2/BH1750, S8/SHT35, E2/PCA9685 fan control, and a C3 Wi-Fi placeholder; replace `main.c` with a small state machine that reads sensors, handles S1 keys, controls E1/E2, and refreshes E1 display.

**Tech Stack:** C, GD32F4xx standard peripheral library, Keil/VS Code embedded project, existing `i2c_addr_def` and I2C helper APIs.

---

## Scope Check

This implementation plan covers one deliverable: a working local smart-home MVP. C3 networking, cloud services, LLM API calls, voice recognition, and persistent settings are intentionally outside this plan. The plan includes C3 only as compile-safe empty hooks.

## File Structure

Create:

- `demo/Project/GD32F450Z_BSP/inc/board_config.h`: central I2C address definitions.
- `demo/Project/GD32F450Z_BSP/inc/smart_home_config.h`: thresholds, duty cycle, and sampling period.
- `demo/Project/GD32F450Z_BSP/inc/s2.h`: S2/BH1750 public interface.
- `demo/Project/GD32F450Z_BSP/src/s2.c`: S2/BH1750 init and lux read implementation.
- `demo/Project/GD32F450Z_BSP/inc/s8.h`: S8/SHT35 public interface.
- `demo/Project/GD32F450Z_BSP/src/s8.c`: S8/SHT35 init and temperature/humidity read implementation.
- `demo/Project/GD32F450Z_BSP/inc/e2.h`: E2 fan public interface.
- `demo/Project/GD32F450Z_BSP/src/e2.c`: E2/PCA9685 fan PWM implementation.
- `demo/Project/GD32F450Z_BSP/inc/c3_wifi.h`: C3 placeholder public interface.
- `demo/Project/GD32F450Z_BSP/src/c3_wifi.c`: C3 placeholder implementation.

Modify:

- `demo/Project/Application/src/main.c`: replace old audio/ultrasonic flow with smart-home state machine.
- `demo/Project/Application/inc/main.h`: keep existing hardware helpers; add no smart-home business logic here unless compile errors require prototypes.
- `demo/Project/KEIL_Project/task2.uvprojx`: add new `.c` files to the Keil project if Keil does not auto-discover them.
- `demo/Project/VS_Project/build/task2/compile_commands.json`: generated file; do not edit manually.

Documentation references:

- `docs/prd.md`: product requirements and acceptance criteria.
- `docs/plan.md`: high-level engineering plan.
- `docs/settings.md`: hardware address reference.

## Commit Guidance

This directory is not currently a git repository. When implementing in a git-enabled copy, commit after each task using the commit messages listed below. In this workspace, record completed tasks manually if git is unavailable.

## Task 1: Add Shared Configuration Headers

**Files:**

- Create: `demo/Project/GD32F450Z_BSP/inc/board_config.h`
- Create: `demo/Project/GD32F450Z_BSP/inc/smart_home_config.h`

- [ ] **Step 1: Create `board_config.h`**

Write exactly:

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

- [ ] **Step 2: Create `smart_home_config.h`**

Write exactly:

```c
#ifndef SMART_HOME_CONFIG_H
#define SMART_HOME_CONFIG_H

#define LIGHT_THRESHOLD      500U
#define TEMP_THRESHOLD       28.0f
#define FAN_ON_DUTY          80U
#define LED_ON_DUTY          100U
#define SENSOR_UPDATE_MS     500U

#endif
```

- [ ] **Step 3: Verify files exist**

Run:

```powershell
Test-Path demo\Project\GD32F450Z_BSP\inc\board_config.h
Test-Path demo\Project\GD32F450Z_BSP\inc\smart_home_config.h
```

Expected output:

```text
True
True
```

- [ ] **Step 4: Commit**

Run in a git-enabled workspace:

```bash
git add demo/Project/GD32F450Z_BSP/inc/board_config.h demo/Project/GD32F450Z_BSP/inc/smart_home_config.h
git commit -m "feat: add smart home configuration headers"
```

## Task 2: Add S2 BH1750 Light Driver

**Files:**

- Create: `demo/Project/GD32F450Z_BSP/inc/s2.h`
- Create: `demo/Project/GD32F450Z_BSP/src/s2.c`

- [ ] **Step 1: Create `s2.h`**

Write exactly:

```c
#ifndef S2_H
#define S2_H

#include "i2c.h"

#define BH1750_POWER_DOWN          0x00
#define BH1750_POWER_ON            0x01
#define BH1750_RESET               0x07
#define BH1750_CONT_H_RES_MODE     0x10
#define BH1750_ONE_H_RES_MODE      0x20

i2c_addr_def s2_init(uint8_t address);
uint8_t s2_read_light(uint32_t i2c_periph, uint8_t i2c_addr, uint16_t *lux);

#endif
```

- [ ] **Step 2: Create `s2.c`**

Write exactly:

```c
#include "s2.h"

i2c_addr_def s2_init(uint8_t address)
{
    i2c_addr_def s2_address;

    s2_address = get_board_address(address);
    if (s2_address.flag) {
        i2c_cmd_write(s2_address.periph, s2_address.addr, BH1750_POWER_ON);
        i2c_cmd_write(s2_address.periph, s2_address.addr, BH1750_CONT_H_RES_MODE);
    }

    return s2_address;
}

uint8_t s2_read_light(uint32_t i2c_periph, uint8_t i2c_addr, uint16_t *lux)
{
    uint8_t buffer[2] = {0};
    uint32_t raw;
    uint32_t value;

    if (lux == 0) {
        return 0;
    }

    i2c_direct_read(i2c_periph, i2c_addr, buffer, 2);
    raw = ((uint32_t)buffer[0] << 8) | buffer[1];

    if (raw == 0xFFFFU) {
        return 0;
    }

    value = (raw * 10U) / 12U;
    if (value > 65535U) {
        value = 65535U;
    }

    *lux = (uint16_t)value;
    return 1;
}
```

- [ ] **Step 3: Verify symbols are searchable**

Run:

```powershell
rg -n "s2_init|s2_read_light|BH1750" demo\Project\GD32F450Z_BSP\inc\s2.h demo\Project\GD32F450Z_BSP\src\s2.c
```

Expected output includes:

```text
s2.h:...:i2c_addr_def s2_init(uint8_t address);
s2.h:...:uint8_t s2_read_light(...)
s2.c:...:i2c_addr_def s2_init(...)
s2.c:...:uint8_t s2_read_light(...)
```

- [ ] **Step 4: Commit**

```bash
git add demo/Project/GD32F450Z_BSP/inc/s2.h demo/Project/GD32F450Z_BSP/src/s2.c
git commit -m "feat: add S2 light sensor driver"
```

## Task 3: Add S8 SHT35 Temperature/Humidity Driver

**Files:**

- Create: `demo/Project/GD32F450Z_BSP/inc/s8.h`
- Create: `demo/Project/GD32F450Z_BSP/src/s8.c`

- [ ] **Step 1: Create `s8.h`**

Write exactly:

```c
#ifndef S8_H
#define S8_H

#include "i2c.h"

#define SHT35_SOFT_RESET_MSB      0x30
#define SHT35_SOFT_RESET_LSB      0xA2
#define SHT35_MEASURE_MSB         0x2C
#define SHT35_MEASURE_LSB         0x0D

i2c_addr_def s8_init(uint8_t address);
uint8_t s8_read_temp_humi(uint32_t i2c_periph, uint8_t i2c_addr, float *temperature, float *humidity);

#endif
```

- [ ] **Step 2: Create `s8.c`**

Write exactly:

```c
#include "s8.h"
#include "main.h"

static void s8_write_command(uint32_t i2c_periph, uint8_t i2c_addr, uint8_t msb, uint8_t lsb)
{
    i2c_byte_write(i2c_periph, i2c_addr, msb, lsb);
}

i2c_addr_def s8_init(uint8_t address)
{
    i2c_addr_def s8_address;

    s8_address = get_board_address(address);
    if (s8_address.flag) {
        s8_write_command(s8_address.periph, s8_address.addr, SHT35_SOFT_RESET_MSB, SHT35_SOFT_RESET_LSB);
        delay_ms(20);
    }

    return s8_address;
}

uint8_t s8_read_temp_humi(uint32_t i2c_periph, uint8_t i2c_addr, float *temperature, float *humidity)
{
    uint8_t buffer[6] = {0};
    uint16_t temp_raw;
    uint16_t humi_raw;

    if (temperature == 0 || humidity == 0) {
        return 0;
    }

    s8_write_command(i2c_periph, i2c_addr, SHT35_MEASURE_MSB, SHT35_MEASURE_LSB);
    delay_ms(20);
    i2c_direct_read(i2c_periph, i2c_addr, buffer, 6);

    temp_raw = ((uint16_t)buffer[0] << 8) | buffer[1];
    humi_raw = ((uint16_t)buffer[3] << 8) | buffer[4];

    if (temp_raw == 0xFFFFU && humi_raw == 0xFFFFU) {
        return 0;
    }

    *temperature = ((float)temp_raw * 175.0f / 65535.0f) - 45.0f;
    *humidity = (float)humi_raw * 100.0f / 65535.0f;

    return 1;
}
```

- [ ] **Step 3: Verify symbols are searchable**

Run:

```powershell
rg -n "s8_init|s8_read_temp_humi|SHT35" demo\Project\GD32F450Z_BSP\inc\s8.h demo\Project\GD32F450Z_BSP\src\s8.c
```

Expected output includes both function definitions.

- [ ] **Step 4: Commit**

```bash
git add demo/Project/GD32F450Z_BSP/inc/s8.h demo/Project/GD32F450Z_BSP/src/s8.c
git commit -m "feat: add S8 temperature humidity driver"
```

## Task 4: Add E2 Fan Driver

**Files:**

- Create: `demo/Project/GD32F450Z_BSP/inc/e2.h`
- Create: `demo/Project/GD32F450Z_BSP/src/e2.c`

- [ ] **Step 1: Create `e2.h`**

Write exactly:

```c
#ifndef E2_H
#define E2_H

#include "i2c.h"

#define PCA9685_ADDRESS_E2    0xC8
#define PCA9685_MODE1         0x00
#define LED0_ON_L             0x06
#define LED0_ON_H             0x07
#define LED0_OFF_L            0x08
#define LED0_OFF_H            0x09
#define ALLLED_ON_L           0xFA
#define ALLLED_ON_H           0xFB
#define ALLLED_OFF_L          0xFC
#define ALLLED_OFF_H          0xFD

i2c_addr_def e2_init(uint8_t address);
void e2_fan_set_duty(uint32_t i2c_periph, uint8_t i2c_addr, uint8_t duty);
void e2_fan_on(uint32_t i2c_periph, uint8_t i2c_addr);
void e2_fan_off(uint32_t i2c_periph, uint8_t i2c_addr);

#endif
```

- [ ] **Step 2: Create `e2.c`**

Write exactly:

```c
#include "e2.h"
#include "smart_home_config.h"

static void e2_set_pwm(uint32_t i2c_periph, uint8_t i2c_addr, uint8_t num, uint16_t on, uint16_t off)
{
    i2c_byte_write(i2c_periph, i2c_addr, LED0_ON_L + 4U * num, (uint8_t)on);
    i2c_byte_write(i2c_periph, i2c_addr, LED0_ON_H + 4U * num, (uint8_t)(on >> 8));
    i2c_byte_write(i2c_periph, i2c_addr, LED0_OFF_L + 4U * num, (uint8_t)off);
    i2c_byte_write(i2c_periph, i2c_addr, LED0_OFF_H + 4U * num, (uint8_t)(off >> 8));
}

static void e2_all_pwm_off(uint32_t i2c_periph, uint8_t i2c_addr)
{
    i2c_byte_write(i2c_periph, i2c_addr, ALLLED_ON_L, 0);
    i2c_byte_write(i2c_periph, i2c_addr, ALLLED_ON_H, 0);
    i2c_byte_write(i2c_periph, i2c_addr, ALLLED_OFF_L, 0);
    i2c_byte_write(i2c_periph, i2c_addr, ALLLED_OFF_H, 0x10);
}

i2c_addr_def e2_init(uint8_t address)
{
    i2c_addr_def e2_address;
    uint8_t i;

    e2_address.flag = 0;
    e2_address.periph = 0;
    e2_address.addr = 0;

    for (i = 0; i < 4; i++) {
        e2_address = get_board_address(address + i * 2U);
        if (e2_address.flag) {
            i2c_delay_byte_write(e2_address.periph, e2_address.addr, PCA9685_MODE1, 0x00);
            e2_all_pwm_off(e2_address.periph, e2_address.addr);
            break;
        }
    }

    return e2_address;
}

void e2_fan_set_duty(uint32_t i2c_periph, uint8_t i2c_addr, uint8_t duty)
{
    uint16_t off;

    if (duty > 100U) {
        duty = 100U;
    }

    if (duty == 0U) {
        e2_set_pwm(i2c_periph, i2c_addr, 0, 0, 0);
        return;
    }

    off = (uint16_t)((4095U * duty) / 100U);
    e2_set_pwm(i2c_periph, i2c_addr, 0, 0, off);
}

void e2_fan_on(uint32_t i2c_periph, uint8_t i2c_addr)
{
    e2_fan_set_duty(i2c_periph, i2c_addr, FAN_ON_DUTY);
}

void e2_fan_off(uint32_t i2c_periph, uint8_t i2c_addr)
{
    e2_fan_set_duty(i2c_periph, i2c_addr, 0);
}
```

- [ ] **Step 3: Verify symbols are searchable**

Run:

```powershell
rg -n "e2_init|e2_fan_set_duty|e2_fan_on|e2_fan_off" demo\Project\GD32F450Z_BSP\inc\e2.h demo\Project\GD32F450Z_BSP\src\e2.c
```

Expected output includes all four public functions.

- [ ] **Step 4: Commit**

```bash
git add demo/Project/GD32F450Z_BSP/inc/e2.h demo/Project/GD32F450Z_BSP/src/e2.c
git commit -m "feat: add E2 fan driver"
```

## Task 5: Add C3 Wi-Fi Placeholder

**Files:**

- Create: `demo/Project/GD32F450Z_BSP/inc/c3_wifi.h`
- Create: `demo/Project/GD32F450Z_BSP/src/c3_wifi.c`

- [ ] **Step 1: Create `c3_wifi.h`**

Write exactly:

```c
#ifndef C3_WIFI_H
#define C3_WIFI_H

#include "gd32f4xx.h"

void Wifi_Init(void);
void Wifi_SendSensorData(float temperature, float humidity, uint16_t light);
void Wifi_ReceiveCommand(void);

#endif
```

- [ ] **Step 2: Create `c3_wifi.c`**

Write exactly:

```c
#include "c3_wifi.h"

void Wifi_Init(void)
{
}

void Wifi_SendSensorData(float temperature, float humidity, uint16_t light)
{
    (void)temperature;
    (void)humidity;
    (void)light;
}

void Wifi_ReceiveCommand(void)
{
}
```

- [ ] **Step 3: Verify symbols are searchable**

Run:

```powershell
rg -n "Wifi_Init|Wifi_SendSensorData|Wifi_ReceiveCommand" demo\Project\GD32F450Z_BSP\inc\c3_wifi.h demo\Project\GD32F450Z_BSP\src\c3_wifi.c
```

Expected output includes declarations and definitions.

- [ ] **Step 4: Commit**

```bash
git add demo/Project/GD32F450Z_BSP/inc/c3_wifi.h demo/Project/GD32F450Z_BSP/src/c3_wifi.c
git commit -m "feat: add C3 wifi placeholder"
```

## Task 6: Replace `main.c` With Smart Home State Machine

**Files:**

- Modify: `demo/Project/Application/src/main.c`

- [ ] **Step 1: Replace top-level includes and global state**

At the top of `main.c`, replace the old S3/S6/ring voice includes and audio globals with:

```c
#include "gd32f4xx.h"
#include "main.h"
#include "i2c.h"
#include "stdio.h"
#include "string.h"
#include "s1.h"
#include "e1.h"
#include "s2.h"
#include "s8.h"
#include "e2.h"
#include "c3_wifi.h"
#include "board_config.h"
#include "smart_home_config.h"

volatile uint16_t delay_count = 0;
volatile uint16_t time_count = 0;
volatile uint8_t deal_flag = 0;

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

static i2c_addr_def e1_led_addr;
static i2c_addr_def e1_display_addr;
static i2c_addr_def s1_key_addr;
static i2c_addr_def s2_light_addr;
static i2c_addr_def s8_temp_humi_addr;
static i2c_addr_def e2_fan_addr;
static SmartHomeState state = {0};
```

- [ ] **Step 2: Add local helper functions above `main()`**

Insert these helpers before `int main(void)`:

```c
static uint8_t clamp_u8_int(int value, uint8_t min, uint8_t max)
{
    if (value < (int)min) {
        return min;
    }
    if (value > (int)max) {
        return max;
    }
    return (uint8_t)value;
}

static void smart_home_led_set(uint8_t on)
{
    state.led_on = on ? 1U : 0U;
    if (e1_led_addr.flag) {
        if (state.led_on) {
            e1_rgb_control(e1_led_addr.periph, e1_led_addr.addr, 255, 255, 255);
        } else {
            e1_rgb_control(e1_led_addr.periph, e1_led_addr.addr, 0, 0, 0);
        }
    }
}

static void smart_home_fan_set(uint8_t on)
{
    state.fan_on = on ? 1U : 0U;
    if (e2_fan_addr.flag) {
        if (state.fan_on) {
            e2_fan_on(e2_fan_addr.periph, e2_fan_addr.addr);
        } else {
            e2_fan_off(e2_fan_addr.periph, e2_fan_addr.addr);
        }
    }
}

static void smart_home_display_update(void)
{
    uint8_t temp_display;
    uint8_t light_display;
    uint16_t display_value;

    if (!e1_display_addr.flag) {
        return;
    }

    temp_display = clamp_u8_int((int)state.temperature, 0, 99);
    light_display = clamp_u8_int((int)(state.light / 100U), 0, 99);
    display_value = (uint16_t)(temp_display * 100U + light_display);

    ht16k33_display_float(e1_display_addr.periph, e1_display_addr.addr, display_value);
}

static void smart_home_read_sensors(void)
{
    uint16_t light;
    float temperature;
    float humidity;

    if (s2_light_addr.flag && s2_read_light(s2_light_addr.periph, s2_light_addr.addr, &light)) {
        state.light = light;
    }

    if (s8_temp_humi_addr.flag && s8_read_temp_humi(s8_temp_humi_addr.periph, s8_temp_humi_addr.addr, &temperature, &humidity)) {
        if (temperature < -40.0f) {
            temperature = -40.0f;
        } else if (temperature > 125.0f) {
            temperature = 125.0f;
        }
        if (humidity < 0.0f) {
            humidity = 0.0f;
        } else if (humidity > 100.0f) {
            humidity = 100.0f;
        }
        state.temperature = temperature;
        state.humidity = humidity;
    }
}

static void smart_home_handle_key(uint8_t key)
{
    if (key == SW1) {
        state.mode = (state.mode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
    } else if (state.mode == MODE_MANUAL && key == SW2) {
        smart_home_led_set(!state.led_on);
    } else if (state.mode == MODE_MANUAL && key == SW3) {
        smart_home_fan_set(!state.fan_on);
    }
}

static void smart_home_apply_auto_control(void)
{
    if (state.mode != MODE_AUTO) {
        return;
    }

    smart_home_led_set(state.light < LIGHT_THRESHOLD);
    smart_home_fan_set(state.temperature >= TEMP_THRESHOLD);
}
```

- [ ] **Step 3: Replace `main()` body**

Replace the old `main()` function with:

```c
int main(void)
{
    uint8_t key_value;

    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);
    uart_init(USART0);

    dis_led_init();

    i2c0_gpio_config();
    i2c0_config();
    i2c1_gpio_config();
    i2c1_config();

    e1_led_addr = e1_init(E1_LED_ADDR);
    e1_display_addr = e1_init(E1_DISPLAY_ADDR);
    s1_key_addr = s1_init(S1_KEY_ADDR);
    s2_light_addr = s2_init(S2_LIGHT_ADDR);
    s8_temp_humi_addr = s8_init(S8_TEMP_HUMI_ADDR);
    e2_fan_addr = e2_init(E2_FAN_ADDR);
    Wifi_Init();

    state.mode = MODE_AUTO;
    state.temperature = 25.0f;
    state.humidity = 50.0f;
    state.light = 0;
    smart_home_led_set(0);
    smart_home_fan_set(0);

    timer3_init();

    while (1) {
        key_value = SWN;
        if (s1_key_addr.flag) {
            key_value = get_key_value(s1_key_addr.periph, s1_key_addr.addr);
        }

        if (key_value != SWN) {
            smart_home_handle_key(key_value);
        }

        smart_home_read_sensors();
        smart_home_apply_auto_control();
        smart_home_display_update();
        Wifi_SendSensorData(state.temperature, state.humidity, state.light);
        Wifi_ReceiveCommand();

        delay_ms(SENSOR_UPDATE_MS);
    }
}
```

- [ ] **Step 4: Remove old audio interrupt implementation**

Delete the old `SPI1_IRQHandler` body and the audio-specific global variables. Keep `TIMER3_IRQHandler`, `delay_ms`, `timer3_init`, `dis_led_init`, `uart_init`, `uart_print`, `uart_print16`, and `debug_printf`.

The resulting file must not include these strings:

```text
ring_voice
s3_init
s6_init
e3_sound_recording_config
e3_playback_config
playdata
playflag
record_flag
SPI1_IRQHandler
```

- [ ] **Step 5: Verify old business references are gone**

Run:

```powershell
rg -n "ring_voice|s3_init|s6_init|e3_sound_recording_config|e3_playback_config|playdata|playflag|record_flag|SPI1_IRQHandler" demo\Project\Application\src\main.c
```

Expected: no matches and exit code 1.

- [ ] **Step 6: Verify smart-home references exist**

Run:

```powershell
rg -n "SmartHomeState|smart_home_read_sensors|smart_home_apply_auto_control|s2_read_light|s8_read_temp_humi|e2_fan_on" demo\Project\Application\src\main.c
```

Expected: matches for each listed symbol.

- [ ] **Step 7: Commit**

```bash
git add demo/Project/Application/src/main.c
git commit -m "feat: replace main loop with smart home controller"
```

## Task 7: Add New Source Files to Keil Project

**Files:**

- Modify: `demo/Project/KEIL_Project/task2.uvprojx`

- [ ] **Step 1: Inspect existing BSP file entries**

Run:

```powershell
rg -n "<FileName>.*\\.c|<FilePath>.*GD32F450Z_BSP" demo\Project\KEIL_Project\task2.uvprojx
```

Expected: entries for existing BSP files such as `s1.c`, `e1.c`, and `i2c.c`.

- [ ] **Step 2: Add new `.c` files to the same BSP group**

Add file entries matching the project XML style. The required file paths are:

```xml
<FilePath>..\GD32F450Z_BSP\src\s2.c</FilePath>
<FilePath>..\GD32F450Z_BSP\src\s8.c</FilePath>
<FilePath>..\GD32F450Z_BSP\src\e2.c</FilePath>
<FilePath>..\GD32F450Z_BSP\src\c3_wifi.c</FilePath>
```

Use the neighboring `s1.c` or `e1.c` entries as the exact structural template for `<File>`, `<FileName>`, `<FileType>`, and `<FilePath>`.

- [ ] **Step 3: Verify project entries**

Run:

```powershell
rg -n "s2.c|s8.c|e2.c|c3_wifi.c" demo\Project\KEIL_Project\task2.uvprojx
```

Expected output includes all four file names.

- [ ] **Step 4: Commit**

```bash
git add demo/Project/KEIL_Project/task2.uvprojx
git commit -m "build: include smart home BSP sources in Keil project"
```

## Task 8: Build and Hardware Smoke Verification

**Files:**

- Verify: all implementation files from previous tasks.
- Modify only if build errors point to exact symbol mismatches.

- [ ] **Step 1: Compile in Keil or the existing VS Code embedded toolchain**

Use the existing project build command or Keil UI. Expected result:

```text
0 Error(s)
```

If the build reports a missing include path for `board_config.h`, `smart_home_config.h`, `s2.h`, `s8.h`, `e2.h`, or `c3_wifi.h`, add `demo/Project/GD32F450Z_BSP/inc` to the project include paths using the same include path pattern already used for `s1.h` and `e1.h`.

- [ ] **Step 2: Verify no old business references remain in main**

Run:

```powershell
rg -n "ring_voice|s3_init|s6_init|record|playback|distance" demo\Project\Application\src\main.c
```

Expected: no matches and exit code 1.

- [ ] **Step 3: Flash to U1 hardware**

Use the same flashing flow previously used for `task2.hex` or Keil download. Expected:

```text
download/flash succeeds
```

- [ ] **Step 4: Hardware check for S1**

Operate:

```text
Press K1 once.
Press K2 once.
Press K3 once.
```

Expected:

```text
K1 switches auto/manual mode.
In manual mode, K2 toggles LED.
In manual mode, K3 toggles fan.
```

- [ ] **Step 5: Hardware check for S2 and LED**

Operate:

```text
Cover S2 light sensor.
Uncover S2 light sensor.
```

Expected:

```text
In auto mode, covering S2 turns E1 LED on.
In auto mode, restoring light turns E1 LED off.
```

- [ ] **Step 6: Hardware check for S8 and E2**

Operate:

```text
Warm the S8 sensor or temporarily lower TEMP_THRESHOLD in smart_home_config.h to a value below room temperature.
Restore TEMP_THRESHOLD after the test.
```

Expected:

```text
In auto mode, temperature >= TEMP_THRESHOLD turns fan on.
Temperature < TEMP_THRESHOLD turns fan off.
```

- [ ] **Step 7: Hardware check for E1 display**

Observe:

```text
E1 four-digit display while system is running.
```

Expected:

```text
First two digits show integer temperature.
Last two digits show light / 100.
Example: 26 C and 780 lx displays 2607.
```

- [ ] **Step 8: Commit verification fixes**

Only if implementation files changed during verification:

```bash
git add demo/Project/GD32F450Z_BSP demo/Project/Application/src/main.c demo/Project/KEIL_Project/task2.uvprojx
git commit -m "fix: verify smart home hardware integration"
```

## Task 9: Documentation Sync After Hardware Verification

**Files:**

- Modify: `docs/prd.md`
- Modify: `docs/plan.md`
- Modify: `docs/settings.md`

- [ ] **Step 1: Confirm final runtime parameters**

Record the values actually used after hardware verification:

```text
LIGHT_THRESHOLD = 500U
TEMP_THRESHOLD = 28.0f
FAN_ON_DUTY = 80U
LED_ON_DUTY = 100U
SENSOR_UPDATE_MS = 500U
```

If hardware tuning changes any value, update the three docs consistently.

- [ ] **Step 2: Verify docs mention implementation files**

Run:

```powershell
rg -n "s2.c|s8.c|e2.c|c3_wifi.c|board_config.h|smart_home_config.h" docs\prd.md docs\plan.md docs\settings.md
```

Expected: at least `docs/plan.md` contains all implementation filenames; `docs/prd.md` and `docs/settings.md` contain the hardware addresses and system behavior.

- [ ] **Step 3: Verify docs contain no placeholders**

Run:

```powershell
rg -n "T[O]DO|T[B]D|F[I]XME|待[定]" docs\prd.md docs\plan.md docs\settings.md
```

Expected: no matches and exit code 1.

- [ ] **Step 4: Commit docs**

```bash
git add docs/prd.md docs/plan.md docs/settings.md
git commit -m "docs: sync smart home implementation details"
```

## Self-Review

Spec coverage:

- PRD local smart-home scope is covered by Tasks 1 through 8.
- S2 light collection is covered by Task 2 and Task 8.
- S8 temperature/humidity collection is covered by Task 3 and Task 8.
- E1 LED/display usage is covered by Task 6 and Task 8.
- E2 fan control is covered by Task 4 and Task 8.
- S1 manual/auto control is covered by Task 6 and Task 8.
- C3 reserved interface is covered by Task 5.
- Documentation sync is covered by Task 9.

Placeholder scan:

- This plan contains no unfinished placeholder markers.
- Each code-writing step includes concrete code.
- Each verification step includes exact commands or hardware actions and expected results.

Type consistency:

- All new BSP init functions return `i2c_addr_def`, matching existing `s1_init()` and `e1_init()`.
- Sensor read functions return `uint8_t` success flags, matching local embedded style.
- `main.c` uses `SW1`, `SW2`, `SW3`, and `SWN` from existing `s1.h`.
- `main.c` uses `e1_rgb_control()` and `ht16k33_display_float()` from existing `e1.h`.
