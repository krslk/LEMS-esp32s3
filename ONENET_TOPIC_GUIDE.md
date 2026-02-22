# OneNet MQTT主题使用指南

## 📡 OneNet物模型通信主题规范

根据OneNet官方文档，本系统严格遵循OneNet物模型通信主题标准。

### 主题格式说明

```
$sys/{pid}/{device-name}/thing/{function}/{operation}
```

- `{pid}`: 产品ID（本项目：3HSBa0ZB1R）
- `{device-name}`: 设备名称（本项目：ESP_01）
- `{function}`: 功能类别（property/event/service）
- `{operation}`: 操作类型（post/set/get等）

---

## 📤 本系统使用的主题

### 1. 属性相关主题

#### ✅ 属性上报（正常数据上报）
**主题：** `$sys/3HSBa0ZB1R/ESP_01/thing/property/post`  
**方向：** 设备 → OneNet  
**权限：** 发布  
**频率：** 每5秒一次  

**消息示例：**
```json
{
  "id": "123",
  "version": "1.0",
  "params": {
    "CurrentTemperature": {"value": 25.5},
    "RelativeHumidity": {"value": 65.0},
    "FlameScope": {"value": 15.2},
    "SmokeScope": {"value": 10.5},
    "LightLuxValue": {"value": 450.0},
    "HeatScope": {"value": 30.0},
    "PowerStatus": {"value": 1},
    "LightStatus": {"value": 1},
    "RelayNum1Status": {"value": 1},
    "RelayNum2Status": {"value": 1},
    "username": {"value": "USER001"}
  }
}
```

#### ✅ 属性上报响应
**主题：** `$sys/3HSBa0ZB1R/ESP_01/thing/property/post/reply`  
**方向：** OneNet → 设备  
**权限：** 订阅  

#### ✅ 属性设置（远程控制）
**主题：** `$sys/3HSBa0ZB1R/ESP_01/thing/property/set`  
**方向：** OneNet → 设备  
**权限：** 订阅  

**消息示例（远程控制电源）：**
```json
{
  "id": "123",
  "version": "1.0",
  "params": {
    "PowerControl": {"value": 0}
  }
}
```

#### ✅ 属性设置响应
**主题：** `$sys/3HSBa0ZB1R/ESP_01/thing/property/set_reply`  
**方向：** 设备 → OneNet  
**权限：** 发布  

---

### 2. 事件相关主题

#### ✅ 事件上报（告警/紧急事件）
**主题：** `$sys/3HSBa0ZB1R/ESP_01/thing/event/post`  
**方向：** 设备 → OneNet  
**权限：** 发布  

**⚠️ 重要说明：**
- 所有事件（紧急告警、一般告警、断电事件等）都使用**同一个主题**
- 事件类型通过JSON消息体中的 `identifier` 字段区分
- **不要**为不同事件创建不同的主题

**消息示例1 - 紧急告警事件：**
```json
{
  "id": "123",
  "version": "1.0",
  "params": {
    "AlertEmergency": {
      "value": {
        "alertType": "FIRE_EMERGENCY",
        "severity": "EMERGENCY",
        "sensorValue": 75.0,
        "threshold": 70.0,
        "timestamp": 1698765432,
        "username": "USER001",
        "temperature": 45.2,
        "humidity": 65.0,
        "flameScope": 75.0,
        "smokeScope": 45.0
      }
    }
  }
}
```

**消息示例2 - 一般告警事件：**
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
        "timestamp": 1698765432,
        "username": "USER001",
        "temperature": 36.5,
        "humidity": 60.0,
        "flameScope": 10.0,
        "smokeScope": 5.0
      }
    }
  }
}
```

**消息示例3 - 紧急断电事件：**
```json
{
  "id": "123",
  "version": "1.0",
  "params": {
    "EmergencyPowerOff": {
      "value": {
        "reason": "Remote emergency command",
        "timestamp": 1698765432
      }
    }
  }
}
```

#### ✅ 事件上报响应
**主题：** `$sys/3HSBa0ZB1R/ESP_01/thing/event/post/reply`  
**方向：** OneNet → 设备  
**权限：** 订阅  

---

## 🔧 OneNet物模型配置

### 在OneNet控制台配置事件

需要在物模型中定义以下事件：

#### 1. 紧急告警事件
```json
{
  "identifier": "AlertEmergency",
  "name": "紧急告警事件",
  "type": "event",
  "eventType": "alert",
  "outputData": [
    {"identifier": "alertType", "dataType": "string", "name": "告警类型"},
    {"identifier": "severity", "dataType": "string", "name": "严重程度"},
    {"identifier": "sensorValue", "dataType": "float", "name": "传感器值"},
    {"identifier": "threshold", "dataType": "float", "name": "阈值"},
    {"identifier": "timestamp", "dataType": "long", "name": "时间戳"},
    {"identifier": "username", "dataType": "string", "name": "用户名"},
    {"identifier": "temperature", "dataType": "float", "name": "温度"},
    {"identifier": "humidity", "dataType": "float", "name": "湿度"},
    {"identifier": "flameScope", "dataType": "float", "name": "火焰强度"},
    {"identifier": "smokeScope", "dataType": "float", "name": "烟雾浓度"}
  ]
}
```

#### 2. 一般告警事件
```json
{
  "identifier": "AlertWarning",
  "name": "一般告警事件",
  "type": "event",
  "eventType": "info",
  "outputData": [
    // 与紧急告警相同的字段定义
  ]
}
```

#### 3. 紧急断电事件
```json
{
  "identifier": "EmergencyPowerOff",
  "name": "紧急断电事件",
  "type": "event",
  "eventType": "alert",
  "outputData": [
    {"identifier": "reason", "dataType": "string", "name": "断电原因"},
    {"identifier": "timestamp", "dataType": "long", "name": "时间戳"}
  ]
}
```

---

## 📊 通配符订阅（可选）

OneNet支持使用通配符订阅多个主题：

### 订阅所有物模型相关主题
```c
esp_mqtt_client_subscribe(client, "$sys/3HSBa0ZB1R/ESP_01/thing/#", 0);
```

### 订阅所有属性相关主题
```c
esp_mqtt_client_subscribe(client, "$sys/3HSBa0ZB1R/ESP_01/thing/property/#", 0);
```

### 订阅所有事件相关主题
```c
esp_mqtt_client_subscribe(client, "$sys/3HSBa0ZB1R/ESP_01/thing/event/#", 0);
```

**注意：** 通配符订阅便于调试，但正式环境建议只订阅需要的具体主题以节省资源。

---

## ✅ 代码实现对照

### 发布消息

```c
// ✅ 正确：属性上报
esp_mqtt_client_publish(client, 
    "$sys/3HSBa0ZB1R/ESP_01/thing/property/post",
    json_data, 0, 1, 0);

// ✅ 正确：事件上报（所有事件都用这个主题）
esp_mqtt_client_publish(client,
    "$sys/3HSBa0ZB1R/ESP_01/thing/event/post",
    alert_json, 0, 1, 0);

// ❌ 错误：不要为不同事件创建不同主题
esp_mqtt_client_publish(client,
    "$sys/3HSBa0ZB1R/ESP_01/thing/event/emergency",  // 错误！
    json_data, 0, 1, 0);
```

### 订阅消息

```c
// ✅ 正确：订阅标准回复主题
esp_mqtt_client_subscribe(client,
    "$sys/3HSBa0ZB1R/ESP_01/thing/property/post/reply", 0);

esp_mqtt_client_subscribe(client,
    "$sys/3HSBa0ZB1R/ESP_01/thing/property/set", 0);

esp_mqtt_client_subscribe(client,
    "$sys/3HSBa0ZB1R/ESP_01/thing/event/post/reply", 0);

// ❌ 错误：这些主题不符合OneNet标准
esp_mqtt_client_subscribe(client,
    "$sys/3HSBa0ZB1R/ESP_01/thing/event/alert/reply", 0);  // 错误！
```

---

## 🔍 调试技巧

### 1. 开启通配符订阅查看所有消息

在 `mqtt_event_handler()` 的 `MQTT_EVENT_CONNECTED` 中取消注释：

```c
msg_id = esp_mqtt_client_subscribe(
    gHandle_mqttClient, "$sys/3HSBa0ZB1R/ESP_01/thing/#", 0);
ESP_LOGI(MQTT_LOG, "Subscribed to all thing topics, msg_id=%d", msg_id);
```

### 2. 打印收到的所有MQTT消息

```c
case MQTT_EVENT_DATA:
    ESP_LOGI(MQTT_LOG, "MQTT_EVENT_DATA");
    ESP_LOGI(MQTT_LOG, "TOPIC=%.*s", event->topic_len, event->topic);
    ESP_LOGI(MQTT_LOG, "DATA=%.*s", event->data_len, event->data);
    break;
```

### 3. 在OneNet控制台查看设备日志

OneNet控制台 → 设备管理 → 选择设备 → 设备日志

可以看到：
- 设备上报的消息
- 平台下发的消息
- 通信错误日志

---

## 📝 常见问题

### Q1: 为什么所有事件都用同一个主题？
**A:** 这是OneNet物模型的标准规范。事件类型通过JSON中的 `identifier` 字段区分，而不是通过不同的主题。这样设计更加灵活和标准化。

### Q2: 如何区分紧急事件和一般事件？
**A:** 通过JSON消息体中的 `identifier` 字段：
- `AlertEmergency` = 紧急告警事件
- `AlertWarning` = 一般告警事件
- `EmergencyPowerOff` = 紧急断电事件

### Q3: OneNet会自动回复吗？
**A:** 是的，OneNet会在以下主题回复：
- 属性上报回复：`thing/property/post/reply`
- 事件上报回复：`thing/event/post/reply`

### Q4: 可以自定义主题吗？
**A:** 不建议。使用OneNet标准主题可以：
- 与OneNet规则引擎无缝集成
- 使用OneNet的数据转发功能
- 在控制台直接调试和查看
- 符合物联网标准规范

---

## 📚 参考资料

- [OneNet物模型通信主题文档](https://open.iot.10086.cn/doc/v5/develop/detail/639)
- [OneJSON数据格式](https://open.iot.10086.cn/doc/v5/develop/detail/640)
- [设备接入MQTT](https://open.iot.10086.cn/doc/v5/develop/detail/635)

---

## ✅ 检查清单

配置前请确认：

- [ ] 在OneNet创建了产品和设备
- [ ] 记录了产品ID（pid）和设备名称（device-name）
- [ ] 在物模型中定义了所有属性
- [ ] 在物模型中定义了所有事件
- [ ] 使用正确的主题格式
- [ ] 消息JSON格式符合OneJSON规范
- [ ] 订阅了必要的回复主题

---

**本系统已完全按照OneNet标准实现！** ✅
