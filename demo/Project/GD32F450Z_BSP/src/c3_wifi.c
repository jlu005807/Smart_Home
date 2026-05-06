#include "c3_wifi.h"
#include "i2c.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

#define C3_WIFI_USART_PERIPH            USART5
#define C3_WIFI_SSID                    "vivo X2000 Pro"
#define C3_WIFI_PASSWORD                "12345678"
#define C3_SERVER_HOST                  "192.168.110.167"
#define C3_SERVER_PORT                  5000U
#define C3_DEVICE_ID                    "smart-home-001"
#define C3_RESP_BUFFER_SIZE             768U
#define C3_HTTP_BUFFER_SIZE             512U
#define C3_CMD_POLL_LOOP_INTERVAL       10U
#define C3_REPORT_LOOP_INTERVAL         10U
#define C3_WIFI_JOIN_TIMEOUT_MS         20000U
#define C3_TCP_OPEN_TIMEOUT_MS          1200U
#define C3_HTTP_WAIT_TIMEOUT_MS         900U

static wifi_cmd_t g_pending_cmd = WIFI_CMD_NONE;
static uint8_t g_poll_div = 0U;
static uint8_t g_report_div = 0U;
static uint8_t g_wifi_ready = 0U;
static char g_resp_buffer[C3_RESP_BUFFER_SIZE];
static char g_http_buffer[C3_HTTP_BUFFER_SIZE];

static void c3_wait_ms(uint16_t ms)
{
    i2c_delay((uint32_t)ms * 12000U);
}

static void c3_uart_send_text(const char *text)
{
    uart_print(C3_WIFI_USART_PERIPH, (uint8_t *)text, (uint16_t)strlen(text));
}

static void c3_uart_flush_rx(void)
{
    uint32_t idle = 0U;

    while (idle < 30000U) {
        if (RESET != usart_flag_get(C3_WIFI_USART_PERIPH, USART_FLAG_RBNE)) {
            (void)usart_data_receive(C3_WIFI_USART_PERIPH);
            idle = 0U;
        } else {
            idle++;
        }
    }
}

static uint16_t c3_uart_read_response(char *buffer, uint16_t buffer_size, uint16_t timeout_ms)
{
    uint16_t length = 0U;
    uint16_t waited = 0U;

    if (buffer_size == 0U) {
        return 0U;
    }

    buffer[0] = '\0';
    while (waited < timeout_ms) {
        uint8_t got_data = 0U;

        while (RESET != usart_flag_get(C3_WIFI_USART_PERIPH, USART_FLAG_RBNE)) {
            uint8_t ch = (uint8_t)usart_data_receive(C3_WIFI_USART_PERIPH);
            if (length + 1U < buffer_size) {
                buffer[length++] = (char)ch;
            }
            got_data = 1U;
        }

        if (got_data != 0U) {
            waited = 0U;
            continue;
        }

        c3_wait_ms(1U);
        waited++;
    }

    buffer[length] = '\0';
    return length;
}

static uint8_t c3_send_at_expect(const char *at_cmd, const char *expect, uint16_t timeout_ms)
{
    c3_uart_flush_rx();
    c3_uart_send_text(at_cmd);
    c3_uart_read_response(g_resp_buffer, (uint16_t)sizeof(g_resp_buffer), timeout_ms);

    if (expect == 0) {
        return 1U;
    }
    if (strstr(g_resp_buffer, expect) != 0) {
        return 1U;
    }
    return 0U;
}

static uint8_t c3_open_tcp(void)
{
    int n;
    if (!g_wifi_ready) {
        return 0U;
    }

    (void)c3_send_at_expect("AT+CIPCLOSE\r\n", "OK", 300U);

    n = snprintf(g_http_buffer,
                 sizeof(g_http_buffer),
                 "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n",
                 C3_SERVER_HOST,
                 C3_SERVER_PORT);
    if (n <= 0 || n >= (int)sizeof(g_http_buffer)) {
        return 0U;
    }

    if (c3_send_at_expect(g_http_buffer, "OK", C3_TCP_OPEN_TIMEOUT_MS)) {
        return 1U;
    }
    if (strstr(g_resp_buffer, "CONNECT") != 0) {
        return 1U;
    }
    return 0U;
}

static uint8_t c3_send_http_and_wait_response(const char *http_msg, uint16_t wait_ms)
{
    int n;
    uint16_t payload_len = (uint16_t)strlen(http_msg);

    n = snprintf(g_http_buffer, sizeof(g_http_buffer), "AT+CIPSEND=%u\r\n", payload_len);
    if (n <= 0 || n >= (int)sizeof(g_http_buffer)) {
        return 0U;
    }

    if (!c3_send_at_expect(g_http_buffer, ">", 1200U)) {
        return 0U;
    }

    c3_uart_send_text(http_msg);
    c3_uart_read_response(g_resp_buffer, (uint16_t)sizeof(g_resp_buffer), wait_ms);
    (void)c3_send_at_expect("AT+CIPCLOSE\r\n", "OK", 500U);
    return 1U;
}

static wifi_cmd_t c3_parse_cmd_from_response(const char *response)
{
    if (response == 0) {
        return WIFI_CMD_NONE;
    }
    if (strstr(response, "LED_ON") != 0) {
        return WIFI_CMD_LED_ON;
    }
    if (strstr(response, "LED_OFF") != 0) {
        return WIFI_CMD_LED_OFF;
    }
    if (strstr(response, "FAN_ON") != 0) {
        return WIFI_CMD_FAN_ON;
    }
    if (strstr(response, "FAN_OFF") != 0) {
        return WIFI_CMD_FAN_OFF;
    }
    if (strstr(response, "AUTO_MODE") != 0) {
        return WIFI_CMD_AUTO_MODE;
    }
    if (strstr(response, "MANUAL_MODE") != 0) {
        return WIFI_CMD_MANUAL_MODE;
    }
    if (strstr(response, "READ_STATUS") != 0) {
        return WIFI_CMD_READ_STATUS;
    }
    return WIFI_CMD_NONE;
}

void Wifi_Init(void)
{
    int n;
    uint8_t ok_at;
    uint8_t ok;

    uart_init(C3_WIFI_USART_PERIPH);
    c3_wait_ms(200U);

    ok_at = c3_send_at_expect("AT\r\n", "OK", 800U);
    if (!ok_at) {
        debug_printf(USART0, "C3 AT FAIL\r\n");
        g_wifi_ready = 0U;
        return;
    }

    (void)c3_send_at_expect("ATE0\r\n", "OK", 800U);
    (void)c3_send_at_expect("AT+CWMODE=1\r\n", "OK", 1200U);

    n = snprintf(g_http_buffer,
                 sizeof(g_http_buffer),
                 "AT+CWJAP=\"%s\",\"%s\"\r\n",
                 C3_WIFI_SSID,
                 C3_WIFI_PASSWORD);
    if (n > 0 && n < (int)sizeof(g_http_buffer)) {
        ok = c3_send_at_expect(g_http_buffer, "OK", C3_WIFI_JOIN_TIMEOUT_MS);
        if (ok) {
            debug_printf(USART0, "C3 WIFI JOIN OK\r\n");
            g_wifi_ready = 1U;
        } else {
            debug_printf(USART0, "C3 WIFI JOIN FAIL\r\n");
            if (strlen(g_resp_buffer) > 0U) {
                debug_printf(USART0, g_resp_buffer);
                debug_printf(USART0, "\r\n");
            }
            g_wifi_ready = 0U;
        }
    }

    if (g_wifi_ready) {
        (void)c3_send_at_expect("AT+CIPMUX=0\r\n", "OK", 1200U);
    }
}

void Wifi_SendSensorData(float temperature, float humidity, uint16_t light, uint8_t fan_on, uint8_t led_on, uint8_t mode)
{
    int body_len;
    int req_len;
    char request_buffer[C3_RESP_BUFFER_SIZE];
    const char *fan_text = (fan_on != 0U) ? "on" : "off";
    const char *led_text = (led_on != 0U) ? "on" : "off";
    const char *mode_text = (mode == 0U) ? "auto" : "manual";

    if (!g_wifi_ready) {
        return;
    }

    g_report_div++;
    if (g_report_div < C3_REPORT_LOOP_INTERVAL) {
        return;
    }
    g_report_div = 0U;

    body_len = snprintf(g_http_buffer,
                        sizeof(g_http_buffer),
                        "{\"device_id\":\"%s\",\"temperature\":%.2f,\"humidity\":%.2f,\"light\":%u,\"fan\":\"%s\",\"led\":\"%s\",\"mode\":\"%s\"}",
                        C3_DEVICE_ID,
                        temperature,
                        humidity,
                        light,
                        fan_text,
                        led_text,
                        mode_text);
    if (body_len <= 0 || body_len >= (int)sizeof(g_http_buffer)) {
        return;
    }

    req_len = snprintf(request_buffer,
                       sizeof(request_buffer),
                       "POST /api/device/report HTTP/1.1\r\n"
                       "Host: %s:%u\r\n"
                       "Content-Type: application/json\r\n"
                       "Connection: close\r\n"
                       "Content-Length: %d\r\n"
                       "\r\n"
                       "%s",
                       C3_SERVER_HOST,
                       C3_SERVER_PORT,
                       body_len,
                       g_http_buffer);
    if (req_len <= 0 || req_len >= (int)sizeof(request_buffer)) {
        return;
    }

    if (!c3_open_tcp()) {
        return;
    }
    (void)c3_send_http_and_wait_response(request_buffer, C3_HTTP_WAIT_TIMEOUT_MS);
}

void Wifi_ReceiveCommand(void)
{
    int req_len;
    wifi_cmd_t cmd;
    if (!g_wifi_ready) {
        return;
    }

    g_poll_div++;
    if (g_poll_div < C3_CMD_POLL_LOOP_INTERVAL) {
        return;
    }
    g_poll_div = 0U;

    req_len = snprintf(g_http_buffer,
                       sizeof(g_http_buffer),
                       "GET /api/device/next_cmd?device_id=%s HTTP/1.1\r\n"
                       "Host: %s:%u\r\n"
                       "Connection: close\r\n"
                       "\r\n",
                       C3_DEVICE_ID,
                       C3_SERVER_HOST,
                       C3_SERVER_PORT);
    if (req_len <= 0 || req_len >= (int)sizeof(g_http_buffer)) {
        return;
    }

    if (!c3_open_tcp()) {
        return;
    }

    if (!c3_send_http_and_wait_response(g_http_buffer, C3_HTTP_WAIT_TIMEOUT_MS)) {
        return;
    }

    cmd = c3_parse_cmd_from_response(g_resp_buffer);
    if (cmd != WIFI_CMD_NONE) {
        g_pending_cmd = cmd;
    }
}

wifi_cmd_t Wifi_GetPendingCommand(void)
{
    wifi_cmd_t cmd = g_pending_cmd;
    g_pending_cmd = WIFI_CMD_NONE;
    return cmd;
}
