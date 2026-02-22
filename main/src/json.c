#include "json.h"

int generate_alert_json(char *buffer, size_t buf_len, AlertType alert_type,
                        float sensor_value, float threshold,
                        uint8_t is_emergency)
{
    if (buffer == NULL || buf_len == 0)
    {
        return -1;
    }
    memset(buffer, 0, buf_len);
    const char *severity = is_emergency ? "EMERGENCY" : "WARNING";
    const char *alert_name = get_alert_type_string(alert_type);
    // OneNet标准事件格式：使用identifier区分事件类型
    const char *event_identifier = is_emergency ? "AlertEmergency" : "AlertWarning";
    int len = snprintf(
        buffer, buf_len,
        "{\"id\":\"123\","
        "\"version\":\"1.0\","
        "\"params\":{"
        "\"%s\":{"
        "\"value\":{"
        "\"alertType\":\"%s\","
        "\"severity\":\"%s\","
        "\"sensorValue\":%.2f,"
        "\"threshold\":%.2f,"
        "\"username\":\"%s\","
        "\"temperature\":%.2f,"
        "\"humidity\":%.1f,"
        "\"flameScope\":%.1f,"
        "\"smokeScope\":%.1f"
        "}"
        "}"
        "}"
        "}",
        event_identifier, alert_name, severity, sensor_value, threshold,
        g_CurrentWorkStaStatus.curUsername, g_CurrentWorkStaStatus.temperature,
        g_CurrentWorkStaStatus.humidity, g_CurrentWorkStaStatus.flameScope,
        g_CurrentWorkStaStatus.smokeScope);

    if (len < 0 || (size_t)len >= buf_len)
    {
        return -2;
    }
    return len;
}

void send_alert_mqtt(AlertType alert_type, float sensor_value, float threshold,
                     uint8_t is_emergency)
{
    if (!gTag_mqttIsConnected)
    {
        ESP_LOGW(TAG_ALERT, "MQTT not connected, alert not sent");
        return;
    }

    char alert_json[512];
    int json_len = generate_alert_json(alert_json, sizeof(alert_json), alert_type,
                                       sensor_value, threshold, is_emergency);

    if (json_len > 0)
    {
        // OneNet标准事件上报主题（所有事件统一使用此主题）
        const char *topic = "$sys/3HSBa0ZB1R/ESP_01/thing/event/post";

        ESP_LOGW(TAG_ALERT, "Sending %s event to OneNet",
                 is_emergency ? "EMERGENCY" : "WARNING");
        ESP_LOGW(TAG_ALERT, "Event JSON: %s", alert_json);
        esp_mqtt_client_publish(gHandle_mqttClient, topic, alert_json, 0, 1, 0);
    }
}

/**
 * @brief 生成只包含温湿度和烟雾数据的简化JSON字符串（MQTT离线时使用）
 *
 * @param buffer [out] 用于存放生成的JSON字符串的缓冲区
 * @param buf_len [in] 缓冲区的大小
 * @param humidity [in] 相对湿度 (float)
 * @param temperature [in] 温度 (float)
 * @param smoke_scope [in] 烟雾浓度 (float)
 * @return int 成功则返回生成的JSON字符串长度；失败则返回 -1 (参数无效) 或 -2 (缓冲区不足)
 */
int generate_basic_sensor_json(char *buffer, size_t buf_len, float humidity, float temperature, float smoke_scope)
{
    // 1. 基础参数校验
    if (buffer == NULL || buf_len == 0)
    {
        return -1;
    }

    // 清空缓冲区
    memset(buffer, 0, buf_len);

    // 2. 使用 snprintf 一次性拼接基本传感器数据
    int len = snprintf(buffer, buf_len,
                       "{\"id\":\"123\","
                       "\"version\":\"1.0\","
                       "\"params\":{"
                       "\"CurrentTemperature\":{\"value\":%.2f},"
                       "\"RelativeHumidity\":{\"value\":%.1f},"
                       "\"SmokeScope\":{\"value\":%.1f}"
                       "}"
                       "}",
                       temperature, humidity, smoke_scope);

    // 3. 检查 snprintf 结果
    if (len < 0 || (size_t)len >= buf_len)
    {
        return -2; // 生成失败或缓冲区不足
    }

    return len;
}

/**
 * @brief 生成包含所有传感器和设备状态的JSON字符串
 *
 * @param gJson_DataUpLoad [out] 用于存放生成的JSON字符串的缓冲区
 * @param buf_len [in] 缓冲区的大小
 * @param light_lux_value [in] 光照强度 (float)
 * @param smoke_scope [in] 烟雾浓度 (float)
 * @param flame_scope [in] 火焰强度 (float)
 * @param heat_scope [in] 热度 (float)
 * @param humidity [in] 相对湿度 (float)
 * @param temperature [in] 温度 (float)
 * @param light_status [in] 灯光状态 (int32)
 * @param power_status [in] 电源状态 (int32)
 * @param relay_num1_status [in] 1号继电器状态 (int32)
 * @param relay_num2_status [in] 2号继电器状态 (int32)
 * @param user_id [in] 用户username (字符串)
 *
 * @return int 成功则返回生成的JSON字符串长度；失败则返回 -1 (参数无效) 或 -2
 * (缓冲区不足)
 */
int generate_complete_json(char *gJson_DataUpLoad, size_t buf_len,
                           float light_lux_value, float smoke_scope,
                           float flame_scope, float heat_scope, float humidity,
                           float temperature, int32_t light_status,
                           int32_t power_status, int32_t relay_num1_status,
                           int32_t relay_num2_status, char *username)
{
    // 1. 基础参数校验 (检查所有指针参数)
    if (gJson_DataUpLoad == NULL || buf_len == 0)
    {
        return -1;
    }

    // 清空缓冲区
    memset(gJson_DataUpLoad, 0, buf_len);

    // 2. 使用 snprintf 一次性拼接所有数据
    int len = snprintf(gJson_DataUpLoad, buf_len,
                       "{\"id\":\"123\","
                       "\"version\":\"1.0\","
                       "\"params\":{"
                       "\"CurrentTemperature\":{\"value\":%.2f},"
                       "\"FlameScope\":{\"value\":%.1f},"
                       "\"HeatScope\":{\"value\":%.1f},"
                       "\"LightLuxValue\":{\"value\":%.1f},"
                       "\"LightStatus\":{\"value\":%ld},"
                       "\"PowerStatus\":{\"value\":%ld},"
                       "\"RelativeHumidity\":{\"value\":%.1f},"
                       "\"RelayNum1Status\":{\"value\":%ld},"
                       "\"RelayNum2Status\":{\"value\":%ld},"
                       "\"SmokeScope\":{\"value\":%.1f},"
                       "\"username\":{\"value\":\"%s\"}"
                       "}"
                       "}",
                       temperature, flame_scope, heat_scope, light_lux_value,
                       light_status, power_status, humidity, relay_num1_status,
                       relay_num2_status, smoke_scope, username);

    // 3. 检查 snprintf 结果
    if (len < 0 || (size_t)len >= buf_len)
    {
        return -2; // 生成失败或缓冲区不足
    }

    return len;
}