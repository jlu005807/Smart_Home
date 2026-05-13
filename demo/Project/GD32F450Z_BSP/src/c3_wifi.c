#include "c3_wifi.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

#define C3_WIFI_USART_PERIPH            USART5
#define C3_WIFI_SSID                    "vivo X2000 Pro"
#define C3_WIFI_PASSWORD                "12345678"
#define C3_SERVER_HOST                  "192.168.103.167"
#define C3_SERVER_PORT                  5000U
#define C3_DEVICE_ID                    "smart-home-001"
#define C3_LINK_ID                      0U

#define C3_RESP_BUFFER_SIZE             768U
#define C3_HTTP_BUFFER_SIZE             512U
#define C3_CMD_POLL_LOOP_INTERVAL       5U
#define C3_REPORT_LOOP_INTERVAL         5U
#define C3_WIFI_JOIN_TIMEOUT_MS         20000U
#define C3_TCP_OPEN_TIMEOUT_MS          1500U
#define C3_HTTP_WAIT_TIMEOUT_MS         1200U
#define C3_UART_FLUSH_IDLE_MS           20U
#define C3_AT_READY_RETRY_COUNT         5U
#define C3_AT_READY_TIMEOUT_MS          1000U
#define C3_WIFI_LEAVE_TIMEOUT_MS        3000U
#define C3_WIFI_FORCE_RESTORE_ON_BOOT   1U
#define C3_WIFI_RESTORE_TIMEOUT_MS      8000U
#define C3_WIFI_POST_RESTORE_WAIT_MS    3000U
#define C3_WIFI_VERIFY_TIMEOUT_MS       1500U

#define C3_TCP_DEBUG                    1U

#define C3_SERVER_ACTIVE_QUERY          0U
#define C3_SERVER_LISTEN_PORT           5001U
#define C3_SERVER_RX_TIMEOUT_MS         10U

#if C3_SERVER_ACTIVE_QUERY
#define C3_REPORT_TO_SERVER             0U
#else
#define C3_REPORT_TO_SERVER             1U
#endif

static wifi_cmd_t g_pending_cmd = WIFI_CMD_NONE;
static uint8_t g_poll_div = 0U;
static uint8_t g_report_div = 0U;
static uint8_t g_wifi_ready = 0U;
static uint8_t g_use_multi_conn = 1U;
static uint8_t g_reconnect_div = 0U;
static uint8_t g_tcp_fail_count = 0U;
static uint8_t g_restore_done = 0U;
static char g_resp_buffer[C3_RESP_BUFFER_SIZE];
static char g_http_buffer[C3_HTTP_BUFFER_SIZE];

static wifi_cmd_t c3_parse_cmd_from_response(const char *response);

#if C3_SERVER_ACTIVE_QUERY
typedef struct {
    float temperature;
    float humidity;
    uint16_t light;
    uint8_t fan_on;
    uint8_t led_on;
    uint8_t mode;
    uint8_t valid;
} c3_status_cache_t;

static c3_status_cache_t g_status_cache;
#endif

static void c3_wait_ms(uint16_t ms)
{
    delay_ms(ms);
}

static void c3_uart_send_text(const char *text)
{
    if (text == 0) {
        return;
    }
    uart_print(C3_WIFI_USART_PERIPH, (uint8_t *)text, (uint16_t)strlen(text));
}

static void c3_uart_flush_rx(void)
{
    uint16_t idle_ms = 0U;

    while (idle_ms < C3_UART_FLUSH_IDLE_MS) {
        if (RESET != usart_flag_get(C3_WIFI_USART_PERIPH, USART_FLAG_RBNE)) {
            (void)usart_data_receive(C3_WIFI_USART_PERIPH);
            idle_ms = 0U;
        } else {
            c3_wait_ms(1U);
            idle_ms++;
        }
    }
}

static uint16_t c3_uart_read_response(char *buffer, uint16_t buffer_size, uint16_t timeout_ms)
{
    uint16_t length = 0U;
    uint16_t waited = 0U;

    if (buffer == 0 || buffer_size == 0U) {
        return 0U;
    }

    buffer[0] = '\0';
    while (waited < timeout_ms) {
        uint8_t got_data = 0U;

        while (RESET != usart_flag_get(C3_WIFI_USART_PERIPH, USART_FLAG_RBNE)) {
            uint8_t ch = (uint8_t)usart_data_receive(C3_WIFI_USART_PERIPH);
            if ((length + 1U) < buffer_size) {
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

static void c3_log_resp(const char *title)
{
    if (title != 0) {
        debug_printf(USART0, (char *)title);
    }
    if (g_resp_buffer[0] != '\0') {
        debug_printf(USART0, g_resp_buffer);
        if (g_resp_buffer[strlen(g_resp_buffer) - 1U] != '\n') {
            debug_printf(USART0, "\r\n");
        }
    }
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

#if C3_SERVER_ACTIVE_QUERY
static uint16_t c3_extract_ipd_payload(const char *resp, uint8_t *link_id, const char **payload)
{
    const char *ipd;
    const char *p;
    uint32_t id = 0U;
    uint32_t len = 0U;

    if (resp == 0 || link_id == 0 || payload == 0) {
        return 0U;
    }

    ipd = strstr(resp, "+IPD,");
    if (ipd == 0) {
        return 0U;
    }

    p = ipd + 5;
    if (*p < '0' || *p > '9') {
        return 0U;
    }
    while (*p >= '0' && *p <= '9') {
        id = (id * 10U) + (uint32_t)(*p - '0');
        p++;
    }
    if (*p != ',') {
        return 0U;
    }
    p++;
    if (*p < '0' || *p > '9') {
        return 0U;
    }
    while (*p >= '0' && *p <= '9') {
        len = (len * 10U) + (uint32_t)(*p - '0');
        p++;
    }
    if (*p != ':') {
        return 0U;
    }
    p++;
    if (len == 0U) {
        return 0U;
    }

    *link_id = (uint8_t)id;
    *payload = p;
    return (uint16_t)len;
}

static void c3_close_tcp_link(uint8_t link_id)
{
    int n;

    n = snprintf(g_http_buffer, sizeof(g_http_buffer), "AT+CIPCLOSE=%u\r\n", link_id);
    if (n > 0 && n < (int)sizeof(g_http_buffer)) {
        c3_uart_send_text(g_http_buffer);
    }
}

static uint8_t c3_send_tcp_payload(uint8_t link_id, const char *payload)
{
    int n;
    uint16_t payload_len;
    char at_cmd[32];

    if (payload == 0) {
        return 0U;
    }

    payload_len = (uint16_t)strlen(payload);
    n = snprintf(at_cmd, sizeof(at_cmd), "AT+CIPSEND=%u,%u\r\n", link_id, payload_len);
    if (n <= 0 || n >= (int)sizeof(at_cmd)) {
        return 0U;
    }

    if (c3_send_at_expect(at_cmd, ">", 1200U) == 0U) {
        return 0U;
    }

    c3_uart_send_text(payload);
    c3_uart_read_response(g_resp_buffer, (uint16_t)sizeof(g_resp_buffer), 500U);
    return 1U;
}

static void c3_send_status_response(uint8_t link_id)
{
    char temp_text[16];
    char hum_text[16];
    char light_text[16];
    const char *fan_text = "unknown";
    const char *led_text = "unknown";
    const char *mode_text = "unknown";
    int n;

    if (g_status_cache.valid != 0U) {
        (void)snprintf(temp_text, sizeof(temp_text), "%.2f", g_status_cache.temperature);
        (void)snprintf(hum_text, sizeof(hum_text), "%.2f", g_status_cache.humidity);
        (void)snprintf(light_text, sizeof(light_text), "%u", g_status_cache.light);
        fan_text = (g_status_cache.fan_on != 0U) ? "on" : "off";
        led_text = (g_status_cache.led_on != 0U) ? "on" : "off";
        mode_text = (g_status_cache.mode == 0U) ? "auto" : "manual";
    } else {
        (void)snprintf(temp_text, sizeof(temp_text), "null");
        (void)snprintf(hum_text, sizeof(hum_text), "null");
        (void)snprintf(light_text, sizeof(light_text), "null");
    }

    n = snprintf(g_http_buffer,
                 sizeof(g_http_buffer),
                 "{\"device_id\":\"%s\",\"temperature\":%s,\"humidity\":%s,\"light\":%s,\"fan\":\"%s\",\"led\":\"%s\",\"mode\":\"%s\"}\n",
                 C3_DEVICE_ID,
                 temp_text,
                 hum_text,
                 light_text,
                 fan_text,
                 led_text,
                 mode_text);
    if (n > 0 && n < (int)sizeof(g_http_buffer)) {
        (void)c3_send_tcp_payload(link_id, g_http_buffer);
    }
}

static void c3_handle_server_payload(uint8_t link_id, const char *payload, uint16_t payload_len)
{
    char request[96];
    uint16_t copy_len = payload_len;
    uint8_t want_status;
    wifi_cmd_t cmd;

    if (payload == 0 || payload_len == 0U) {
        return;
    }

    if (copy_len >= (uint16_t)sizeof(request)) {
        copy_len = (uint16_t)(sizeof(request) - 1U);
    }

    (void)memcpy(request, payload, copy_len);
    request[copy_len] = '\0';

    want_status = (strstr(request, "STATUS") != 0) ? 1U : 0U;
    cmd = c3_parse_cmd_from_response(request);
    if (cmd != WIFI_CMD_NONE) {
        g_pending_cmd = cmd;
    }

    if (want_status != 0U) {
        c3_send_status_response(link_id);
    } else {
        (void)c3_send_tcp_payload(link_id, "OK\n");
    }

    c3_close_tcp_link(link_id);
}

static void c3_process_server_request(void)
{
    uint8_t link_id = 0U;
    const char *payload = 0;
    uint16_t payload_len;

    c3_uart_read_response(g_resp_buffer, (uint16_t)sizeof(g_resp_buffer), C3_SERVER_RX_TIMEOUT_MS);
    payload_len = c3_extract_ipd_payload(g_resp_buffer, &link_id, &payload);
    if (payload_len == 0U) {
        return;
    }

    c3_handle_server_payload(link_id, payload, payload_len);
}
#endif

static uint8_t c3_wait_at_ready(void)
{
    uint8_t retry;

    for (retry = 0U; retry < C3_AT_READY_RETRY_COUNT; retry++) {
        if (c3_send_at_expect("AT\r\n", "OK", C3_AT_READY_TIMEOUT_MS) != 0U) {
            return 1U;
        }
        c3_log_resp("C3 AT WAIT: ");
        c3_wait_ms(500U);
    }

    return 0U;
}

static void c3_restore_module_config_once(void)
{
#if C3_WIFI_FORCE_RESTORE_ON_BOOT
    if (g_restore_done != 0U) {
        return;
    }

    g_restore_done = 1U;
    debug_printf(USART0, "C3 RESTORE CONFIG\r\n");

    c3_uart_flush_rx();
    c3_uart_send_text("AT+RESTORE\r\n");
    c3_uart_read_response(g_resp_buffer, (uint16_t)sizeof(g_resp_buffer), C3_WIFI_RESTORE_TIMEOUT_MS);
    c3_log_resp("C3 RESTORE RESP: ");

    c3_wait_ms(C3_WIFI_POST_RESTORE_WAIT_MS);
    c3_uart_flush_rx();
    (void)c3_wait_at_ready();
#endif
}

static void c3_clear_previous_wifi(void)
{
    (void)c3_send_at_expect("AT+CWAUTOCONN=0\r\n", "OK", 1000U);
    (void)c3_send_at_expect("AT+CWRECONNCFG=0,0\r\n", "OK", 1000U);
    (void)c3_send_at_expect("AT+CWQAP\r\n", "OK", C3_WIFI_LEAVE_TIMEOUT_MS);
    (void)c3_send_at_expect("AT+SYSSTORE=0\r\n", "OK", 1000U);
}

static uint8_t c3_join_wifi(void)
{
    char join_cmd[160];
    int n;

    n = snprintf(join_cmd,
                 sizeof(join_cmd),
                 "AT+CWJAP=\"%s\",\"%s\"\r\n",
                 C3_WIFI_SSID,
                 C3_WIFI_PASSWORD);
    if (n > 0 && n < (int)sizeof(join_cmd)) {
        if (c3_send_at_expect(join_cmd, "OK", C3_WIFI_JOIN_TIMEOUT_MS) != 0U) {
            return 1U;
        }
        c3_log_resp("C3 CWJAP FAIL: ");
    }

    n = snprintf(join_cmd,
                 sizeof(join_cmd),
                 "AT+CWJAP_CUR=\"%s\",\"%s\"\r\n",
                 C3_WIFI_SSID,
                 C3_WIFI_PASSWORD);
    if (n <= 0 || n >= (int)sizeof(join_cmd)) {
        debug_printf(USART0, "C3 CWJAP BUILD FAIL\r\n");
        return 0U;
    }

    if (c3_send_at_expect(join_cmd, "OK", C3_WIFI_JOIN_TIMEOUT_MS) != 0U) {
        return 1U;
    }

    c3_log_resp("C3 CWJAP_CUR FAIL: ");
    return 0U;
}

static uint8_t c3_verify_joined_ap(void)
{
    (void)c3_send_at_expect("AT+CWJAP?\r\n", "OK", C3_WIFI_VERIFY_TIMEOUT_MS);
    c3_log_resp("C3 AP INFO: ");

    if (strstr(g_resp_buffer, C3_WIFI_SSID) != 0) {
        return 1U;
    }

    debug_printf(USART0, "C3 AP VERIFY FAIL\r\n");
    return 0U;
}

static void c3_close_tcp(void)
{
    if (g_use_multi_conn != 0U) {
        c3_uart_send_text("AT+CIPCLOSE=0\r\n");
    } else {
        c3_uart_send_text("AT+CIPCLOSE\r\n");
    }
    c3_wait_ms(1100U);
}

static uint8_t c3_open_tcp(void)
{
    int n;

    if (g_wifi_ready == 0U) {
        return 0U;
    }

    c3_close_tcp();

    if (g_use_multi_conn != 0U) {
        n = snprintf(g_http_buffer,
                     sizeof(g_http_buffer),
                     "AT+CIPSTART=%u,\"TCP\",\"%s\",%u\r\n",
                     C3_LINK_ID,
                     C3_SERVER_HOST,
                     C3_SERVER_PORT);
    } else {
        n = snprintf(g_http_buffer,
                     sizeof(g_http_buffer),
                     "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n",
                     C3_SERVER_HOST,
                     C3_SERVER_PORT);
    }
    if (n <= 0 || n >= (int)sizeof(g_http_buffer)) {
        return 0U;
    }

    if (c3_send_at_expect(g_http_buffer, "OK", C3_TCP_OPEN_TIMEOUT_MS) != 0U) {
        g_tcp_fail_count = 0U;
#if C3_TCP_DEBUG
        debug_printf(USART0, "C3 TCP OK\r\n");
#endif
        return 1U;
    }
    if (strstr(g_resp_buffer, "CONNECT") != 0) {
        g_tcp_fail_count = 0U;
#if C3_TCP_DEBUG
        debug_printf(USART0, "C3 TCP OK\r\n");
#endif
        return 1U;
    }
    c3_log_resp("C3 TCP OPEN FAIL: ");
    if (g_tcp_fail_count < 255U) {
        g_tcp_fail_count++;
    }
    if (g_tcp_fail_count >= 3U) {
        g_wifi_ready = 0U;
        g_tcp_fail_count = 0U;
        debug_printf(USART0, "C3 NET LOST\r\n");
    }
    return 0U;
}

static uint8_t c3_send_http_and_wait_response(const char *http_msg, uint16_t wait_ms)
{
    int n;
    uint16_t payload_len;
    char at_cmd[32];

    if (http_msg == 0) {
        return 0U;
    }

    payload_len = (uint16_t)strlen(http_msg);
    if (g_use_multi_conn != 0U) {
        n = snprintf(at_cmd, sizeof(at_cmd), "AT+CIPSEND=%u,%u\r\n", C3_LINK_ID, payload_len);
    } else {
        n = snprintf(at_cmd, sizeof(at_cmd), "AT+CIPSEND=%u\r\n", payload_len);
    }
    if (n <= 0 || n >= (int)sizeof(at_cmd)) {
        return 0U;
    }

    if (c3_send_at_expect(at_cmd, ">", 1200U) == 0U) {
        c3_log_resp("C3 CIPSEND FAIL: ");
        return 0U;
    }

#if C3_TCP_DEBUG
    debug_printf(USART0, "C3 TX: ");
    debug_printf(USART0, (char *)http_msg);
    if (http_msg[0] != '\0' && http_msg[strlen(http_msg) - 1U] != '\n') {
        debug_printf(USART0, "\r\n");
    }
#endif

    c3_uart_send_text(http_msg);
    c3_uart_read_response(g_resp_buffer, (uint16_t)sizeof(g_resp_buffer), wait_ms);
#if C3_TCP_DEBUG
    c3_log_resp("C3 RX: ");
#endif
    c3_close_tcp();
    return 1U;
}

#if !C3_SERVER_ACTIVE_QUERY
static void c3_probe_server_once(void)
{
    int req_len;

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
        debug_printf(USART0, "C3 PROBE BUILD FAIL\r\n");
        return;
    }

    if (c3_open_tcp() == 0U) {
        debug_printf(USART0, "C3 PROBE TCP FAIL\r\n");
        return;
    }

    if (c3_send_http_and_wait_response(g_http_buffer, C3_HTTP_WAIT_TIMEOUT_MS) == 0U) {
        debug_printf(USART0, "C3 PROBE SEND FAIL\r\n");
        return;
    }

    if (strstr(g_resp_buffer, "HTTP/1.1 200") != 0 || strstr(g_resp_buffer, "\"cmd\"") != 0) {
        debug_printf(USART0, "C3 PROBE OK\r\n");
    } else {
        debug_printf(USART0, "C3 PROBE RESP:\r\n");
        c3_log_resp(0);
    }
}
#endif

static wifi_cmd_t c3_parse_cmd_from_response(const char *response)
{
    if (response == 0) {
        return WIFI_CMD_NONE;
    }
    if (strstr(response, "\"cmd\":\"LED_ON\"") != 0 || strstr(response, "LED_ON") != 0) {
        return WIFI_CMD_LED_ON;
    }
    if (strstr(response, "\"cmd\":\"LED_OFF\"") != 0 || strstr(response, "LED_OFF") != 0) {
        return WIFI_CMD_LED_OFF;
    }
    if (strstr(response, "\"cmd\":\"FAN_ON\"") != 0 || strstr(response, "FAN_ON") != 0) {
        return WIFI_CMD_FAN_ON;
    }
    if (strstr(response, "\"cmd\":\"FAN_OFF\"") != 0 || strstr(response, "FAN_OFF") != 0) {
        return WIFI_CMD_FAN_OFF;
    }
    if (strstr(response, "\"cmd\":\"AUTO_MODE\"") != 0 || strstr(response, "AUTO_MODE") != 0) {
        return WIFI_CMD_AUTO_MODE;
    }
    if (strstr(response, "\"cmd\":\"MANUAL_MODE\"") != 0 || strstr(response, "MANUAL_MODE") != 0) {
        return WIFI_CMD_MANUAL_MODE;
    }
    if (strstr(response, "\"cmd\":\"READ_STATUS\"") != 0 || strstr(response, "READ_STATUS") != 0) {
        return WIFI_CMD_READ_STATUS;
    }
    return WIFI_CMD_NONE;
}

void Wifi_Init(void)
{
    uint8_t ok;

    g_wifi_ready = 0U;
    g_pending_cmd = WIFI_CMD_NONE;
    g_poll_div = 0U;
    g_report_div = 0U;
    g_reconnect_div = 0U;
    g_tcp_fail_count = 0U;

    c3_wait_ms(1500U);

    debug_printf(USART0, "C3 WIFI TARGET=");
    debug_printf(USART0, (char *)C3_WIFI_SSID);
    debug_printf(USART0, "\r\n");

    ok = c3_wait_at_ready();
    if (ok == 0U) {
        c3_log_resp("C3 AT FAIL: ");
        return;
    }

    c3_restore_module_config_once();

    (void)c3_send_at_expect("ATE0\r\n", "OK", 1000U);

    ok = c3_send_at_expect("AT+CWMODE=1\r\n", "OK", 1500U);
    if (ok == 0U) {
        c3_log_resp("C3 CWMODE FAIL: ");
        return;
    }

    c3_clear_previous_wifi();

    ok = c3_join_wifi();
    if (ok == 0U) {
        return;
    }

    ok = c3_verify_joined_ap();
    if (ok == 0U) {
        c3_clear_previous_wifi();
        ok = c3_join_wifi();
        if (ok == 0U || c3_verify_joined_ap() == 0U) {
            return;
        }
    }

    if (strstr(g_resp_buffer, "WIFI GOT IP") != 0) {
        debug_printf(USART0, "C3 WIFI CONNECTED\r\n");
    } else {
        debug_printf(USART0, "C3 WIFI JOINED\r\n");
    }

    (void)c3_send_at_expect("AT+CIPSTA?\r\n", "OK", 1500U);
    c3_log_resp("C3 IP INFO: ");

#if C3_SERVER_ACTIVE_QUERY
    ok = c3_send_at_expect("AT+CIPMUX=1\r\n", "OK", 1000U);
    if (ok == 0U) {
        c3_log_resp("C3 CIPMUX FAIL: ");
        return;
    }

    g_use_multi_conn = 1U;
    debug_printf(USART0, "C3 CIPMUX=1\r\n");

    (void)c3_send_at_expect("AT+CIPSERVER=0\r\n", "OK", 1000U);
    (void)snprintf(g_http_buffer, sizeof(g_http_buffer), "AT+CIPSERVER=1,%u\r\n", C3_SERVER_LISTEN_PORT);
    ok = c3_send_at_expect(g_http_buffer, "OK", 1000U);
    if (ok == 0U) {
        c3_log_resp("C3 CIPSERVER FAIL: ");
        return;
    }
    (void)snprintf(g_http_buffer, sizeof(g_http_buffer), "C3 LISTEN=%u\r\n", C3_SERVER_LISTEN_PORT);
    debug_printf(USART0, g_http_buffer);
#else
    ok = c3_send_at_expect("AT+CIPMUX=1\r\n", "OK", 1000U);
    if (ok != 0U) {
        g_use_multi_conn = 1U;
        debug_printf(USART0, "C3 CIPMUX=1\r\n");
    } else {
        ok = c3_send_at_expect("AT+CIPMUX=0\r\n", "OK", 1000U);
        if (ok != 0U) {
            g_use_multi_conn = 0U;
            debug_printf(USART0, "C3 CIPMUX=0\r\n");
        } else {
            c3_log_resp("C3 CIPMUX FAIL: ");
            return;
        }
    }

    debug_printf(USART0, "C3 SERVER=");
    debug_printf(USART0, (char *)C3_SERVER_HOST);
    debug_printf(USART0, "\r\n");
#endif
    g_wifi_ready = 1U;
#if !C3_SERVER_ACTIVE_QUERY
    c3_probe_server_once();
#endif
}

static void c3_try_reconnect(void)
{
    if (g_wifi_ready != 0U) {
        return;
    }

    g_reconnect_div++;
    if (g_reconnect_div < 25U) {
        return;
    }
    g_reconnect_div = 0U;
    debug_printf(USART0, "C3 RECONNECT...\r\n");
    Wifi_Init();
}

void Wifi_SendSensorData(float temperature, float humidity, uint16_t light, uint8_t fan_on, uint8_t led_on, uint8_t mode)
{
    int body_len;
    int req_len;
    char request_buffer[C3_RESP_BUFFER_SIZE];
    const char *fan_text = (fan_on != 0U) ? "on" : "off";
    const char *led_text = (led_on != 0U) ? "on" : "off";
    const char *mode_text = (mode == 0U) ? "auto" : "manual";

#if C3_SERVER_ACTIVE_QUERY
    g_status_cache.temperature = temperature;
    g_status_cache.humidity = humidity;
    g_status_cache.light = light;
    g_status_cache.fan_on = fan_on;
    g_status_cache.led_on = led_on;
    g_status_cache.mode = mode;
    g_status_cache.valid = 1U;
#endif

    if (g_wifi_ready == 0U) {
        c3_try_reconnect();
        return;
    }

#if C3_REPORT_TO_SERVER
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

    if (c3_open_tcp() == 0U) {
        return;
    }
    (void)c3_send_http_and_wait_response(request_buffer, C3_HTTP_WAIT_TIMEOUT_MS);
#endif
}

void Wifi_ReceiveCommand(void)
{
    int req_len;
    wifi_cmd_t cmd;

#if C3_SERVER_ACTIVE_QUERY
    if (g_wifi_ready == 0U) {
        c3_try_reconnect();
        return;
    }

    c3_process_server_request();
    return;
#else
    if (g_wifi_ready == 0U) {
        c3_try_reconnect();
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

    if (c3_open_tcp() == 0U) {
        return;
    }

    if (c3_send_http_and_wait_response(g_http_buffer, C3_HTTP_WAIT_TIMEOUT_MS) == 0U) {
        return;
    }

    cmd = c3_parse_cmd_from_response(g_resp_buffer);
    if (cmd != WIFI_CMD_NONE) {
        g_pending_cmd = cmd;
    }
#endif
}

wifi_cmd_t Wifi_GetPendingCommand(void)
{
    wifi_cmd_t cmd = g_pending_cmd;
    g_pending_cmd = WIFI_CMD_NONE;
    return cmd;
}
