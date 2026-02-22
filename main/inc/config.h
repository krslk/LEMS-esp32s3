#ifndef __CONFIG_H__
#define __CONFIG_H__

// 传感器数据采集引脚
#define SENSOT_TEMP_AND_HUMI_DATA_GPIO 26         // 温湿度传感器模块输入引脚
#define SENSOT_HEAT_FIGURE_GPIO 34                // 热敏传感器模块输入引脚
#define SENSOT_LIGHT_FIGURE_GPIO 35               // 光敏传感器模块输入引脚
#define SENSOT_FLAME_FIGURE_GPIO 32               // 火焰传感器模块输入引脚
#define SENSOT_SMOKE_FIGURE_GPIO 33               // 烟雾传感器模块输入引脚
#define SENSOT_HUMAN_BODY_INFRARED_FIGURE_GPIO 36 // 人体红外传感器模块输入引脚

// 引脚对应 ADC1 通道号
#define HEAT_ADC_CHANNEL ADC1_CHANNEL_6  // GPIO34 → ADC1_CH6
#define LIGHT_ADC_CHANNEL ADC1_CHANNEL_7 // GPIO35 → ADC1_CH7
#define FLAME_ADC_CHANNEL ADC1_CHANNEL_4 // GPIO32 → ADC1_CH4
#define SMOKE_ADC_CHANNEL ADC1_CHANNEL_5 // GPIO33 → ADC1_CH5

// ADC 采样参数
#define ADC_ATTENUATION ADC_ATTEN_DB_12 // 衰减值12dB，支持0~3.3V测量
#define ADC_WIDTH ADC_WIDTH_BIT_12      // 采样精度12位（输出0~4095）

// 执行器控制引脚
#define SET_RELAY_NUM1_GPIO 13   // 1号继电器控制引脚
#define SET_RELAY_NUM2_GPIO 4    // 2号继电器控制引脚
#define SET_LIGHT_CONTROL_GPIO 2 // 灯光控制引脚
#define SET_POWER_STATUS_GPIO 0  // 电源状态控制引脚

// RC522 RFID读卡器引脚配置
#define RC522_SPI_BUS_GPIO_MISO 25   // SPI MISO引脚
#define RC522_SPI_BUS_GPIO_MOSI 21   // SPI MOSI引脚
#define RC522_SPI_BUS_GPIO_SCLK 22   // SPI SCLK引脚
#define RC522_SPI_SCANNER_GPIO_SDA 5 // SPI CS引脚
#define RC522_SCANNER_GPIO_RST -1    // RST引脚（-1表示软复位）

// ==================== 异常检测阈值配置 ====================
// 开发模式(0): 只告警不断电，方便调试
// 生产模式(1): 紧急情况自动断电保护
#define ENABLE_AUTO_POWER_OFF 1 // 改为 1 启用生产模式
#define DEBUG_LOG 1

// 紧急异常阈值（ESP32本地立即处理）
#define THRESHOLD_FLAME_EMERGENCY 70.0f // 火焰强度 > 70% 触发紧急告警
#define THRESHOLD_SMOKE_EMERGENCY 95.0f // 烟雾浓度 > 75% 触发紧急告警
#define THRESHOLD_TEMP_EMERGENCY 50.0f  // 温度 > 50℃ 触发紧急告警
#define THRESHOLD_HEAT_EMERGENCY 80.0f  // 热度 > 80% 触发紧急告警

// 一般告警阈值（仅上报告警）
#define THRESHOLD_FLAME_WARNING 70.0f // 火焰强度 > 40% 发出告警
#define THRESHOLD_SMOKE_WARNING 90.0f // 烟雾浓度 > 60% 发出告警
#define THRESHOLD_TEMP_HIGH 35.0f     // 温度 > 35℃ 发出告警
#define THRESHOLD_TEMP_LOW 10.0f      // 温度 < 10℃ 发出告警
#define THRESHOLD_HUMIDITY_HIGH 65.0f // 湿度 > 65% 发出告警
#define THRESHOLD_HUMIDITY_LOW 20.0f  // 湿度 < 20% 发出告警

// 自动控制阈值
#define THRESHOLD_LIGHT_AUTO_ON 30.0f // 光照强度 < 30% 时自动开灯（光线较暗）
#define THRESHOLD_SMOKE_AUTO_ON 50.0f // 烟雾浓度 > 50% 时自动打开继电器1

// 工位状态结构体
typedef struct WorkstationStatus
{
  char workstationCode[32]; // 工位编号
  char curUsername[12];     // 使用者用户名
  int powerStatus;          // 电源状态
  int lightStatus;          // 灯光状态
  int relayNum1Status;      // 1号继电器状态
  int relayNum2Status;      // 2号继电器状态
  float lightIntensity;     // 光线传感器采集值
  float smokeScope;         // 烟雾传感器采集值
  float flameScope;         // 火焰传感器检测值
  float heatScope;          // 热敏传感器检测值
  float humidity;           // 湿度采集值
  float temperature;        // 温度采集值

  int heatAdcRaw;     // 热敏传感器ADC原始值（原 heat_adc_raw）
  int lightAdcRaw;    // 光敏传感器ADC原始值（原 light_adc_raw）
  int flameAdcRaw;    // 火焰传感器ADC原始值（原 flame_adc_raw）
  int smokeAdcRaw;    // 烟雾传感器ADC原始值（原 smoke_adc_raw）
  float heatVoltage;  // 热敏传感器电压值（原 heat_voltage）
  float lightVoltage; // 光敏传感器电压值（原 light_voltage）
  float flameVoltage; // 火焰传感器电压值（原 flame_voltage）
  float smokeVoltage; // 烟雾传感器电压值（原 smoke_voltage）
} WorkstationStatus;

// 异常类型枚举
typedef enum
{
  ALERT_NONE = 0,
  ALERT_FIRE_EMERGENCY,        // 火灾紧急情况
  ALERT_SMOKE_EMERGENCY,       // 烟雾紧急情况
  ALERT_TEMP_EMERGENCY,        // 高温紧急情况
  ALERT_HEAT_EMERGENCY,        // 热度紧急情况
  ALERT_FIRE_WARNING,          // 火焰告警
  ALERT_SMOKE_WARNING,         // 烟雾告警
  ALERT_TEMP_HIGH_WARNING,     // 高温告警
  ALERT_TEMP_LOW_WARNING,      // 低温告警
  ALERT_HUMIDITY_HIGH_WARNING, // 高湿度告警
  ALERT_HUMIDITY_LOW_WARNING   // 低湿度告警
} AlertType;

#include "headfile.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define ESP_WIFI_STA_SSID "pc_lk"
#define ESP_WIFI_STA_PASSWD "123456789"

#define MQTT_BROKER_URL "mqtt://218.201.45.2:1883"
#define PRODUCE_ID "3HSBa0ZB1R"
#define DEVICE_ID "ESP_01"
#define DEVICE_PASSWORD "version=2018-10-31&res=products%2F3HSBa0ZB1R%2Fdevices%2FESP_01&et=1762879066&method=md5&sign=XRbpWQF8WS0crV%2F5CGCRqQ%3D%3D"

void Init_Config(void);

#endif