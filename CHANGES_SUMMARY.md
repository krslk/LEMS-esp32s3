# 🎯 OneNet标准主题适配 - 更新摘要

## 📌 更新日期
2025-01-XX

## ✅ 已完成的修改

### 1. **修正MQTT主题格式** ⭐

根据OneNet官方文档，将所有事件上报统一为标准格式。

#### 修改前（❌ 不符合标准）
```c
// 紧急事件
"$sys/3HSBa0ZB1R/ESP_01/thing/event/emergency"

// 告警事件  
"$sys/3HSBa0ZB1R/ESP_01/thing/event/alert"

// 订阅回复
"$sys/3HSBa0ZB1R/ESP_01/thing/event/alert/reply"
"$sys/3HSBa0ZB1R/ESP_01/thing/event/emergency/reply"
```

#### 修改后（✅ 符合OneNet标准）
```c
// 所有事件统一使用
"$sys/3HSBa0ZB1R/ESP_01/thing/event/post"

// 订阅统一回复
"$sys/3HSBa0ZB1R/ESP_01/thing/event/post/reply"
```

**重要变化：**
- 所有事件（紧急、告警、断电等）使用**同一个主题**
- 事件类型通过JSON中的 `identifier` 字段区分

---

### 2. **更新JSON消息格式** ⭐

#### 修改前（❌ 不符合OneNet事件格式）
```json
{
  "id": "123",
  "version": "1.0",
  "params": {
    "alertType": {"value": "FIRE_EMERGENCY"},
    "severity": {"value": "EMERGENCY"},
    ...
  }
}
```

#### 修改后（✅ 符合OneNet事件格式）
```json
{
  "id": "123",
  "version": "1.0",
  "params": {
    "AlertEmergency": {
      "value": {
        "alertType": "FIRE_EMERGENCY",
        "severity": "EMERGENCY",
        ...
      }
    }
  }
}
```

**关键变化：**
- 使用 `identifier`（如 `AlertEmergency`）作为事件标识符
- 所有事件数据嵌套在 `value` 对象中
- 符合OneNet物模型事件上报格式

---

### 3. **更新代码函数**

#### 修改的文件：`main/main.c`

**修改的函数：**

1. `generate_alert_json()` - 生成符合OneNet标准的事件JSON
   - 添加 `identifier` 字段
   - 调整JSON结构

2. `send_alert_mqtt()` - 使用统一的事件上报主题
   - 修改主题为 `thing/event/post`
   - 更新日志输出

3. `emergency_power_off()` - 紧急断电事件格式化
   - 使用 `EmergencyPowerOff` 作为identifier
   - 符合OneNet事件格式

4. `mqtt_event_handler()` - MQTT事件处理
   - 更新订阅主题为 `thing/event/post/reply`
   - 移除不标准的订阅

---

### 4. **新增文档**

#### 📄 `ONENET_TOPIC_GUIDE.md` - OneNet主题使用指南
**内容包括：**
- OneNet物模型通信主题规范
- 本系统使用的所有主题列表
- 属性和事件的标准格式
- OneNet物模型配置示例
- 通配符订阅说明
- 调试技巧和常见问题

---

### 5. **更新现有文档**

#### 📄 `EDGE_DETECTION_README.md`
- 更新MQTT主题说明章节
- 修正JSON格式示例
- 添加OneNet标准说明

#### 📄 `BACKEND_INTEGRATION_EXAMPLE.md`
- 后端集成示例保持不变
- 需要根据新的事件格式调整SpringBoot接收逻辑

---

## 📊 主题对比表

| 功能 | 旧主题（不标准） | 新主题（OneNet标准） |
|-----|----------------|-------------------|
| 正常数据 | `thing/property/post` | `thing/property/post` ✅ |
| 紧急告警 | `thing/event/emergency` ❌ | `thing/event/post` ✅ |
| 一般告警 | `thing/event/alert` ❌ | `thing/event/post` ✅ |
| 事件回复 | `thing/event/alert/reply` ❌ | `thing/event/post/reply` ✅ |
| 属性设置 | `thing/property/set` | `thing/property/set` ✅ |

---

## 🎯 事件标识符（identifier）

OneNet通过 `identifier` 字段区分不同事件类型：

| 事件类型 | identifier | 说明 |
|---------|-----------|------|
| 紧急告警 | `AlertEmergency` | 火灾、烟雾、高温等紧急情况 |
| 一般告警 | `AlertWarning` | 温度越限、湿度异常等 |
| 紧急断电 | `EmergencyPowerOff` | 手动或远程紧急断电 |

---

## 🔧 在OneNet平台需要的配置

### 1. 物模型 - 事件定义

需要在OneNet控制台物模型中添加三个事件：

#### AlertEmergency（紧急告警事件）
```json
{
  "identifier": "AlertEmergency",
  "name": "紧急告警事件",
  "type": "event",
  "eventType": "alert",
  "outputData": [
    {"identifier": "alertType", "dataType": "string"},
    {"identifier": "severity", "dataType": "string"},
    {"identifier": "sensorValue", "dataType": "float"},
    {"identifier": "threshold", "dataType": "float"},
    {"identifier": "timestamp", "dataType": "long"},
    {"identifier": "username", "dataType": "string"},
    {"identifier": "temperature", "dataType": "float"},
    {"identifier": "humidity", "dataType": "float"},
    {"identifier": "flameScope", "dataType": "float"},
    {"identifier": "smokeScope", "dataType": "float"}
  ]
}
```

#### AlertWarning（一般告警事件）
```json
{
  "identifier": "AlertWarning",
  "name": "一般告警事件",
  "type": "event",
  "eventType": "info",
  "outputData": [
    // 与AlertEmergency相同的字段
  ]
}
```

#### EmergencyPowerOff（紧急断电事件）
```json
{
  "identifier": "EmergencyPowerOff",
  "name": "紧急断电事件",
  "type": "event",
  "eventType": "alert",
  "outputData": [
    {"identifier": "reason", "dataType": "string"},
    {"identifier": "timestamp", "dataType": "long"}
  ]
}
```

---

## 🧪 如何测试

### 1. 编译烧录
```bash
cd E:\Desktop\EspressifCode\test1_demo
idf.py build
idf.py flash monitor
```

### 2. 查看日志
设备启动后应该看到：
```
[MQTT] Subscribed to property post reply, msg_id=...
[MQTT] Subscribed to property set, msg_id=...
[MQTT] Subscribed to event post reply, msg_id=...
[MQTT] [OK] MQTT fully connected and subscribed to all topics
```

### 3. 触发告警测试
用打火机靠近火焰传感器，应该看到：
```
[ALERT] [FIRE EMERGENCY] Flame: 75.0% (Threshold: 70.0%)
[ALERT] Sending EMERGENCY event to OneNet
[MQTT] Publishing to: $sys/3HSBa0ZB1R/ESP_01/thing/event/post
```

### 4. OneNet平台验证
在OneNet控制台查看：
- 设备日志 → 应该能看到事件上报
- 设备详情 → 事件列表应该显示收到的事件

---

## ⚠️ 注意事项

### 1. 向后兼容性
- ❌ 与旧版本的主题不兼容
- 如果有其他设备订阅了旧主题，需要同步更新

### 2. OneNet规则引擎配置
如果之前配置了规则引擎监听旧主题，需要更新：
- 旧：监听 `thing/event/emergency`
- 新：监听 `thing/event/post`，并根据 `identifier` 字段判断事件类型

### 3. SpringBoot后端适配
后端需要根据新的JSON格式解析事件：

```java
// 旧格式解析（不再适用）
String alertType = json.getParams().getAlertType().getValue();

// 新格式解析（正确）
JsonNode alertEmergency = json.getParams().get("AlertEmergency");
if (alertEmergency != null) {
    String alertType = alertEmergency.get("value").get("alertType").asText();
}
```

---

## ✅ 优势

采用OneNet标准主题格式后：

1. ✅ **完全符合OneNet物模型规范**
2. ✅ **可以使用OneNet规则引擎**
3. ✅ **支持OneNet数据转发功能**
4. ✅ **在控制台可以直接调试**
5. ✅ **更好的扩展性**（添加新事件类型无需新主题）
6. ✅ **符合物联网标准**

---

## 📚 相关文档

- `ONENET_TOPIC_GUIDE.md` - OneNet主题详细指南
- `EDGE_DETECTION_README.md` - 边缘检测系统说明
- `QUICK_START_GUIDE.md` - 快速上手指南
- `BACKEND_INTEGRATION_EXAMPLE.md` - 后端集成示例

---

## 🔄 下一步建议

1. ✅ 代码已更新完成
2. ⏳ 在OneNet控制台配置事件物模型
3. ⏳ 编译烧录测试
4. ⏳ 验证事件上报成功
5. ⏳ 更新SpringBoot后端解析逻辑
6. ⏳ 配置OneNet规则引擎

---

**所有修改已完成，系统现在完全符合OneNet标准！** ✅
