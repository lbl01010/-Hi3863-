/**
 * Hi3861 开发板 HTTP Server 示例代码（参考实现）
 *
 * 功能：
 *   GET  /api/sensor        → 返回温湿度 JSON
 *   POST /api/alert         → 控制蜂鸣器 + LED
 *   POST /api/feed          → 控制投喂电机（舵机/步进）
 *   GET  /api/feeder/status → 返回喂食器状态
 *   GET  /api/location      → 返回 GPS 定位数据
 *
 * 行为：
 *   收到 alertOn  → 蜂鸣器响 10s 后自动停，LED 常亮
 *   收到 alertOff → LED 熄灭
 *   收到 feed     → 电机旋转对应圈数，落下宠粮
 *
 * 编译环境：HarmonyOS LiteOS / Hi3861 SDK
 */

#include <stdio.h>
#include <unistd.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "wifi_device.h"
#include "wifi_hotspot.h"
#include "lwip/netifapi.h"
#include "lwip/api.h"

// ==================== GPIO 引脚定义（根据实际接线修改） ====================
#define DHT11_PIN    5     // 温湿度传感器数据引脚
#define BUZZER_PIN   10    // 蜂鸣器
#define LED_PIN      12    // LED 指示灯
#define MOTOR_PIN    7     // 投喂电机控制（舵机/步进）
#define GPS_TX_PIN   13    // GPS 模块 TX
#define GPS_RX_PIN   14    // GPS 模块 RX

#define HTTP_PORT    8080
#define BUZZER_DURATION_MS  10000  // 蜂鸣器响 10 秒
#define MOTOR_STEP_MS       50     // 电机每步持续时间 ms

// ==================== 全局状态 ====================
static int g_ledOn = 0;           // LED 状态
static int g_foodRemaining = 2000; // 余粮 g
static char g_lastFeedTime[16] = "--";

// ==================== DHT11 温湿度读取（模拟） ====================
// 实际项目中替换为真实 DHT11 驱动读取
static float g_temperature = 26.5;
static float g_humidity = 55.0;

void read_dht11(float *temp, float *humi) {
    // TODO: 替换为真实 DHT11 驱动代码
    // 示例：模拟波动
    g_temperature += (float)(rand() % 10 - 5) / 10.0;
    if (g_temperature < 20) g_temperature = 20;
    if (g_temperature > 40) g_temperature = 40;

    g_humidity += (float)(rand() % 10 - 5) / 10.0;
    if (g_humidity < 30) g_humidity = 30;
    if (g_humidity > 90) g_humidity = 90;

    *temp = g_temperature;
    *humi = g_humidity;
}

// ==================== GPIO 控制 ====================
void gpio_init_all(void) {
    // TODO: 初始化 GPIO
    // IoTGpioInit(BUZZER_PIN);
    // IoTGpioSetFunc(BUZZER_PIN, IoTGpioFunc_GPIO);
    // IoTGpioSetDir(BUZZER_PIN, IoTGpioDir_OUT);
    // IoTGpioInit(LED_PIN);
    // IoTGpioSetFunc(LED_PIN, IoTGpioFunc_GPIO);
    // IoTGpioSetDir(LED_PIN, IoTGpioDir_OUT);
}

void buzzer_on(void) {
    // TODO: IoTGpioSetOutputVal(BUZZER_PIN, 1);
    printf("[BUZZER] ON\n");
}

void buzzer_off(void) {
    // TODO: IoTGpioSetOutputVal(BUZZER_PIN, 0);
    printf("[BUZZER] OFF\n");
}

void led_on(void) {
    // TODO: IoTGpioSetOutputVal(LED_PIN, 1);
    g_ledOn = 1;
    printf("[LED] ON\n");
}

void led_off(void) {
    // TODO: IoTGpioSetOutputVal(LED_PIN, 0);
    g_ledOn = 0;
    printf("[LED] OFF\n");
}

void motor_on(void) {
    // TODO: IoTGpioSetOutputVal(MOTOR_PIN, 1);
    printf("[MOTOR] ON\n");
}

void motor_off(void) {
    // TODO: IoTGpioSetOutputVal(MOTOR_PIN, 0);
    printf("[MOTOR] OFF\n");
}

// 投喂电机控制：amount 克 → 对应电机步数（每克约 2 步）
void feed_motor(int amount) {
    int steps = amount * 2;
    printf("[FEED] 投喂 %dg，电机旋转 %d 步\n", amount, steps);
    for (int i = 0; i < steps; i++) {
        motor_on();
        osDelay(MOTOR_STEP_MS / 10);
        motor_off();
        osDelay(MOTOR_STEP_MS / 10);
    }
    g_foodRemaining -= amount;
    if (g_foodRemaining < 0) g_foodRemaining = 0;
    // 记录投喂时间
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    snprintf(g_lastFeedTime, sizeof(g_lastFeedTime), "%02d:%02d",
             tm_info->tm_hour, tm_info->tm_min);
    printf("[FEED] 完成，余粮 %dg\n", g_foodRemaining);
}

// 蜂鸣器任务：响指定时间后自动停止
void buzzer_task(void) {
    buzzer_on();
    osDelay(BUZZER_DURATION_MS / 10);  // CMSIS-RTOS 延时（tick）
    buzzer_off();
}

// ==================== WiFi 连接 ====================
void connect_wifi(const char *ssid, const char *password) {
    // TODO: 使用 Hi3861 WiFi API 连接
    // WifiDeviceConfig config = {0};
    // strcpy(config.ssid, ssid);
    // strcpy(config.preSharedKey, password);
    // config.securityType = WIFI_SEC_TYPE_PSK;
    // EnableWifi();
    // ConnectToWifi(&config);
    printf("[WiFi] Connecting to %s ...\n", ssid);
}

// ==================== HTTP 响应构建 ====================
void build_json_response(char *buf, int buf_size, const char *json_body) {
    snprintf(buf, buf_size,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        (int)strlen(json_body), json_body);
}

void build_cors_preflight_response(char *buf, int buf_size) {
    snprintf(buf, buf_size,
        "HTTP/1.1 204 No Content\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n"
        "\r\n");
}

// ==================== HTTP 请求处理 ====================
void handle_http_request(struct netconn *conn) {
    struct netbuf *inbuf;
    char *request_buf;
    u16_t request_len;

    netconn_recv(conn, &inbuf);
    if (inbuf == NULL) {
        netconn_close(conn);
        return;
    }

    netbuf_data(inbuf, (void **)&request_buf, &request_len);
    char method[16] = {0};
    char path[64] = {0};

    sscanf(request_buf, "%s %s", method, path);

    char response[1024] = {0};

    // ============ CORS 预检 ============
    if (strcmp(method, "OPTIONS") == 0) {
        build_cors_preflight_response(response, sizeof(response));
        netconn_write(conn, response, strlen(response), NETCONN_COPY);
    }
    // ============ GET /api/sensor ============
    else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/sensor") == 0) {
        float temp, humi;
        read_dht11(&temp, &humi);

        char json[128];
        snprintf(json, sizeof(json),
            "{\"temperature\":%.1f,\"humidity\":%.1f}",
            temp, humi);

        build_json_response(response, sizeof(response), json);
        netconn_write(conn, response, strlen(response), NETCONN_COPY);
    }
    // ============ POST /api/alert ============
    else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/alert") == 0) {
        // 解析 JSON body
        char *body = strstr(request_buf, "\r\n\r\n");
        if (body) {
            body += 4;

            if (strstr(body, "\"on\"")) {
                // 收到告警：蜂鸣器响 10s + LED 常亮
                printf("[ALERT] 收到 alertOn 指令\n");
                led_on();
                // 蜂鸣器在独立任务中运行，10s 后自动停
                osThreadNew((osThreadFunc_t)buzzer_task, NULL, NULL);
            } else if (strstr(body, "\"off\"")) {
                // 取消告警：LED 熄灭
                printf("[ALERT] 收到 alertOff 指令\n");
                led_off();
            }
        }

        build_json_response(response, sizeof(response), "{\"result\":\"ok\"}");
        netconn_write(conn, response, strlen(response), NETCONN_COPY);
    }
    // ============ POST /api/feed ============
    else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/feed") == 0) {
        char *body = strstr(request_buf, "\r\n\r\n");
        int amount = 200; // 默认 200g
        if (body) {
            body += 4;
            // 解析 JSON: {"amount":200}
            char *p = strstr(body, "\"amount\"");
            if (p) {
                p = strstr(p, ":");
                if (p) amount = atoi(p + 1);
            }
        }
        printf("[FEED] 收到投喂指令: %dg\n", amount);
        feed_motor(amount);

        char json[128];
        snprintf(json, sizeof(json),
            "{\"result\":\"ok\",\"amount\":%d,\"remaining\":%d}",
            amount, g_foodRemaining);
        build_json_response(response, sizeof(response), json);
        netconn_write(conn, response, strlen(response), NETCONN_COPY);
    }
    // ============ GET /api/feeder/status ============
    else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/feeder/status") == 0) {
        char json[128];
        snprintf(json, sizeof(json),
            "{\"connected\":true,\"battery\":85,\"foodRemaining\":%d,\"lastFeedTime\":\"%s\"}",
            g_foodRemaining, g_lastFeedTime);
        build_json_response(response, sizeof(response), json);
        netconn_write(conn, response, strlen(response), NETCONN_COPY);
    }
    // ============ GET /api/location ============
    else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/location") == 0) {
        // 模拟 GPS 数据（实际项目替换为 GPS 模块 NMEA 解析）
        char json[128];
        snprintf(json, sizeof(json),
            "{\"latitude\":31.2304,\"longitude\":121.4737,\"distanceFromHome\":0.3,\"isMoving\":true,\"updateTime\":\"%s\"}",
            "10:30");
        build_json_response(response, sizeof(response), json);
        netconn_write(conn, response, strlen(response), NETCONN_COPY);
    }
    // ============ 404 ============
    else {
        snprintf(response, sizeof(response),
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Not Found");
        netconn_write(conn, response, strlen(response), NETCONN_COPY);
    }

    netbuf_delete(inbuf);
    netconn_close(conn);
}

// ==================== HTTP Server 主循环 ====================
void http_server_task(void *arg) {
    (void)arg;

    struct netconn *listener = netconn_new(NETCONN_TCP);
    netconn_bind(listener, IP_ADDR_ANY, HTTP_PORT);
    netconn_listen(listener);

    printf("[HTTP] Server started on port %d\n", HTTP_PORT);

    while (1) {
        struct netconn *new_conn;
        err_t err = netconn_accept(listener, &new_conn);
        if (err == ERR_OK) {
            handle_http_request(new_conn);
            netconn_delete(new_conn);
        }
    }
}

// ==================== 入口 ====================
void pet_monitor_main(void) {
    printf("\n=== 宠物关怀系统 - Hi3861 端启动 ===\n");

    gpio_init_all();

    // 连接 WiFi（替换为实际 SSID 和密码）
    connect_wifi("YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD");

    // 启动 HTTP Server 任务
    osThreadNew(http_server_task, NULL, NULL);

    printf("[System] 初始化完成，等待 App 连接...\n");
}

// HarmonyOS Lite 自动调用入口
SYS_RUN(pet_monitor_main);