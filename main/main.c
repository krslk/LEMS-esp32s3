#include "headfile.h"
#include "icon_codeing.h"

uint8_t gTag_mqttIsConnected = 0;
uint8_t gTag_wifiIsConnected = 0;
uint8_t gSensorCollect_humiBodyInFrared = 0;
uint8_t gTag_cardPresent = 0;   // 卡片存在标志
uint8_t gTag_emergencyMode = 0; // 紧急模式标志
uint8_t gFlag_forceUpload = 0;  // 强制立即上报标志（异常、刷卡、移卡时触发）
char gJson_DataUpLoad[512];
char gCardUsername[32] = {0}; // 存储卡片用户名字符串
httpd_handle_t g_httpd_server = NULL;

EventGroupHandle_t gHandle_wifiEventGroup;
esp_mqtt_client_handle_t gHandle_mqttClient;
rc522_driver_handle_t gHandle_rc522Driver;
rc522_handle_t gHandle_rc522Scanner;
ssd1306_handle_t gSsd_handle;
QueueHandle_t queue_SensorData;

// 函数前置声明
void Task_ReadSensorValue(void *para);
void Task_Display(void *para);
void control_all_relays(uint8_t enable);
void emergency_power_off(const char *reason);
void remote_power_control(uint8_t enable);
AlertType detect_sensor_anomalies(WorkstationStatus *status);

/**
 * @brief 获取告警类型的描述字符串
 */
const char *get_alert_type_string(AlertType type)
{
  switch (type)
  {
  case ALERT_FIRE_EMERGENCY:
    return "FIRE_EMERGENCY";
  case ALERT_SMOKE_EMERGENCY:
    return "SMOKE_EMERGENCY";
  case ALERT_TEMP_EMERGENCY:
    return "TEMP_EMERGENCY";
  case ALERT_HEAT_EMERGENCY:
    return "HEAT_EMERGENCY";
  case ALERT_FIRE_WARNING:
    return "FIRE_WARNING";
  case ALERT_SMOKE_WARNING:
    return "SMOKE_WARNING";
  case ALERT_TEMP_HIGH_WARNING:
    return "TEMP_HIGH_WARNING";
  case ALERT_TEMP_LOW_WARNING:
    return "TEMP_LOW_WARNING";
  case ALERT_HUMIDITY_HIGH_WARNING:
    return "HUMIDITY_HIGH_WARNING";
  case ALERT_HUMIDITY_LOW_WARNING:
    return "HUMIDITY_LOW_WARNING";
  default:
    return "UNKNOWN";
  }
}

/**
 * @brief 电源控制接口
 * @param status：0=关闭（关闭所有设备），1=开启（清除紧急模式+开启电源）
 * @响应：JSON格式，包含status（success/error）和message
 */
static esp_err_t power_handler(httpd_req_t *req)
{
  char buf[100] = {0};
  int status = -1;

  // 解析URL中的status参数
  if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK)
  {
    httpd_query_key_value(buf, "status", buf, sizeof(buf)); // 提取status值
    status = atoi(buf);                                     // 转为整数
  }

  // 校验参数合法性
  if (status != 0 && status != 1)
  {
    const char *resp = "{\"status\":\"error\",\"message\":\"Invalid status! Use 0(off) or 1(on)\"}";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    ESP_LOGE(TAG_HTTP, "Power request: Invalid status=%d", status);
    return ESP_OK;
  }

  // 执行电源控制
  if (status == 1)
  {
    // 开启电源：清除紧急模式+设置管理员用户名
    if (gTag_emergencyMode)
    {
      ESP_LOGW(TAG_HTTP, "Clear emergency mode for power on");
      gTag_emergencyMode = 0;
    }
    if (strcmp(g_CurrentWorkStaStatus.curUsername, "null") == 0)
    {
      strcpy(g_CurrentWorkStaStatus.curUsername, "0000000001"); // 管理员标识
      strcpy(gCardUsername, "0000000001");
    }
    gpio_set_level(SET_POWER_STATUS_GPIO, 1);
    g_CurrentWorkStaStatus.powerStatus = 1;
    // 返回成功响应
    const char *resp = "{\"status\":\"success\",\"message\":\"Power turned ON\"}";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG_HTTP, "Power request: Turned ON");
  }
  else
  {
    // 关闭电源：关闭所有继电器（复用原有control_all_relays函数）
    control_all_relays(0);
    // 重置用户名为null
    strcpy(g_CurrentWorkStaStatus.curUsername, "null");
    strcpy(gCardUsername, "null");

    // 返回成功响应
    const char *resp = "{\"status\":\"success\",\"message\":\"Power turned OFF\"}";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG_HTTP, "Power request: Turned OFF");
  }
  gFlag_forceUpload = 1;
  return ESP_OK;
}

/**
 * @brief 灯光控制接口
 * @param status：0=关闭，1=开启
 */
static esp_err_t light_handler(httpd_req_t *req)
{
  char buf[100] = {0};
  int status = -1;

  // 解析参数
  if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK)
  {
    httpd_query_key_value(buf, "status", buf, sizeof(buf));
    status = atoi(buf);
  }

  // 校验参数
  if (status != 0 && status != 1)
  {
    const char *resp = "{\"status\":\"error\",\"message\":\"Invalid status! Use 0(off) or 1(on)\"}";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    ESP_LOGE(TAG_HTTP, "Light request: Invalid status=%d", status);
    return ESP_OK;
  }

  // 紧急模式拦截（不允许控制）
  if (gTag_emergencyMode)
  {
    const char *resp = "{\"status\":\"error\",\"message\":\"Emergency mode! Cannot control light\"}";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    ESP_LOGE(TAG_HTTP, "Light request: Blocked (emergency mode)");
    return ESP_OK;
  }

  // 执行灯光控制
  gpio_set_level(SET_LIGHT_CONTROL_GPIO, status);
  g_CurrentWorkStaStatus.lightStatus = status;

  // 返回响应
  char resp[100];
  snprintf(resp, sizeof(resp), "{\"status\":\"success\",\"message\":\"Light turned %s\"}",
           status ? "ON" : "OFF");
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  ESP_LOGI(TAG_HTTP, "Light request: Turned %s", status ? "ON" : "OFF");
  gFlag_forceUpload = 1;
  return ESP_OK;
}

/**
 * @brief 继电器1控制接口
 * @param status：0=关闭，1=开启（仅在非紧急模式下生效）
 */
static esp_err_t relay1_handler(httpd_req_t *req)
{
  char buf[100] = {0};
  int status = -1;

  // 解析参数
  if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK)
  {
    httpd_query_key_value(buf, "status", buf, sizeof(buf));
    status = atoi(buf);
  }

  // 校验参数
  if (status != 0 && status != 1)
  {
    const char *resp = "{\"status\":\"error\",\"message\":\"Invalid status! Use 0(off) or 1(on)\"}";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    ESP_LOGE(TAG_HTTP, "Relay1 request: Invalid status=%d", status);
    return ESP_OK;
  }

  // 紧急模式拦截
  if (gTag_emergencyMode)
  {
    const char *resp = "{\"status\":\"error\",\"message\":\"Emergency mode! Cannot control relay1\"}";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    ESP_LOGE(TAG_HTTP, "Relay1 request: Blocked (emergency mode)");
    return ESP_OK;
  }

  // 执行继电器1控制
  gpio_set_level(SET_RELAY_NUM1_GPIO, status);
  g_CurrentWorkStaStatus.relayNum1Status = status;

  // 返回响应
  char resp[100];
  snprintf(resp, sizeof(resp), "{\"status\":\"success\",\"message\":\"Relay1 turned %s\"}",
           status ? "ON" : "OFF");
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  ESP_LOGI(TAG_HTTP, "Relay1 request: Turned %s", status ? "ON" : "OFF");
  gFlag_forceUpload = 1;
  return ESP_OK;
}

/**
 * @brief 继电器2控制接口
 * @param status：0=关闭，1=开启（仅在非紧急模式下生效）
 */
static esp_err_t relay2_handler(httpd_req_t *req)
{
  char buf[100] = {0};
  int status = -1;

  // 解析参数
  if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK)
  {
    httpd_query_key_value(buf, "status", buf, sizeof(buf));
    status = atoi(buf);
  }

  // 校验参数
  if (status != 0 && status != 1)
  {
    const char *resp = "{\"status\":\"error\",\"message\":\"Invalid status! Use 0(off) or 1(on)\"}";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    ESP_LOGE(TAG_HTTP, "Relay2 request: Invalid status=%d", status);
    return ESP_OK;
  }

  // 紧急模式拦截
  if (gTag_emergencyMode)
  {
    const char *resp = "{\"status\":\"error\",\"message\":\"Emergency mode! Cannot control relay2\"}";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    ESP_LOGE(TAG_HTTP, "Relay2 request: Blocked (emergency mode)");
    return ESP_OK;
  }

  // 执行继电器2控制
  gpio_set_level(SET_RELAY_NUM2_GPIO, status);
  g_CurrentWorkStaStatus.relayNum2Status = status;

  // 返回响应
  char resp[100];
  snprintf(resp, sizeof(resp), "{\"status\":\"success\",\"message\":\"Relay2 turned %s\"}",
           status ? "ON" : "OFF");
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  ESP_LOGI(TAG_HTTP, "Relay2 request: Turned %s", status ? "ON" : "OFF");
  gFlag_forceUpload = 1;
  return ESP_OK;
}

// 定义URI与处理函数的映射关系
static const httpd_uri_t power_uri = {
    .uri = "/power",          // 接口路径
    .method = HTTP_GET,       // 请求方法
    .handler = power_handler, // 处理函数
    .user_ctx = NULL};

static const httpd_uri_t light_uri = {
    .uri = "/light",
    .method = HTTP_GET,
    .handler = light_handler,
    .user_ctx = NULL};

static const httpd_uri_t relay1_uri = {
    .uri = "/relay1",
    .method = HTTP_GET,
    .handler = relay1_handler,
    .user_ctx = NULL};

static const httpd_uri_t relay2_uri = {
    .uri = "/relay2",
    .method = HTTP_GET,
    .handler = relay2_handler,
    .user_ctx = NULL};

/**
 * @brief 启动HTTP服务器（端口80）
 */
static esp_err_t httpd_start_server(void)
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  // 1. 启动服务器
  if (httpd_start(&g_httpd_server, &config) != ESP_OK)
  {
    ESP_LOGE(TAG_HTTP, "Failed to start HTTP server");
    return ESP_FAIL;
  }

  // 2. 注册所有URI接口
  httpd_register_uri_handler(g_httpd_server, &power_uri);
  httpd_register_uri_handler(g_httpd_server, &light_uri);
  httpd_register_uri_handler(g_httpd_server, &relay1_uri);
  httpd_register_uri_handler(g_httpd_server, &relay2_uri);

  ESP_LOGI(TAG_HTTP, "HTTP server started on port %d", config.server_port);
  return ESP_OK;
}

/**
 * @brief 检测传感器数据异常并触发相应处理
 * @param status 工位状态数据
 * @return 检测到的最高优先级告警类型
 */
AlertType detect_sensor_anomalies(WorkstationStatus *status)
{
  AlertType detected_alert = ALERT_NONE;

  // ========== 第一优先级：紧急异常检测 ==========

  // 如果已经处于紧急模式，不再检测新的紧急情况
  if (gTag_emergencyMode)
  {
    ESP_LOGD(TAG_ALERT, "Already in emergency mode, skipping anomaly detection");
    return ALERT_NONE;
  }

  // 火焰紧急检测
  if (status->flameScope > THRESHOLD_FLAME_EMERGENCY)
  {
    ESP_LOGE(TAG_ALERT, "[FIRE EMERGENCY] Flame: %.1f%% (Threshold: %.1f%%)",
             status->flameScope, THRESHOLD_FLAME_EMERGENCY);

#if ENABLE_AUTO_POWER_OFF
    ESP_LOGE(TAG_ALERT, "Auto power-off triggered by fire emergency!");
    // 调用紧急断电接口，发送EmergencyPowerOff事件
    char reason[64];
    snprintf(reason, sizeof(reason), "Fire emergency: %.1f%% (threshold: %.1f%%)",
             status->flameScope, THRESHOLD_FLAME_EMERGENCY);
    emergency_power_off(reason);

    // 额外发送AlertEmergency事件
    if (gTag_mqttIsConnected)
    {
      send_alert_mqtt(ALERT_FIRE_EMERGENCY, status->flameScope,
                      THRESHOLD_FLAME_EMERGENCY, 1);
    }
#else
    ESP_LOGW(TAG_ALERT, "Development mode: Power-off disabled, only alerting");
#endif

    return ALERT_FIRE_EMERGENCY;
  }

  // 烟雾紧急检测
  if (status->smokeScope > THRESHOLD_SMOKE_EMERGENCY)
  {
    ESP_LOGE(TAG_ALERT, "[SMOKE EMERGENCY] Smoke: %.1f%% (Threshold: %.1f%%)",
             status->smokeScope, THRESHOLD_SMOKE_EMERGENCY);

#if ENABLE_AUTO_POWER_OFF
    ESP_LOGE(TAG_ALERT, "Auto power-off triggered by smoke emergency!");
    // 调用紧急断电接口，发送EmergencyPowerOff事件
    char reason[64];
    snprintf(reason, sizeof(reason), "Smoke emergency: %.1f%% (threshold: %.1f%%)",
             status->smokeScope, THRESHOLD_SMOKE_EMERGENCY);
    emergency_power_off(reason);

    // 额外发送AlertEmergency事件
    if (gTag_mqttIsConnected)
    {
      send_alert_mqtt(ALERT_SMOKE_EMERGENCY, status->smokeScope,
                      THRESHOLD_SMOKE_EMERGENCY, 1);
    }
#else
    ESP_LOGW(TAG_ALERT, "Development mode: Power-off disabled, only alerting");
#endif

    return ALERT_SMOKE_EMERGENCY;
  }

  // 高温紧急检测
  if (status->temperature > THRESHOLD_TEMP_EMERGENCY)
  {
    ESP_LOGE(TAG_ALERT,
             "[TEMPERATURE EMERGENCY] Temp: %.2f C (Threshold: %.1f C)",
             status->temperature, THRESHOLD_TEMP_EMERGENCY);

#if ENABLE_AUTO_POWER_OFF
    ESP_LOGE(TAG_ALERT, "Auto power-off triggered by temperature emergency!");
    // 调用紧急断电接口，发送EmergencyPowerOff事件
    char reason[64];
    snprintf(reason, sizeof(reason), "Temperature emergency: %.2f C (threshold: %.1f C)",
             status->temperature, THRESHOLD_TEMP_EMERGENCY);
    emergency_power_off(reason);

    // 额外发送AlertEmergency事件
    if (gTag_mqttIsConnected)
    {
      send_alert_mqtt(ALERT_TEMP_EMERGENCY, status->temperature,
                      THRESHOLD_TEMP_EMERGENCY, 1);
    }
#else
    ESP_LOGW(TAG_ALERT, "Development mode: Power-off disabled, only alerting");
#endif

    return ALERT_TEMP_EMERGENCY;
  }

  // 热度紧急检测
  if (status->heatScope > THRESHOLD_HEAT_EMERGENCY)
  {
    ESP_LOGE(TAG_ALERT, "[HEAT EMERGENCY] Heat: %.1f%% (Threshold: %.1f%%)",
             status->heatScope, THRESHOLD_HEAT_EMERGENCY);

#if ENABLE_AUTO_POWER_OFF
    ESP_LOGE(TAG_ALERT, "Auto power-off triggered by heat emergency!");
    // 调用紧急断电接口，发送EmergencyPowerOff事件
    char reason[64];
    snprintf(reason, sizeof(reason), "Heat emergency: %.1f%% (threshold: %.1f%%)",
             status->heatScope, THRESHOLD_HEAT_EMERGENCY);
    emergency_power_off(reason);

    // 额外发送AlertEmergency事件
    if (gTag_mqttIsConnected)
    {
      send_alert_mqtt(ALERT_HEAT_EMERGENCY, status->heatScope,
                      THRESHOLD_HEAT_EMERGENCY, 1);
    }
#else
    ESP_LOGW(TAG_ALERT, "Development mode: Power-off disabled, only alerting");
#endif

    return ALERT_HEAT_EMERGENCY;
  }

  // ========== 第二优先级：一般告警（仅上报，不切断电源）==========

  // 火焰告警
  if (status->flameScope > THRESHOLD_FLAME_WARNING)
  {
    ESP_LOGW(TAG_ALERT, "[WARNING] Fire: %.1f%% (Threshold: %.1f%%)",
             status->flameScope, THRESHOLD_FLAME_WARNING);

    send_alert_mqtt(ALERT_FIRE_WARNING, status->flameScope,
                    THRESHOLD_FLAME_WARNING, 0);

    gFlag_forceUpload = 1;
    detected_alert = ALERT_FIRE_WARNING;
  }

  // 烟雾告警
  if (status->smokeScope > THRESHOLD_SMOKE_WARNING)
  {
    ESP_LOGW(TAG_ALERT, "[WARNING] Smoke: %.1f%% (Threshold: %.1f%%)",
             status->smokeScope, THRESHOLD_SMOKE_WARNING);

    send_alert_mqtt(ALERT_SMOKE_WARNING, status->smokeScope,
                    THRESHOLD_SMOKE_WARNING, 0);

    gFlag_forceUpload = 1;
    if (detected_alert == ALERT_NONE)
    {
      detected_alert = ALERT_SMOKE_WARNING;
    }
  }

  // 高温告警
  if (status->temperature > THRESHOLD_TEMP_HIGH)
  {
    ESP_LOGW(TAG_ALERT,
             "[WARNING] High temperature: %.2f C (Threshold: %.1f C)",
             status->temperature, THRESHOLD_TEMP_HIGH);

    send_alert_mqtt(ALERT_TEMP_HIGH_WARNING, status->temperature,
                    THRESHOLD_TEMP_HIGH, 0);

    gFlag_forceUpload = 1;
    if (detected_alert == ALERT_NONE)
    {
      detected_alert = ALERT_TEMP_HIGH_WARNING;
    }
  }

  // 低温告警
  if (status->temperature < THRESHOLD_TEMP_LOW && status->temperature > 0)
  {
    ESP_LOGW(TAG_ALERT, "[WARNING] Low temperature: %.2f C (Threshold: %.1f C)",
             status->temperature, THRESHOLD_TEMP_LOW);

    send_alert_mqtt(ALERT_TEMP_LOW_WARNING, status->temperature,
                    THRESHOLD_TEMP_LOW, 0);

    gFlag_forceUpload = 1;
    if (detected_alert == ALERT_NONE)
    {
      detected_alert = ALERT_TEMP_LOW_WARNING;
    }
  }

  // 高湿度告警
  if (status->humidity > THRESHOLD_HUMIDITY_HIGH)
  {
    ESP_LOGW(TAG_ALERT, "[WARNING] High humidity: %.1f%% (Threshold: %.1f%%)",
             status->humidity, THRESHOLD_HUMIDITY_HIGH);

    send_alert_mqtt(ALERT_HUMIDITY_HIGH_WARNING, status->humidity,
                    THRESHOLD_HUMIDITY_HIGH, 0);

    gFlag_forceUpload = 1;
    if (detected_alert == ALERT_NONE)
    {
      detected_alert = ALERT_HUMIDITY_HIGH_WARNING;
    }
  }

  // 低湿度告警
  if (status->humidity < THRESHOLD_HUMIDITY_LOW && status->humidity > 0)
  {
    ESP_LOGW(TAG_ALERT, "[WARNING] Low humidity: %.1f%% (Threshold: %.1f%%)",
             status->humidity, THRESHOLD_HUMIDITY_LOW);

    send_alert_mqtt(ALERT_HUMIDITY_LOW_WARNING, status->humidity,
                    THRESHOLD_HUMIDITY_LOW, 0);

    gFlag_forceUpload = 1;
    if (detected_alert == ALERT_NONE)
    {
      detected_alert = ALERT_HUMIDITY_LOW_WARNING;
    }
  }

  return detected_alert;
}

/**
 * @brief 继电器和电源控制函数
 * @param enable 1=开启，0=关闭
 */
void control_all_relays(uint8_t enable)
{
  gpio_set_level(SET_POWER_STATUS_GPIO, enable);
  gpio_set_level(SET_RELAY_NUM1_GPIO, enable);
  gpio_set_level(SET_RELAY_NUM2_GPIO, enable);
  gpio_set_level(SET_LIGHT_CONTROL_GPIO, enable);

  // 更新工位状态
  g_CurrentWorkStaStatus.powerStatus = enable;
  g_CurrentWorkStaStatus.relayNum1Status = enable;
  g_CurrentWorkStaStatus.relayNum2Status = enable;
  g_CurrentWorkStaStatus.lightStatus = enable;

  ESP_LOGI(TAG_ALERT, "All relays %s", enable ? "ON" : "OFF");
}

/**
 * @brief 从JSON字符串中提取指定键的整数值
 * @param json_str JSON字符串
 * @param key 要提取的键名
 * @return 提取的整数值，如果失败返回-1
 */
int extract_json_value(const char *json_str, const char *key)
{
  char search_pattern[128];
  snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);

  char *key_pos = strstr(json_str, search_pattern);
  if (key_pos == NULL)
  {
    return -1;
  }

  // 跳过键名和冒号，找到值
  char *value_pos = key_pos + strlen(search_pattern);

  // 跳过空白字符
  while (*value_pos == ' ' || *value_pos == '\t')
  {
    value_pos++;
  }

  // 解析数值
  if (*value_pos >= '0' && *value_pos <= '9')
  {
    return atoi(value_pos);
  }

  return -1;
}

/**
 * @brief 只开启电源，不开启其他设备
 * @note 刷卡后只开启电源，其他设备保持关闭直到有事件触发
 */
void enable_power_only(void)
{
  // 先确保其他设备处于关闭状态
  gpio_set_level(SET_RELAY_NUM1_GPIO, 0);
  gpio_set_level(SET_RELAY_NUM2_GPIO, 0);
  gpio_set_level(SET_LIGHT_CONTROL_GPIO, 0);

  // 开启电源
  gpio_set_level(SET_POWER_STATUS_GPIO, 1);

  // 只更新电源状态，其他设备状态保持为0
  g_CurrentWorkStaStatus.powerStatus = 1;
  // 其他设备状态保持为0（已在初始化时设置为0）

  ESP_LOGI(TAG_ALERT, "Power enabled, other devices remain OFF until triggered");
}

/**
 * @brief 紧急断电接口
 * @note 此函数提供了一个独立的紧急断电接口，可以被：
 *       1. 本地边缘检测调用
 *       2. MQTT远程控制指令调用
 *       3. 其他安全机制调用
 */
void emergency_power_off(const char *reason)
{
  ESP_LOGE(TAG_ALERT, "========================================");
  ESP_LOGE(TAG_ALERT, "EMERGENCY POWER OFF TRIGGERED!");
  ESP_LOGE(TAG_ALERT, "Reason: %s", reason ? reason : "Unknown");
  ESP_LOGE(TAG_ALERT, "========================================");

  // 立即切断所有电源和继电器
  control_all_relays(0);
  gTag_emergencyMode = 1;

  // 发送紧急断电事件到云端（符合OneNet事件格式）
  if (gTag_mqttIsConnected)
  {
    char msg[256];
    snprintf(msg, sizeof(msg),
             "{\"id\":\"123\",\"version\":\"1.0\",\"params\":{"
             "\"EmergencyPowerOff\":{\"value\":{"
             "\"reason\":\"%s\",\"username\":\"%s\""
             "}}}}",
             reason ? reason : "Unknown",
             g_CurrentWorkStaStatus.curUsername);

    // OneNet标准事件上报主题
    int json_len = esp_mqtt_client_publish(gHandle_mqttClient,
                                           "$sys/3HSBa0ZB1R/ESP_01/thing/event/post", msg, 0, 1, 0);
    if (json_len > 0)
    {
      ESP_LOGW(TAG_ALERT, "Sending EmergencyPowerOff event to OneNet");
      ESP_LOGW(TAG_ALERT, "Event JSON: %s", msg);
    }
  }

  // 紧急断电后不需要触发数据上报，因为已经发送了紧急事件
  // gFlag_forceUpload保持为0，避免发送其他消息
}

/**
 * @brief 远程电源控制接口
 * @param enable 1=开启，0=关闭
 * @note 此函数预留给后续MQTT消息处理使用
 */
void remote_power_control(uint8_t enable)
{
  ESP_LOGI(TAG_ALERT, "Remote power control: %s", enable ? "ON" : "OFF");

  if (enable)
  {
    // 开启电源前检查是否处于紧急模式
    if (gTag_emergencyMode)
    {
      ESP_LOGW(TAG_ALERT, "Device in emergency mode, clearing emergency flag");
      gTag_emergencyMode = 0;
    }
    gpio_set_level(SET_POWER_STATUS_GPIO, 1);
    // 电源开启时关闭灯（自动控制会在需要时开启）
    gpio_set_level(SET_LIGHT_CONTROL_GPIO, 0);
    g_CurrentWorkStaStatus.lightStatus = 0;
  }
  else
  {
    control_all_relays(0);
  }
}

// ADC 初始化函数：配置四个传感器通道
void adc_sensor_init(void)
{
  // 配置引脚为输入模式
  ESP_ERROR_CHECK(gpio_config(&(gpio_config_t){
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
      .pin_bit_mask = (1ULL << SENSOT_HEAT_FIGURE_GPIO) |
                      (1ULL << SENSOT_LIGHT_FIGURE_GPIO) |
                      (1ULL << SENSOT_SMOKE_FIGURE_GPIO) |
                      (1ULL << SENSOT_HUMAN_BODY_INFRARED_FIGURE_GPIO) |
                      (1ULL << SENSOT_FLAME_FIGURE_GPIO)}));
  // 配置 ADC1 全局参数（精度+衰减）
  adc1_config_width(ADC_WIDTH); // 采样精度12位
}

void adc_read_all_sensors(WorkstationStatus *status)
{
  gSensorCollect_humiBodyInFrared = gpio_get_level(SENSOT_HUMAN_BODY_INFRARED_FIGURE_GPIO);
  // 读取 ADC 原始值
  status->heatAdcRaw = adc1_get_raw(HEAT_ADC_CHANNEL);
  status->lightAdcRaw = adc1_get_raw(LIGHT_ADC_CHANNEL);
  status->flameAdcRaw = adc1_get_raw(FLAME_ADC_CHANNEL);
  status->smokeAdcRaw = adc1_get_raw(SMOKE_ADC_CHANNEL);

  // 计算电压值
  float voltage_scale = 3.3f / 4095.0f;
  status->heatVoltage = status->heatAdcRaw * voltage_scale;
  status->lightVoltage = status->lightAdcRaw * voltage_scale;
  status->flameVoltage = status->flameAdcRaw * voltage_scale;
  status->smokeVoltage = status->smokeAdcRaw * voltage_scale;

  // --- 光敏传感器 (Light Intensity) ---
  // 特性：光照越强，电压越低。调整后：
  // - 强光照射时 (100%) 输出 ~0.3V
  // - 完全黑暗时 (0%) 输出 ~3.0V
  const float light_V_min = 0.0f; // 强光电压 (对应100%)
  const float light_V_max = 3.3f; // 黑暗电压 (对应0%)
  // 反转逻辑：电压越低，光照越强，百分比越大
  status->lightIntensity = (status->lightVoltage - light_V_min) /
                           (light_V_max - light_V_min) * 100.0f;

  const float smoke_V_min = 0.0f; // 洁净空气电压 (对应0%)
  const float smoke_V_max = 3.3f; // 报警阈值电压 (对应100%)
  status->smokeScope = (status->smokeVoltage - smoke_V_min) /
                       (smoke_V_max - smoke_V_min) * 100.0f;

  const float flame_V_min = 0.0f; // 无火焰电压 (对应0%)
  const float flame_V_max = 3.3f; // 火焰接近电压 (对应100%)
  status->flameScope = (status->flameVoltage - flame_V_min) /
                       (flame_V_max - flame_V_min) * 100.0f;

  const float heat_V_min = 0.0f; // 室温电压 (对应0%)
  const float heat_V_max = 3.3f; // 高温电压 (对应100%)
  status->heatScope =
      (status->heatVoltage - heat_V_min) / (heat_V_max - heat_V_min) * 100.0f;
}

void Task_ReadSensorValue(void *para)
{
  adc_sensor_init();
  ESP_ERROR_CHECK(
      gpio_set_pull_mode(SENSOT_TEMP_AND_HUMI_DATA_GPIO, GPIO_PULLUP_ONLY));

  ESP_LOGI(TAG_ALERT, "=== Sensor initialization completed ===");
  ESP_LOGI(TAG_ALERT, "Emergency Thresholds:");
  ESP_LOGI(TAG_ALERT,
           "  - Flame > %.1f%%, Smoke > %.1f%%, Temp > %.1f C, Heat > %.1f%%",
           THRESHOLD_FLAME_EMERGENCY, THRESHOLD_SMOKE_EMERGENCY,
           THRESHOLD_TEMP_EMERGENCY, THRESHOLD_HEAT_EMERGENCY);
  // 上报计数器（每5秒+1，达到12次=60秒时上报）
  uint8_t upload_counter = 0;
  // 基础传感器数据上报计数器（每5秒+1，达到6次=30秒时上报，未刷卡时使用）
  uint8_t basic_upload_counter = 0;

  while (1)
  {
    // 读取所有传感器数据
    adc_read_all_sensors(&g_CurrentWorkStaStatus);
    dht_read_float_data(DHT_TYPE_DHT11, SENSOT_TEMP_AND_HUMI_DATA_GPIO,
                        &(g_CurrentWorkStaStatus.humidity),
                        &(g_CurrentWorkStaStatus.temperature));

    // 打印原始传感器数据
    ESP_LOGI(ENV_LOG,
             "ADC原始/电压：火焰 %4d/%5.2fV | 光照 %4d/%5.2fV | 烟雾 "
             "%4d/%5.2fV | 热度 %4d/%5.2fV",
             g_CurrentWorkStaStatus.flameAdcRaw,  // 火焰ADC原始值
             g_CurrentWorkStaStatus.flameVoltage, // 火焰电压值
             g_CurrentWorkStaStatus.lightAdcRaw,  // 光敏ADC原始值
             g_CurrentWorkStaStatus.lightVoltage, // 光敏电压值
             g_CurrentWorkStaStatus.smokeAdcRaw,  // 烟雾ADC原始值
             g_CurrentWorkStaStatus.smokeVoltage, // 烟雾电压值
             g_CurrentWorkStaStatus.heatAdcRaw,   // 热敏ADC原始值
             g_CurrentWorkStaStatus.heatVoltage); // 热敏电压值

    ESP_LOGI(ENV_LOG,
             "温湿度：湿度 %.1lf%% | 温度 %.2lf℃ | 火焰 %.0f | 光照 %.0f | "
             "烟雾 %.0f | 热度 %.0f | 人体红外：%u",
             g_CurrentWorkStaStatus.humidity,       // 湿度
             g_CurrentWorkStaStatus.temperature,    // 温度
             g_CurrentWorkStaStatus.flameScope,     // 火焰
             g_CurrentWorkStaStatus.lightIntensity, // 光照
             g_CurrentWorkStaStatus.smokeScope,     // 烟雾
             g_CurrentWorkStaStatus.heatScope,      // 热度
             gSensorCollect_humiBodyInFrared        // 人体红外
    );

    // ========== 边缘异常检测 ==========
    AlertType alert = detect_sensor_anomalies(&g_CurrentWorkStaStatus);

    // ========== 自动控制逻辑 ==========
    // 只有在电源开启且非紧急模式下才执行自动控制
    if (g_CurrentWorkStaStatus.powerStatus == 1 && gTag_emergencyMode == 0)
    {
      ESP_LOGI(TAG_ALERT, "Auto control check - Power: %d, Emergency: %d, Light: %.1f%%, Smoke: %.1f%%",
               g_CurrentWorkStaStatus.powerStatus, gTag_emergencyMode,
               g_CurrentWorkStaStatus.lightIntensity, g_CurrentWorkStaStatus.smokeScope);

      // 光敏传感器自动控制灯
      if (g_CurrentWorkStaStatus.lightIntensity < THRESHOLD_LIGHT_AUTO_ON)
      {
        gpio_set_level(SET_LIGHT_CONTROL_GPIO, 1);
        g_CurrentWorkStaStatus.lightStatus = 1;
        ESP_LOGI(TAG_ALERT, "Auto light control: ON (Light intensity: %.1f%%, threshold: %.1f%%)",
                 g_CurrentWorkStaStatus.lightIntensity, THRESHOLD_LIGHT_AUTO_ON);
      }
      else
      {
        gpio_set_level(SET_LIGHT_CONTROL_GPIO, 0);
        g_CurrentWorkStaStatus.lightStatus = 0;
        ESP_LOGI(TAG_ALERT, "Auto light control: OFF (Light intensity: %.1f%%, threshold: %.1f%%)",
                 g_CurrentWorkStaStatus.lightIntensity, THRESHOLD_LIGHT_AUTO_ON);
      }

      // 烟雾传感器自动控制继电器1
      if (g_CurrentWorkStaStatus.smokeScope > THRESHOLD_SMOKE_AUTO_ON)
      {
        gpio_set_level(SET_RELAY_NUM1_GPIO, 1);
        g_CurrentWorkStaStatus.relayNum1Status = 1;
        ESP_LOGI(TAG_ALERT, "Auto relay1 control: ON (Smoke level: %.1f%%, threshold: %.1f%%)",
                 g_CurrentWorkStaStatus.smokeScope, THRESHOLD_SMOKE_AUTO_ON);
      }
      else
      {
        gpio_set_level(SET_RELAY_NUM1_GPIO, 0);
        g_CurrentWorkStaStatus.relayNum1Status = 0;
        ESP_LOGI(TAG_ALERT, "Auto relay1 control: OFF (Smoke intensity: %.1f%%, threshold: %.1f%%)",
                 g_CurrentWorkStaStatus.smokeScope, THRESHOLD_SMOKE_AUTO_ON);
      }
    }
    else
    {
      ESP_LOGI(TAG_ALERT, "Auto control skipped - Power: %d, Emergency: %d",
               g_CurrentWorkStaStatus.powerStatus, gTag_emergencyMode);
    }

#if ENABLE_AUTO_POWER_OFF
    // 生产模式：紧急情况下暂停正常数据上报
    if (alert >= ALERT_FIRE_EMERGENCY && alert <= ALERT_HEAT_EMERGENCY)
    {
      ESP_LOGE(TAG_ALERT, "[EMERGENCY MODE] Device powered off for safety!");
      ESP_LOGE(TAG_ALERT, "EmergencyPowerOff event already sent, manual reset required to resume operation.");

      // 紧急模式下暂停正常数据上报
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue; // 跳过正常数据上报
    }
#else
    // 开发模式：即使紧急异常也继续正常运行和数据上报
    if (alert >= ALERT_FIRE_EMERGENCY && alert <= ALERT_HEAT_EMERGENCY)
    {
      ESP_LOGW(TAG_ALERT,
               "[DEV MODE] Emergency detected but continuing normal operation");
    }
#endif

    // 发送数据到显示队列
    xQueueSend(queue_SensorData, &g_CurrentWorkStaStatus, 0);

    // 计数器递增
    upload_counter++;
    basic_upload_counter++;

    // 判断是否需要上报数据
    uint8_t should_upload = 0;
    uint8_t should_upload_basic = 0;

    // 定时上报（每60秒）
    if (upload_counter >= 12)
    {
      should_upload = 1;
      upload_counter = 0;
      ESP_LOGI(MQTT_LOG, "Regular upload (60s timer)");
    }

    // 强制立即上报（异常、刷卡、移卡）
    if (gFlag_forceUpload)
    {
      should_upload = 1;
      upload_counter = 0;       // 重置计数器
      basic_upload_counter = 0; // 重置基础计数器
      gFlag_forceUpload = 0;    // 清除标志
      ESP_LOGI(MQTT_LOG, "Force upload triggered (event detected)");
    }
    else if (strcmp(g_CurrentWorkStaStatus.curUsername, "null") != 0)
    {
      // 刷卡后重置基础计数器，避免刷卡后立即上报基础数据
      basic_upload_counter = 0;
    }

    // 未刷卡时才使用基础传感器上报
    if (strcmp(g_CurrentWorkStaStatus.curUsername, "null") == 0)
    {
      // 检查基础传感器数据是否需要上报（30秒间隔）
      if (basic_upload_counter >= 6)
      {
        should_upload_basic = 1;
        basic_upload_counter = 0;
        ESP_LOGI(MQTT_LOG, "Basic sensor upload (30s timer, no card)");
      }
    }

    // 执行数据上报
    if (should_upload || should_upload_basic)
    {
      if (gTag_mqttIsConnected)
      {
        if (should_upload)
        {
          // MQTT连接时：上传完整的工作站信息
          ESP_LOGI(MQTT_LOG, "MQTT connected - uploading complete workstation data");

#if DEBUG_LOG
          ESP_LOGI(MQTT_LOG, "About to upload - username: '%s' (length: %d)",
                   g_CurrentWorkStaStatus.curUsername,
                   strlen(g_CurrentWorkStaStatus.curUsername));
#endif
          int json_len = generate_complete_json(
              gJson_DataUpLoad, sizeof(gJson_DataUpLoad),
              g_CurrentWorkStaStatus.lightIntensity,
              g_CurrentWorkStaStatus.smokeScope, g_CurrentWorkStaStatus.flameScope,
              g_CurrentWorkStaStatus.heatScope, g_CurrentWorkStaStatus.humidity,
              g_CurrentWorkStaStatus.temperature,
              g_CurrentWorkStaStatus.lightStatus,
              g_CurrentWorkStaStatus.powerStatus,
              g_CurrentWorkStaStatus.relayNum1Status,
              g_CurrentWorkStaStatus.relayNum2Status,
              g_CurrentWorkStaStatus.curUsername);
          if (json_len > 0)
          {
            ESP_LOGI(MQTT_LOG, "Uploading complete data to OneNet...");
            ESP_LOGI(MQTT_LOG, "JSON payload: %s", gJson_DataUpLoad);
            esp_mqtt_client_publish(gHandle_mqttClient,
                                    "$sys/3HSBa0ZB1R/ESP_01/thing/property/post",
                                    gJson_DataUpLoad, 0, 1, 0);
          }
          else
          {
            ESP_LOGE(MQTT_LOG, "Failed to generate complete JSON for upload");
          }
        }
        else if (should_upload_basic)
        {
          // MQTT连接时：上传基础传感器数据（未刷卡时使用）
          ESP_LOGI(MQTT_LOG, "MQTT connected - uploading basic sensor data (no card)");

#if DEBUG_LOG
          ESP_LOGI(MQTT_LOG, "About to upload basic data - username: '%s'",
                   g_CurrentWorkStaStatus.curUsername);
#endif
          char basic_json[256];
          int json_len = generate_basic_sensor_json(
              basic_json, sizeof(basic_json),
              g_CurrentWorkStaStatus.humidity,
              g_CurrentWorkStaStatus.temperature,
              g_CurrentWorkStaStatus.smokeScope);

          if (json_len > 0)
          {
            ESP_LOGI(MQTT_LOG, "Uploading basic sensor data to OneNet...");
            ESP_LOGI(MQTT_LOG, "Basic JSON payload: %s", basic_json);
            esp_mqtt_client_publish(gHandle_mqttClient,
                                    "$sys/3HSBa0ZB1R/ESP_01/thing/property/post",
                                    basic_json, 0, 1, 0);
          }
          else
          {
            ESP_LOGE(MQTT_LOG, "Failed to generate basic sensor JSON for upload");
          }
        }
      }
      else
      {
        ESP_LOGW(MQTT_LOG, "MQTT disconnected - no upload");
      }
    }
    // 每5秒采集一次数据
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void Task_Display(void *para)
{
  uint8_t text[16];
  i2c_config_t iic_config = {.mode = I2C_MODE_MASTER,
                             .scl_io_num = GPIO_NUM_18,
                             .sda_io_num = GPIO_NUM_23,
                             .scl_pullup_en = GPIO_PULLUP_ENABLE,
                             .sda_pullup_en = GPIO_PULLUP_ENABLE,
                             .master.clk_speed = 400000};
  ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &iic_config));
  ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
  gSsd_handle = ssd1306_create(I2C_NUM_0, 0x3C);
  ESP_ERROR_CHECK(ssd1306_init(gSsd_handle));
  while (1)
  {
    if (xQueueReceive(queue_SensorData, &g_CurrentWorkStaStatus, 100) ==
        pdTRUE)
    {
      ssd1306_clear_screen(gSsd_handle, 0);
      if (gTag_wifiIsConnected)
      {
        ssd1306_draw_bitmap(gSsd_handle, 111, 0, gImage_icon_wifi, 16, 16);
      }
      ssd1306_draw_bitmap(gSsd_handle, 0, 16, gImage_icon_humi, 48, 48);
      sprintf((char *)text, "%1.2lfC", g_CurrentWorkStaStatus.temperature);
      ssd1306_draw_string(gSsd_handle, 50, 20, text, 16, 1);
      sprintf((char *)text, "%1.2lf%%", g_CurrentWorkStaStatus.humidity);
      ssd1306_draw_string(gSsd_handle, 50, 36, text, 16, 1);
      ssd1306_refresh_gram(gSsd_handle);
    }
    vTaskDelay(100);
  }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    ESP_LOGI(TAG_STA, "WiFi STA started. Connecting to AP '%s'...",
             ESP_WIFI_STA_SSID);
    esp_wifi_connect();
    gTag_wifiIsConnected = 0;
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG_STA, "Successfully connected to ap SSID:%s password:%s",
             ESP_WIFI_STA_SSID, ESP_WIFI_STA_PASSWD);
    ESP_LOGI(TAG_STA, "Obtained IP address: " IPSTR,
             IP2STR(&event->ip_info.ip));
    gTag_wifiIsConnected = 1;
    xEventGroupSetBits(gHandle_wifiEventGroup, WIFI_CONNECTED_BIT);
    ESP_LOGI(TAG_STA, "   - WiFi ready. Starting MQTT client...");
    esp_mqtt_client_start(gHandle_mqttClient);
  }
  else if (event_base == WIFI_EVENT &&
           event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    if (gHandle_mqttClient != NULL)
    {
      esp_mqtt_client_stop(gHandle_mqttClient);
    }
    wifi_event_sta_disconnected_t *disconnected_event =
        (wifi_event_sta_disconnected_t *)event_data;
    ESP_LOGW(TAG_STA, "Disconnected from AP '%s' (Reason: %d)",
             ESP_WIFI_STA_SSID, disconnected_event->reason);
    gTag_wifiIsConnected = 0;
    esp_wifi_connect();
  }
}

esp_netif_t *wifi_init_sta(void)
{
  esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();

  wifi_config_t wifi_sta_config = {
      .sta =
          {
              .ssid = ESP_WIFI_STA_SSID,
              .password = ESP_WIFI_STA_PASSWD,
              .scan_method = WIFI_ALL_CHANNEL_SCAN,
              .failure_retry_cnt = 10,
              .threshold.authmode = WIFI_AUTH_WPA_PSK,
              .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
          },
  };
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
  return esp_netif_sta;
}

static void log_error_if_nonzero(const char *message, int error_code)
{
  if (error_code != 0)
  {
    ESP_LOGE(MQTT_LOG, "Last error %s: 0x%x", message, error_code);
  }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
  ESP_LOGD(MQTT_LOG,
           "Event dispatched from event loop base=%s, event_id=%" PRIi32 "",
           base, event_id);
  esp_mqtt_event_handle_t event = event_data;
  esp_mqtt_client_handle_t gHandle_mqttClient = event->client;
  int msg_id;
  switch ((esp_mqtt_event_id_t)event_id)
  {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(MQTT_LOG, "MQTT_EVENT_CONNECTED");

    // 订阅属性上报回复主题
    msg_id = esp_mqtt_client_subscribe(
        gHandle_mqttClient, "$sys/3HSBa0ZB1R/ESP_01/thing/property/post/reply",
        0);
    ESP_LOGI(MQTT_LOG, "Subscribed to property post reply, msg_id=%d", msg_id);

    // 订阅属性设置主题（云端控制）
    msg_id = esp_mqtt_client_subscribe(
        gHandle_mqttClient, "$sys/3HSBa0ZB1R/ESP_01/thing/property/set", 0);
    ESP_LOGI(MQTT_LOG, "Subscribed to property set, msg_id=%d", msg_id);

    // 订阅事件上报回复主题（OneNet标准格式）
    msg_id = esp_mqtt_client_subscribe(
        gHandle_mqttClient, "$sys/3HSBa0ZB1R/ESP_01/thing/event/post/reply", 0);
    ESP_LOGI(MQTT_LOG, "Subscribed to event post reply, msg_id=%d", msg_id);

    gTag_mqttIsConnected = 1;
    break;
  case MQTT_EVENT_DISCONNECTED:
    gTag_mqttIsConnected = 0;
    ESP_LOGI(MQTT_LOG, "MQTT_EVENT_DISCONNECTED");
    break;

  case MQTT_EVENT_SUBSCRIBED: // 订阅成功
    ESP_LOGI(MQTT_LOG,
             "[OK] MQTT fully connected and subscribed to all topics");
    break;
  case MQTT_EVENT_PUBLISHED: // 发布确认
    ESP_LOGI(MQTT_LOG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_DATA: // 收到消息
    ESP_LOGI(MQTT_LOG, "MQTT_EVENT_DATA");
    printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
    printf("DATA=%.*s\r\n", event->data_len, event->data);

    // 处理远程控制指令
    if (strstr(event->topic, "property/set") != NULL)
    {
      ESP_LOGI(MQTT_LOG, "Processing remote control command");

      // 解析JSON消息中的设备状态设置
      char *data_str = malloc(event->data_len + 1);
      if (data_str == NULL)
      {
        ESP_LOGE(MQTT_LOG, "Failed to allocate memory for data parsing");
        break;
      }

      memcpy(data_str, event->data, event->data_len);
      data_str[event->data_len] = '\0';

      ESP_LOGI(MQTT_LOG, "Received command: %s", data_str);

      // 解析设备状态控制指令
      if (strstr(data_str, "\"LightStatus\"") != NULL)
      {
        int light_status = extract_json_value(data_str, "LightStatus");
        ESP_LOGI(MQTT_LOG, "Setting light status to: %d", light_status);
        gpio_set_level(SET_LIGHT_CONTROL_GPIO, light_status);
        g_CurrentWorkStaStatus.lightStatus = light_status;
      }

      if (strstr(data_str, "\"PowerStatus\"") != NULL)
      {
        int power_status = extract_json_value(data_str, "PowerStatus");
        ESP_LOGI(MQTT_LOG, "Setting power status to: %d", power_status);

        if (power_status == 1)
        {
          // 开启电源前检查是否处于紧急模式
          if (gTag_emergencyMode)
          {
            ESP_LOGW(MQTT_LOG, "Device in emergency mode, clearing emergency flag");
            gTag_emergencyMode = 0;
          }

          // 如果当前用户名是"null"（未刷卡状态），设置为管理员用户名
          if (strcmp(g_CurrentWorkStaStatus.curUsername, "null") == 0)
          {
            strcpy(g_CurrentWorkStaStatus.curUsername, "0000000001");
            strcpy(gCardUsername, "0000000001");
            ESP_LOGI(MQTT_LOG, "Remote power on by administrator: %s", g_CurrentWorkStaStatus.curUsername);
          }

          gpio_set_level(SET_POWER_STATUS_GPIO, 1);
          g_CurrentWorkStaStatus.powerStatus = 1;
        }
        else
        {
          control_all_relays(0);

          // 如果当前用户名是管理员用户名（远程开启时设置的），关闭电源时重置为null
          if (strcmp(g_CurrentWorkStaStatus.curUsername, "0000000001") == 0)
          {
            strcpy(g_CurrentWorkStaStatus.curUsername, "null");
            strcpy(gCardUsername, "null");
            ESP_LOGI(MQTT_LOG, "Remote power off, resetting username to null");
          }
        }
      }

      if (strstr(data_str, "\"RelayNum1Status\"") != NULL)
      {
        int relay1_status = extract_json_value(data_str, "RelayNum1Status");
        ESP_LOGI(MQTT_LOG, "Setting relay1 status to: %d", relay1_status);
        gpio_set_level(SET_RELAY_NUM1_GPIO, relay1_status);
        g_CurrentWorkStaStatus.relayNum1Status = relay1_status;
      }

      if (strstr(data_str, "\"RelayNum2Status\"") != NULL)
      {
        int relay2_status = extract_json_value(data_str, "RelayNum2Status");
        ESP_LOGI(MQTT_LOG, "Setting relay2 status to: %d", relay2_status);
        gpio_set_level(SET_RELAY_NUM2_GPIO, relay2_status);
        g_CurrentWorkStaStatus.relayNum2Status = relay2_status;
      }

      free(data_str);
      ESP_LOGI(MQTT_LOG, "Remote control command processed successfully");
    }
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGI(MQTT_LOG, "MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
    {
      log_error_if_nonzero("reported from esp-tls",
                           event->error_handle->esp_tls_last_esp_err);
      log_error_if_nonzero("reported from tls stack",
                           event->error_handle->esp_tls_stack_err);
      log_error_if_nonzero("captured as transport's socket errno",
                           event->error_handle->esp_transport_sock_errno);
      ESP_LOGI(MQTT_LOG, "Last errno string (%s)",
               strerror(event->error_handle->esp_transport_sock_errno));
    }
    break;
  default:
    ESP_LOGI(MQTT_LOG, "Other event id:%d", event->event_id);
    break;
  }
}

// RC522 RFID卡片状态变化事件处理函数
static void on_rfid_card_state_changed(void *arg, esp_event_base_t base,
                                       int32_t event_id, void *data)
{
  rc522_picc_state_changed_event_t *event =
      (rc522_picc_state_changed_event_t *)data;
  rc522_picc_t *picc = event->picc;

  // 卡片激活（靠近）
  if (picc->state == RC522_PICC_STATE_ACTIVE)
  {

#if DEBUG_LOG
    rc522_picc_print(picc); // 打印卡片详细信息
#endif

    // 检查卡片是否为MIFARE Classic兼容卡片
    if (!rc522_mifare_type_is_classic_compatible(picc->type))
    {
      ESP_LOGW(TAG_RC522, "Card type not supported for block reading");
      return;
    }

    // 使用默认密钥进行认证
    const uint8_t block_address = 1; // 读取第1块
    rc522_mifare_key_t key = {
        .value = {RC522_MIFARE_KEY_VALUE_DEFAULT}, // 默认密钥 FF FF FF FF FF FF
    };

    // 认证Block 1所在的扇区（扇区0）
    if (rc522_mifare_auth(gHandle_rc522Scanner, picc, block_address, &key) !=
        ESP_OK)
    {
      ESP_LOGE(TAG_RC522, "Authentication failed for block %d", block_address);
      return;
    }

    // 读取Block 1的数据（16字节）
    uint8_t read_buffer[RC522_MIFARE_BLOCK_SIZE];
    if (rc522_mifare_read(gHandle_rc522Scanner, picc, block_address,
                          read_buffer) != ESP_OK)
    {
      ESP_LOGE(TAG_RC522, "Failed to read block %d", block_address);
      rc522_mifare_deauth(gHandle_rc522Scanner, picc);
      return;
    }

    // 清空用户名缓冲区
    memset(gCardUsername, '\0', sizeof(gCardUsername));

    // 从卡片读取前10个字节，但要处理NULL字符
    // 查找第一个NULL字符的位置，确保复制完整的字符串
    int copy_length = 0;
    for (int i = 0; i < 10; i++)
    {
      if (read_buffer[i] == '\0')
      {
        break; // 遇到NULL字符，停止复制
      }
      copy_length = i + 1;
    }

    // 如果前10个字节都没有NULL字符，取全部10个字节
    if (copy_length == 0)
    {
      copy_length = 10;
    }

    memcpy(gCardUsername, read_buffer, copy_length);
    gCardUsername[copy_length] = '\0'; // 确保字符串结束

#if DEBUG_LOG
    // 调试日志：打印读取到的用户名和原始数据
    ESP_LOGI(TAG_RC522, "Raw card data (first 10 bytes):");
    for (int i = 0; i < 10; i++)
    {
      ESP_LOGI(TAG_RC522, "  [%d]: 0x%02X ('%c')", i, read_buffer[i],
               (read_buffer[i] >= 32 && read_buffer[i] <= 126) ? read_buffer[i] : '?');
    }
    ESP_LOGI(TAG_RC522, "Extracted username: '%s' (length: %d)", gCardUsername, strlen(gCardUsername));
#endif
    // 检查读取到的用户名是否有效
    if (strlen(gCardUsername) == 0)
    {
      ESP_LOGW(TAG_RC522, "Warning: Card contains no username, setting to UNKNOWN");
      strcpy(gCardUsername, "UNKNOWN");
    }
    else if (strcmp(gCardUsername, "null") == 0)
    {
      ESP_LOGW(TAG_RC522, "Warning: Card contains 'null' value, setting to UNKNOWN");
      strcpy(gCardUsername, "UNKNOWN");
    }

    // 清理认证状态
    rc522_mifare_deauth(gHandle_rc522Scanner, picc);

    // 设置卡片存在标志
    gTag_cardPresent = 1;

    // 只开启电源，其他设备保持关闭直到有事件触发
    enable_power_only();

    // 更新工位状态中的用户ID，确保完整复制
    strncpy(g_CurrentWorkStaStatus.curUsername, gCardUsername,
            sizeof(g_CurrentWorkStaStatus.curUsername) - 1);
    g_CurrentWorkStaStatus.curUsername[sizeof(g_CurrentWorkStaStatus.curUsername) - 1] = '\0'; // 确保字符串结尾

    // 刷卡事件：触发立即上报
    gFlag_forceUpload = 1;

    ESP_LOGI(TAG_RC522, "Card detected! Username from Block 1: %s",
             gCardUsername);
#if DEBUG_LOG
    ESP_LOGI(TAG_RC522,
             "Block 1 raw data (hex): %02X %02X %02X %02X %02X %02X %02X %02X "
             "%02X %02X %02X %02X %02X %02X %02X %02X",
             read_buffer[0], read_buffer[1], read_buffer[2], read_buffer[3],
             read_buffer[4], read_buffer[5], read_buffer[6], read_buffer[7],
             read_buffer[8], read_buffer[9], read_buffer[10], read_buffer[11],
             read_buffer[12], read_buffer[13], read_buffer[14],
             read_buffer[15]);
#endif
    ESP_LOGI(TAG_RC522, "Power and relays turned ON");
  }
  // 卡片移除
  else if (picc->state == RC522_PICC_STATE_IDLE &&
           event->old_state >= RC522_PICC_STATE_ACTIVE)
  {
    ESP_LOGI(TAG_RC522, "Card removed! Username was: %s", gCardUsername);

    // 清除卡片存在标志
    gTag_cardPresent = 0;

    // 关闭所有继电器和电源
    control_all_relays(0);

    strcpy(gCardUsername, "null");
    strncpy(g_CurrentWorkStaStatus.curUsername, "null", sizeof(g_CurrentWorkStaStatus.curUsername) - 1);
    g_CurrentWorkStaStatus.curUsername[sizeof(g_CurrentWorkStaStatus.curUsername) - 1] = '\0';

    // 移卡事件：触发立即上报
    gFlag_forceUpload = 1;

    ESP_LOGI(TAG_RC522, "Card removed, but power kept ON for auto-control testing");
  }
}

// RC522初始化函数
void rc522_init(void)
{
  // 配置RC522 SPI驱动
  rc522_spi_config_t driver_config = {
      .host_id = SPI3_HOST,
      .bus_config =
          &(spi_bus_config_t){
              .miso_io_num = RC522_SPI_BUS_GPIO_MISO,
              .mosi_io_num = RC522_SPI_BUS_GPIO_MOSI,
              .sclk_io_num = RC522_SPI_BUS_GPIO_SCLK,
          },
      .dev_config =
          {
              .spics_io_num = RC522_SPI_SCANNER_GPIO_SDA,
          },
      .rst_io_num = RC522_SCANNER_GPIO_RST,
  };

  // 创建SPI驱动
  ESP_ERROR_CHECK(rc522_spi_create(&driver_config, &gHandle_rc522Driver));
  ESP_ERROR_CHECK(rc522_driver_install(gHandle_rc522Driver));

  // 创建RC522扫描器
  rc522_config_t scanner_config = {
      .driver = gHandle_rc522Driver,
  };

  ESP_ERROR_CHECK(rc522_create(&scanner_config, &gHandle_rc522Scanner));
  ESP_ERROR_CHECK(rc522_register_events(gHandle_rc522Scanner,
                                        RC522_EVENT_PICC_STATE_CHANGED,
                                        on_rfid_card_state_changed, NULL));
  ESP_ERROR_CHECK(rc522_start(gHandle_rc522Scanner));

  ESP_LOGI(TAG_RC522, "RC522 RFID reader initialized successfully");
}

void mqtt_app_init(void)
{
  esp_mqtt_client_config_t mqtt_cfg = {
      .session.keepalive = 10,
      .network.disable_auto_reconnect = false,
      .network.reconnect_timeout_ms = 5000,
      .network.timeout_ms = 15000,
      .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1,
      .session.message_retransmit_timeout = 3000,
      .broker.address.uri = MQTT_BROKER_URL,
      .credentials.username = PRODUCE_ID,
      .credentials.client_id = DEVICE_ID,
      .credentials.authentication.password =
          DEVICE_PASSWORD};
  gHandle_mqttClient = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(gHandle_mqttClient, ESP_EVENT_ANY_ID,
                                 mqtt_event_handler, NULL);
}

/**
 * @brief HTTP服务器任务
 */
void Task_HttpServer(void *pvParameters)
{
  // 等待WiFi连接成功
  if (gHandle_wifiEventGroup != NULL)
  {
    xEventGroupWaitBits(
        gHandle_wifiEventGroup,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdTRUE,
        portMAX_DELAY // 无限等待WiFi连接
    );
    ESP_LOGI(TAG_HTTP, "WiFi connected, starting HTTP server...");
  }

  // 启动HTTP服务器
  httpd_start_server();

  // 任务阻塞（保持服务器运行，不退出）
  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(1000)); // 降低CPU占用
  }

  // （可选）停止服务器（实际不会执行，除非任务被删除）
  if (g_httpd_server != NULL)
  {
    httpd_stop(g_httpd_server);
    g_httpd_server = NULL;
    ESP_LOGI(TAG_HTTP, "HTTP server stopped");
  }

  vTaskDelete(NULL);
}

void app_main(void)
{
  Init_Config();
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  gHandle_wifiEventGroup = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_LOGI(TAG_STA, "ESP_WIFI_MODE_STA");
  wifi_init_sta();
  gTag_wifiIsConnected = 0;
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG_STA, "wifi has started.");
  mqtt_app_init();

  // 初始化RC522 RFID读卡器
  rc522_init();

  queue_SensorData = xQueueCreate(1, sizeof(g_CurrentWorkStaStatus));
  xTaskCreate(Task_ReadSensorValue, "ReadSensorValue", 3072, NULL, 7, NULL);
  xTaskCreate(Task_Display, "Display", 2048, NULL, 7, NULL);
  xTaskCreate(Task_HttpServer, "Task_HttpServer", 4096, NULL, 5, NULL);
  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
