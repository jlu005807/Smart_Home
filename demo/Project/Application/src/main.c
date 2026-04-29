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

    if (s8_temp_humi_addr.flag &&
        s8_read_temp_humi(s8_temp_humi_addr.periph, s8_temp_humi_addr.addr, &temperature, &humidity)) {
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

static void smart_home_debug_output(void)
{
    char msg[128];

    sprintf(msg,
            "mode=%s temp=%.2fC humi=%.2f%%RH light=%u led=%u fan=%u\r\n",
            (state.mode == MODE_AUTO) ? "AUTO" : "MANUAL",
            state.temperature,
            state.humidity,
            state.light,
            state.led_on,
            state.fan_on);
    debug_printf(USART0, msg);
}

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
        smart_home_debug_output();
        Wifi_SendSensorData(state.temperature, state.humidity, state.light);
        Wifi_ReceiveCommand();

        delay_ms(SENSOR_UPDATE_MS);
    }
}

void timer3_init(void)
{
    timer_parameter_struct timer_init_struct;

    rcu_periph_clock_enable(RCU_TIMER3);

    timer_deinit(TIMER3);
    timer_init_struct.prescaler = 4199;
    timer_init_struct.period = 20;
    timer_init_struct.alignedmode = TIMER_COUNTER_EDGE;
    timer_init_struct.counterdirection = TIMER_COUNTER_UP;
    timer_init_struct.clockdivision = TIMER_CKDIV_DIV1;
    timer_init_struct.repetitioncounter = 0;
    timer_init(TIMER3, &timer_init_struct);

    nvic_irq_enable(TIMER3_IRQn, 1, 1);
    timer_interrupt_enable(TIMER3, TIMER_INT_UP);
    timer_enable(TIMER3);
}

void delay_ms(uint16_t mstime)
{
    delay_count = mstime;
    while (delay_count) {
    }
}

void dis_led_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOF);
    gpio_mode_set(GPIOF, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_4);
    gpio_output_options_set(GPIOF, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_4);
    GPIO_BC(GPIOF) = GPIO_PIN_4;
}

void uart_init(uint32_t usart_periph)
{
    if (usart_periph == USART0) {
        nvic_irq_enable(USART0_IRQn, 0, 0);
        rcu_periph_clock_enable(RCU_GPIOA);
        rcu_periph_clock_enable(RCU_USART0);
        gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_9);
        gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_10);
        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9);
        gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_10);
        gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);
        usart_deinit(USART0);
        usart_baudrate_set(USART0, 115200U);
        usart_receive_config(USART0, USART_RECEIVE_ENABLE);
        usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
        usart_enable(USART0);
        usart_interrupt_enable(USART0, USART_INT_RBNE);
    } else if (usart_periph == USART2) {
        nvic_irq_enable(USART2_IRQn, 0, 0);
        rcu_periph_clock_enable(RCU_GPIOD);
        rcu_periph_clock_enable(RCU_USART2);
        gpio_af_set(GPIOD, GPIO_AF_7, GPIO_PIN_8);
        gpio_af_set(GPIOD, GPIO_AF_7, GPIO_PIN_9);
        gpio_mode_set(GPIOD, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_8);
        gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8);
        gpio_mode_set(GPIOD, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9);
        gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
        usart_deinit(USART2);
        usart_baudrate_set(USART2, 115200U);
        usart_receive_config(USART2, USART_RECEIVE_ENABLE);
        usart_transmit_config(USART2, USART_TRANSMIT_ENABLE);
        usart_enable(USART2);
        usart_interrupt_enable(USART2, USART_INT_RBNE);
    } else if (usart_periph == USART5) {
        nvic_irq_enable(USART5_IRQn, 0, 0);
        rcu_periph_clock_enable(RCU_GPIOC);
        rcu_periph_clock_enable(RCU_USART5);
        gpio_af_set(GPIOC, GPIO_AF_8, GPIO_PIN_6);
        gpio_af_set(GPIOC, GPIO_AF_8, GPIO_PIN_7);
        gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_6);
        gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
        gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_7);
        gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7);
        usart_deinit(USART5);
        usart_baudrate_set(USART5, 115200U);
        usart_receive_config(USART5, USART_RECEIVE_ENABLE);
        usart_transmit_config(USART5, USART_TRANSMIT_ENABLE);
        usart_enable(USART5);
        usart_interrupt_enable(USART5, USART_INT_RBNE);
    }
}

uint16_t uart_print(uint32_t usart_periph, uint8_t *data, uint16_t len)
{
    uint16_t i;

    for (i = 0; i < len; i++) {
        while (usart_flag_get(usart_periph, USART_FLAG_TC) == RESET) {
        }
        usart_data_transmit(usart_periph, data[i]);
    }

    while (usart_flag_get(usart_periph, USART_FLAG_TC) == RESET) {
    }
    return len;
}

uint16_t uart_print16(uint32_t usart_periph, uint16_t *data, uint16_t len)
{
    uint16_t i;
    uint8_t print_buffer[100];

    for (i = 0; i < len; i++) {
        sprintf((char *)print_buffer, (const char *)"0x%x,", data[i]);
        debug_printf(USART2, (char *)print_buffer);
        if (((i % 50U) == 0U) && (i != 0U)) {
            sprintf((char *)print_buffer, (const char *)"\r\n");
            debug_printf(USART2, (char *)print_buffer);
        }
    }

    while (usart_flag_get(usart_periph, USART_FLAG_TC) == RESET) {
    }
    return len;
}

void debug_printf(uint32_t usart_periph, char *string)
{
    uint8_t buffer[100];
    uint16_t len;

    len = (uint16_t)strlen(string);
    strncpy((char *)buffer, string, len);
    uart_print(usart_periph, buffer, len);
}

void TIMER3_IRQHandler(void)
{
    if (timer_interrupt_flag_get(TIMER3, TIMER_INT_FLAG_UP)) {
        timer_interrupt_flag_clear(TIMER3, TIMER_INT_FLAG_UP);

        if (delay_count > 0U) {
            delay_count--;
        }

        if (time_count++ >= 1000U) {
            time_count = 0;
            deal_flag = 1;
        }
    }
}
