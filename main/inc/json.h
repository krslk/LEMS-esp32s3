#ifndef __JSON_H__
#define __JSON_H__

#include "headfile.h"

/**
 * @brief 生成包含告警信息的JSON字符串
 *
 * @param buffer [out] 用于存放生成的JSON字符串的缓冲区
 * @param buf_len [in] 缓冲区的大小
 * @param alert_type [in] 告警类型
 * @param sensor_value [in] 传感器值 (float)
 * @param threshold [in] 阈值 (float)
 * @param is_emergency [in] 是否紧急 (uint8_t)
 * @return int 成功则返回生成的JSON字符串长度；失败则返回 -1 (参数无效) 或 -2 (缓冲区不足)
 */
int generate_alert_json(char *buffer, size_t buf_len, AlertType alert_type,
                        float sensor_value, float threshold,
                        uint8_t is_emergency);

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
int generate_basic_sensor_json(char *buffer, size_t buf_len, float humidity, float temperature, float smoke_scope);

/**
 * @brief 发送告警消息到MQTT
 *
 * @param alert_type [in] 告警类型
 * @param sensor_value [in] 传感器值 (float)
 * @param threshold [in] 阈值 (float)
 * @param is_emergency [in] 是否紧急 (uint8_t)
 */
void send_alert_mqtt(AlertType alert_type, float sensor_value, float threshold,
                     uint8_t is_emergency);

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
                           int32_t relay_num2_status, char *username);

#endif