# ESP32边缘异常检测系统

## 📋 功能概述

本系统在ESP32硬件端实现了**分层异常检测**机制，能够在传感器检测到危险情况时立即做出响应，无需等待云端指令。

## 🔧 开发/生产模式切换

系统支持两种工作模式，通过修改 `main.c` 中的宏定义切换：

```c
#define ENABLE_AUTO_POWER_OFF 0  // 0=开发模式, 1=生产模式
```

### 开发模式 (ENABLE_AUTO_POWER_OFF = 0)
- ✅ 检测所有异常并上报告警消息
- ✅ 继续正常数据上报
- ❌ **不会自动断电**（方便调试和测试）
- 💡 适用于开发、调试、阈值校准阶段

### 生产模式 (ENABLE_AUTO_POWER_OFF = 1)
- ✅ 检测所有异常并上报告警消息
- ✅ 紧急情况**立即自动断电**保护
- ⚠️ 断电后需要手动恢复或远程控制恢复
- 💡 适用于正式部署运行

## 🚨 异常检测分类

### 第一优先级：紧急异常（EMERGENCY）
当检测到以下情况时，ESP32会**立即切断所有电源和继电器**：

| 传感器 | 阈值 | 动作 |
|-------|------|------|
| 🔥 火焰传感器 | > 70% | 立即断电 + 上报 |
| 💨 烟雾传感器 | > 60% | 立即断电 + 上报 |
| 🌡️ 温度传感器 | > 50℃ | 立即断电 + 上报 |
| 🔥 热度传感器 | > 80% | 立即断电 + 上报 |

### 第二优先级：一般告警（WARNING）
当检测到以下情况时，ESP32会**仅上报告警消息**，不切断电源：

| 传感器 | 阈值 | 动作 |
|-------|------|------|
| 火焰传感器 | > 40% | 上报告警 |
| 烟雾传感器 | > 30% | 上报告警 |
| 温度（高） | > 35℃ | 上报告警 |
| 温度（低） | < 10℃ | 上报告警 |
| 湿度（高） | > 80% | 上报告警 |
| 湿度（低） | < 20% | 上报告警 |

## 📡 MQTT主题说明（符合OneNet标准）

### 上报主题

1. **正常数据上报（属性上报）**
   ```
   $sys/3HSBa0ZB1R/ESP_01/thing/property/post
   ```
   每5秒上报一次所有传感器数据

2. **事件上报（告警/紧急事件）**
   ```
   $sys/3HSBa0ZB1R/ESP_01/thing/event/post
   ```
   **重要：** 所有事件（紧急告警、一般告警、断电事件等）都使用这个统一主题。
   事件类型通过JSON消息体中的 `identifier` 字段区分：
   - `AlertEmergency` - 紧急告警事件
   - `AlertWarning` - 一般告警事件
   - `EmergencyPowerOff` - 紧急断电事件

### 订阅主题

1. **属性设置（云端控制）**
   ```
   $sys/3HSBa0ZB1R/ESP_01/thing/property/set
   ```

2. **属性上报回复**
   ```
   $sys/3HSBa0ZB1R/ESP_01/thing/property/post/reply
   ```

3. **事件上报回复**
   ```
   $sys/3HSBa0ZB1R/ESP_01/thing/event/post/reply
   ```

### 通配符订阅（可选）

用于调试，可订阅所有物模型相关主题：
```
$sys/3HSBa0ZB1R/ESP_01/thing/#
```

## 📊 告警JSON格式示例（OneNet标准格式）

### 紧急事件消息
主题：`$sys/3HSBa0ZB1R/ESP_01/thing/event/post`

```json
{
  "id": "123",
  "version": "1.0",
  "params": {
    "AlertEmergency": {
      "value": {
        "alertType": "FIRE_EMERGENCY",
        "severity": "EMERGENCY",
        "sensorValue": 75.5,
        "threshold": 70.0,
        "timestamp": 1234567890,
        "username": "USER001",
        "temperature": 45.2,
        "humidity": 65.3,
        "flameScope": 75.5,
        "smokeScope": 45.2
      }
    }
  }
}
```

### 一般告警消息
主题：`$sys/3HSBa0ZB1R/ESP_01/thing/event/post`

```json
{
  "id": "123",
  "version": "1.0",
  "params": {
    "AlertWarning": {
      "value": {
        "alertType": "TEMP_HIGH_WARNING",
        "severity": "WARNING",
        "sensorValue": 36.5,
        "threshold": 35.0,
        "timestamp": 1234567890,
        "username": "USER001",
        "temperature": 36.5,
        "humidity": 65.3,
        "flameScope": 15.2,
        "smokeScope": 10.5
      }
    }
  }
}
```

### 紧急断电事件消息
主题：`$sys/3HSBa0ZB1R/ESP_01/thing/event/post`

```json
{
  "id": "123",
  "version": "1.0",
  "params": {
    "EmergencyPowerOff": {
      "value": {
        "reason": "Remote emergency command",
        "timestamp": 1234567890
      }
    }
  }
}
```

## 🔧 核心函数说明

### 1. `detect_sensor_anomalies()`
主异常检测函数，检测所有传感器数据并触发相应的告警。

**返回值：**
- `ALERT_NONE` - 无异常
- `ALERT_FIRE_EMERGENCY` - 火灾紧急情况
- `ALERT_SMOKE_EMERGENCY` - 烟雾紧急情况
- 其他告警类型...

### 2. `send_alert_mqtt()`
发送告警消息到MQTT服务器。

**参数：**
- `alert_type` - 告警类型
- `sensor_value` - 传感器值
- `threshold` - 阈值
- `is_emergency` - 是否为紧急情况

### 3. `generate_alert_json()`
生成告警事件的JSON消息。

### 4. `control_all_relays()`
基础电源控制函数，控制所有继电器和电源的开关。

**参数：**
- `enable` - 1=开启，0=关闭

### 5. `emergency_power_off()` ⭐ 预留接口
紧急断电接口，可以被多种方式调用：
- 本地边缘检测自动调用
- MQTT远程控制指令调用
- 其他安全机制调用

**参数：**
- `reason` - 断电原因字符串

**使用示例：**
```c
// 本地检测触发
emergency_power_off("Fire detected by local sensor");

// 远程指令触发
emergency_power_off("Remote emergency shutdown command");
```

### 6. `remote_power_control()` ⭐ 预留接口
远程电源控制接口，预留给MQTT消息处理使用。

**参数：**
- `enable` - 1=开启，0=关闭

**使用场景：**
- 远程开启/关闭设备电源
- 紧急断电后的远程恢复
- 定时开关控制

**集成示例（在MQTT事件处理中）：**
```c
case MQTT_EVENT_DATA:
    // 解析JSON消息
    if (strstr(event->data, "\"PowerControl\"") != NULL) {
        // 提取value值
        int value = parse_power_control_value(event->data);
        
        // 调用远程控制接口
        remote_power_control(value);
    }
    break;
```

## 🔄 工作流程

```
┌─────────────────────┐
│ 读取传感器数据       │
└──────────┬──────────┘
           │
           ↓
┌─────────────────────┐
│ detect_sensor_      │
│ anomalies()         │
└──────────┬──────────┘
           │
    ┌──────┴──────┐
    │             │
    ↓             ↓
┌─────────┐  ┌────────┐
│紧急异常? │  │一般告警?│
└────┬────┘  └───┬────┘
     │ YES       │ YES
     ↓           ↓
┌─────────┐  ┌────────┐
│立即断电  │  │仅上报  │
│发送紧急  │  │发送告警│
│事件消息  │  │消息    │
└─────────┘  └────────┘
     │           │
     └─────┬─────┘
           ↓
┌─────────────────────┐
│ 继续监测            │
└─────────────────────┘
```

## 💡 使用建议

### 调整阈值
根据实际环境调整阈值常量（位于main.c开头）：

```c
// 紧急异常阈值
#define THRESHOLD_FLAME_EMERGENCY 70.0f
#define THRESHOLD_SMOKE_EMERGENCY 60.0f
#define THRESHOLD_TEMP_EMERGENCY 50.0f
#define THRESHOLD_HEAT_EMERGENCY 80.0f

// 一般告警阈值
#define THRESHOLD_FLAME_WARNING 40.0f
#define THRESHOLD_SMOKE_WARNING 30.0f
#define THRESHOLD_TEMP_HIGH 35.0f
#define THRESHOLD_TEMP_LOW 10.0f
#define THRESHOLD_HUMIDITY_HIGH 80.0f
#define THRESHOLD_HUMIDITY_LOW 20.0f
```

### 紧急模式恢复
当触发紧急模式后，设备会持续发送告警但不会自动恢复。需要：
1. 排除危险源
2. 手动重启ESP32设备
3. 或通过云端下发控制指令（需要实现相应的MQTT消息处理）

## 🎯 后续集成建议

### OneNet云平台配置
1. 在OneNet中配置规则引擎，监听紧急事件主题
2. 设置邮件/短信通知规则
3. 配置数据转发到SpringBoot后端

### SpringBoot后端处理
1. 接收OneNet转发的告警消息
2. 存储告警历史到数据库
3. 推送通知给相关人员
4. 生成告警统计报表

## 📝 日志示例

### 正常启动（开发模式）
```
[ALERT] === Edge Anomaly Detection System Started ===
[ALERT] Mode: DEVELOPMENT (Auto power-off disabled)
[ALERT] Emergency Thresholds:
[ALERT]   - Flame > 70.0%, Smoke > 60.0%, Temp > 50.0 C, Heat > 80.0%
```

### 正常启动（生产模式）
```
[ALERT] === Edge Anomaly Detection System Started ===
[ALERT] Mode: PRODUCTION (Auto power-off enabled)
[ALERT] Emergency Thresholds:
[ALERT]   - Flame > 70.0%, Smoke > 60.0%, Temp > 50.0 C, Heat > 80.0%
```

### 检测到一般告警
```
[ALERT] [WARNING] High temperature: 36.5 C (Threshold: 35.0 C)
[ALERT] Sending alert: {"id":"123","version":"1.0",...}
```

### 检测到紧急异常（开发模式）
```
[ALERT] [FIRE EMERGENCY] Flame: 75.0% (Threshold: 70.0%)
[ALERT] Development mode: Power-off disabled, only alerting
[ALERT] [DEV MODE] Emergency detected but continuing normal operation
[ENV] 温湿度：湿度 65.0% | 温度 25.50℃ | 火焰 75 | ...
[MQTT] Generated JSON: {...}
```

### 检测到紧急异常（生产模式）
```
[ALERT] [FIRE EMERGENCY] Flame: 75.0% (Threshold: 70.0%)
[ALERT] Auto power-off triggered!
[ALERT] All relays OFF
[ALERT] [EMERGENCY MODE] Device powered off for safety!
[ALERT] Manual reset required to resume operation.
```

### 远程电源控制
```
[MQTT] MQTT_EVENT_DATA
TOPIC=$sys/3HSBa0ZB1R/ESP_01/thing/property/set
DATA={"PowerControl":{"value":0}}
[MQTT] Property set command received
[ALERT] Remote power control: OFF
[ALERT] All relays OFF
```

## ⚠️ 注意事项

1. **响应时间**：边缘检测响应时间小于1秒
2. **断电保护**：紧急模式会切断所有继电器，确保安全
3. **网络依赖**：告警上报需要MQTT连接，但本地保护不依赖网络
4. **阈值校准**：部署前请根据实际环境校准传感器阈值
5. **误报处理**：传感器质量会影响检测准确度，建议定期校准

## 🔗 相关文件

- `main/main.c` - 主程序，包含异常检测逻辑
- `main/inc/config.h` - 配置文件，包含数据结构定义
- `main/headfile.h` - 头文件集合

## 📞 技术支持

如有问题，请检查：
1. 串口日志输出
2. MQTT连接状态
3. 传感器接线和校准
4. OneNet云平台配置
