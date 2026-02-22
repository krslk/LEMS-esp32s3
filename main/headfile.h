#ifndef __HEADFILE_H__
#define __HEADFILE_H__

#include "inttypes.h"
#include "stdio.h"

#include "driver/gpio.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"
#include <time.h>
#include <sys/time.h>
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
#include "dht.h"
#include "driver/adc.h"
#include "driver/i2c.h"
#include "mqtt_client.h"
#include "picc/rc522_mifare.h"
#include "rc522.h"
#include "rc522_picc.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_http_server.h"
#include "driver/rc522_spi.h"
#include "esp_timer.h"
#include "ssd1306.h"
#include "led.h"
#include "config.h"
#include "json.h"

#ifndef LOG_TAG
#define LOG_TAG

#define MQTT_LOG "MQTT"
#define ENV_LOG "ENV"
#define TAG_HTTP "HTTP"
#define TAG_STA "WIFI"
#define TAG_RC522 "RC522"
#define TAG_ALERT "ALERT"

#endif

extern char json_buf[512];

extern uint8_t gWifi_connected;
extern uint8_t gMqtt_isconnected;

extern esp_mqtt_client_handle_t client;
extern QueueHandle_t queue_temperatureData;

extern char gCardUsername[32];     // 存储卡片用户名字符串
extern uint8_t gTag_cardPresent;   // 卡片存在标志
extern uint8_t gTag_emergencyMode; // 紧急模式标志
extern uint8_t gFlag_forceUpload;  // 强制立即上报标志（异常、刷卡、移卡时触发）
extern char gJson_DataUpLoad[512];
extern uint8_t gTag_mqttIsConnected;
extern uint8_t gTag_wifiIsConnected;
extern httpd_handle_t g_httpd_server;
extern QueueHandle_t g_keyCallBackQueue;
extern rc522_handle_t gHandle_rc522Scanner;
extern uint8_t gSensorCollect_humiBodyInFrared;
extern WorkstationStatus g_CurrentWorkStaStatus;
extern EventGroupHandle_t gHandle_wifiEventGroup;
extern rc522_driver_handle_t gHandle_rc522Driver;
extern esp_mqtt_client_handle_t gHandle_mqttClient;

extern ssd1306_handle_t gSsd_handle;
extern QueueHandle_t queue_SensorData;

const char *get_alert_type_string(AlertType type);

#endif