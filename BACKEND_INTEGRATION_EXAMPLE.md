# 后端集成示例（OneNet标准）

## 🌐 OneNet云平台配置

### 1. 配置物模型事件（OneNet标准）

在OneNet物模型中添加以下**三个独立事件**：

#### AlertEmergency（紧急告警事件）
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

#### AlertWarning（一般告警事件）
```json
{
  "identifier": "AlertWarning",
  "name": "一般告警事件",
  "type": "event",
  "eventType": "info",
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

#### EmergencyPowerOff（紧急断电事件）
```json
{
  "identifier": "EmergencyPowerOff",
  "name": "紧急断电事件",
  "type": "event",
  "eventType": "alert",
  "outputData": [
    {"identifier": "reason", "dataType": "string", "name": "断电原因"},
    {"identifier": "username", "dataType": "string", "name": "用户名"}
  ]
}
```

### 2. 配置规则引擎（OneNet标准）

#### 规则1：紧急告警邮件通知
```javascript
// 触发条件 - AlertEmergency事件
event.params.AlertEmergency != null

// 动作：发送邮件通知
sendEmail({
  to: "admin@company.com",
  subject: "【紧急】工位异常告警",
  body: "设备ESP_01检测到" + event.params.AlertEmergency.value.alertType +
        "，传感器值：" + event.params.AlertEmergency.value.sensorValue
})
```

#### 规则2：一般告警邮件通知
```javascript
// 触发条件 - AlertWarning事件
event.params.AlertWarning != null

// 动作：发送提醒邮件
sendEmail({
  to: "admin@company.com",
  subject: "【提醒】工位告警",
  body: "设备ESP_01检测到" + event.params.AlertWarning.value.alertType +
        "，传感器值：" + event.params.AlertWarning.value.sensorValue
})
```

#### 规则3：紧急断电通知
```javascript
// 触发条件 - EmergencyPowerOff事件
event.params.EmergencyPowerOff != null

// 动作：发送紧急断电邮件
sendEmail({
  to: "admin@company.com",
  subject: "【紧急】设备断电告警",
  body: "设备ESP_01发生紧急断电，原因：" + event.params.EmergencyPowerOff.value.reason
})
```

#### 规则4：转发到SpringBoot后端
```javascript
// 触发条件 - 所有事件类型
event.params.AlertEmergency != null ||
event.params.AlertWarning != null ||
event.params.EmergencyPowerOff != null

// 动作：HTTP推送
httpPush({
  url: "http://your-backend.com/api/onenet/notify",
  method: "POST",
  headers: {"Content-Type": "application/json"},
  body: event
})
```

---

## ☕ SpringBoot后端处理

### 1. 实体类定义

```java
package com.example.iot.entity;

import lombok.Data;
import java.time.LocalDateTime;

/**
 * 传感器数据实体（正常数据上报）
 */
@Data
public class SensorData {
    private Long id;
    private String deviceName;
    private String productId;
    private Float temperature;      // 温度
    private Float humidity;         // 湿度
    private Float flameScope;       // 火焰强度
    private Float smokeScope;       // 烟雾浓度
    private Float lightLux;         // 光照强度
    private Float heatScope;        // 热度
    private Integer powerStatus;    // 电源状态
    private Integer lightStatus;    // 灯光状态
    private Integer relay1Status;   // 1号继电器状态
    private Integer relay2Status;   // 2号继电器状态
    private String username;        // 用户名
    private LocalDateTime dataTime; // 数据时间
}

/**
 * 告警事件实体（告警事件上报）
 */
@Data
public class AlertEvent {
    private Long id;
    private String deviceName;      // 设备名称
    private String alertType;       // 告警类型（如FIRE_EMERGENCY）
    private String severity;        // 严重程度（EMERGENCY/WARNING）
    private Float sensorValue;      // 触发告警的传感器值
    private Float threshold;        // 阈值
    private Long timestamp;         // 事件时间戳
    private String username;        // 用户名
    private Float temperature;      // 当时的温度
    private Float humidity;         // 当时的湿度
    private Float flameScope;       // 当时的火焰强度
    private Float smokeScope;       // 当时的烟雾浓度
    private LocalDateTime createdAt;// 创建时间
    private Boolean processed;      // 是否已处理
}
```

### 2. DTO类定义（根据实际OneNet推送格式）

```java
package com.example.iot.dto;

import lombok.Data;
import com.fasterxml.jackson.annotation.JsonProperty;
import java.util.Map;

/**
 * OneNet平台推送的数据格式
 * 实际格式示例：
 * {
 *   "notifyType": "property",
 *   "productId": "3HSBa0ZB1R",
 *   "messageType": "notify",
 *   "deviceName": "ESP_01",
 *   "data": {
 *     "id": "123",
 *     "params": {
 *       "CurrentTemperature": {"time": 1759980215771, "value": 24.00},
 *       "FlameScope": {"time": 1759980215771, "value": 90.7},
 *       ...
 *     }
 *   }
 * }
 */
@Data
public class OneNetNotifyDTO {
    private String notifyType;      // "property" 或 "event"
    private String productId;       // 产品ID
    private String messageType;     // "notify"
    private String deviceName;      // 设备名称
    private DataWrapper data;       // 实际数据
    
    @Data
    public static class DataWrapper {
        private String id;
        private Map<String, PropertyValue> params;  // 动态属性映射
    }
    
    @Data
    public static class PropertyValue {
        private Long time;      // OneNet添加的时间戳
        private Object value;   // 属性值（可能是String、Number等）
    }
}

/**
 * 告警事件DTO（事件上报格式）
 */
@Data
public class OneNetAlertEventDTO {
    private String notifyType;  // "event"
    private String productId;
    private String messageType;
    private String deviceName;
    private AlertData data;
    
    @Data
    public static class AlertData {
        private String id;
        private Map<String, AlertEventValue> params;
        
        @Data
        public static class AlertEventValue {
            private Long time;
            private AlertDetail value;
            
            @Data
            public static class AlertDetail {
                private String alertType;
                private String severity;
                private Float sensorValue;
                private Float threshold;
                private Long timestamp;
                private String username;
                private Float temperature;
                private Float humidity;
                private Float flameScope;
                private Float smokeScope;
            }
        }
    }
}
```

### 3. Controller接收OneNet推送

```java
package com.example.iot.controller;

import com.example.iot.dto.OneNetNotifyDTO;
import com.example.iot.dto.OneNetAlertEventDTO;
import com.example.iot.service.SensorDataService;
import com.example.iot.service.AlertService;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.bind.annotation.*;
import java.util.Map;
import java.util.HashMap;

@Slf4j
@RestController
@RequestMapping("/api/onenet")
public class OneNetController {
    
    @Autowired
    private SensorDataService sensorDataService;
    
    @Autowired
    private AlertService alertService;
    
    /**
     * 接收OneNet推送的所有消息（属性上报 + 事件上报）
     * 这是OneNet的HTTP推送回调接口
     * 
     * OneNet平台只能推送到一个指定的URL，通过messageType字段区分消息类型：
     * - messageType="property": 属性上报（正常传感器数据）
     * - messageType="event": 事件上报（告警事件）
     */
    @PostMapping("/notify")
    public Map<String, Object> receiveNotify(@RequestBody OneNetNotifyDTO notify) {
        log.info("收到OneNet推送: notifyType={}, messageType={}, deviceName={}", 
                 notify.getNotifyType(), notify.getMessageType(), notify.getDeviceName());
        log.debug("完整数据: {}", notify);
        
        try {
            // 根据messageType字段分发处理
            if ("property".equals(notify.getMessageType())) {
                // 属性上报（正常传感器数据）
                sensorDataService.handlePropertyData(notify);
                
            } else if ("event".equals(notify.getMessageType())) {
                // 事件上报（告警事件）
                alertService.handleEventData(notify);
            }
            
            // OneNet要求返回200状态码
            return Map.of(
                "code", 200,
                "msg", "success"
            );
            
        } catch (Exception e) {
            log.error("处理OneNet推送失败", e);
            return Map.of(
                "code", 500,
                "msg", "error: " + e.getMessage()
            );
        }
    }
    
    /**
     * 查询设备历史数据
     */
    @GetMapping("/data/{deviceName}")
    public Map<String, Object> getDeviceData(
            @PathVariable String deviceName,
            @RequestParam(required = false) Long startTime,
            @RequestParam(required = false) Long endTime,
            @RequestParam(defaultValue = "0") int page,
            @RequestParam(defaultValue = "20") int size) {
        
        return sensorDataService.queryDeviceData(deviceName, startTime, endTime, page, size);
    }
    
    /**
     * 查询告警历史
     */
    @GetMapping("/alerts")
    public Map<String, Object> getAlerts(
            @RequestParam(required = false) String deviceName,
            @RequestParam(required = false) String severity,
            @RequestParam(defaultValue = "0") int page,
            @RequestParam(defaultValue = "20") int size) {
        
        return alertService.queryAlerts(deviceName, severity, page, size);
    }
}
```

### 4. SensorDataService（处理正常传感器数据）

```java
package com.example.iot.service;

import com.example.iot.dto.OneNetNotifyDTO;
import com.example.iot.entity.SensorData;
import com.example.iot.mapper.SensorDataMapper;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.LocalDateTime;
import java.util.Map;

@Slf4j
@Service
public class SensorDataService {
    
    @Autowired
    private SensorDataMapper sensorDataMapper;
    
    @Autowired
    private WebSocketService webSocketService;
    
    /**
     * 处理OneNet推送的属性数据（正常传感器数据）
     */
    @Transactional
    public void handlePropertyData(OneNetNotifyDTO notify) {
        log.info("处理设备属性数据: {}", notify.getDeviceName());
        
        Map<String, OneNetNotifyDTO.PropertyValue> params = notify.getData().getParams();
        
        // 创建传感器数据实体
        SensorData sensorData = new SensorData();
        sensorData.setDeviceName(notify.getDeviceName());
        sensorData.setProductId(notify.getProductId());
        sensorData.setDataTime(new LocalDateTime.now());
        
        // 提取各个传感器数值（使用getAsXxx辅助方法）
        sensorData.setTemperature(getFloatValue(params, "CurrentTemperature"));
        sensorData.setHumidity(getFloatValue(params, "RelativeHumidity"));
        sensorData.setFlameScope(getFloatValue(params, "FlameScope"));
        sensorData.setSmokeScope(getFloatValue(params, "SmokeScope"));
        sensorData.setLightLux(getFloatValue(params, "LightLuxValue"));
        sensorData.setHeatScope(getFloatValue(params, "HeatScope"));
        
        sensorData.setPowerStatus(getIntValue(params, "PowerStatus"));
        sensorData.setLightStatus(getIntValue(params, "LightStatus"));
        sensorData.setRelay1Status(getIntValue(params, "RelayNum1Status"));
        sensorData.setRelay2Status(getIntValue(params, "RelayNum2Status"));
        
        sensorData.setUsername(getStringValue(params, "username"));
        
        // 保存到数据库
        sensorDataMapper.insert(sensorData);
        log.info("传感器数据已保存，温度: {}°C, 湿度: {}%", 
                sensorData.getTemperature(), sensorData.getHumidity());
        
        // 推送到前端实时显示
        webSocketService.pushSensorData(sensorData);
        
        // 检查是否需要触发报警（虽然ESP32已经做了边缘检测，这里可以做二次校验）
        checkThresholds(sensorData);
    }
    
    /**
     * 阈值检查（后端二次校验）
     */
    private void checkThresholds(SensorData data) {
        // 这里可以实现后端的阈值检查逻辑
        // 通常ESP32已经做了边缘检测，这里可以做更复杂的分析
        // 比如：趋势分析、多设备关联分析等
        
        if (data.getTemperature() != null && data.getTemperature() > 40.0f) {
            log.warn("后端检测到高温: {}°C (设备: {})", 
                    data.getTemperature(), data.getDeviceName());
            // 可以触发额外的通知或处理
        }
    }
    
    // 辅助方法：从OneNet PropertyValue中提取Float值
    private Float getFloatValue(Map<String, OneNetNotifyDTO.PropertyValue> params, String key) {
        OneNetNotifyDTO.PropertyValue pv = params.get(key);
        if (pv == null || pv.getValue() == null) return null;
        
        Object value = pv.getValue();
        if (value instanceof Number) {
            return ((Number) value).floatValue();
        }
        return null;
    }
    
    // 辅助方法：从OneNet PropertyValue中提取Integer值
    private Integer getIntValue(Map<String, OneNetNotifyDTO.PropertyValue> params, String key) {
        OneNetNotifyDTO.PropertyValue pv = params.get(key);
        if (pv == null || pv.getValue() == null) return null;
        
        Object value = pv.getValue();
        if (value instanceof Number) {
            return ((Number) value).intValue();
        }
        return null;
    }
    
    // 辅助方法：从OneNet PropertyValue中提取String值
    private String getStringValue(Map<String, OneNetNotifyDTO.PropertyValue> params, String key) {
        OneNetNotifyDTO.PropertyValue pv = params.get(key);
        if (pv == null || pv.getValue() == null) return null;
        return String.valueOf(pv.getValue());
    }
    
    /**
     * 查询设备历史数据
     */
    public Map<String, Object> queryDeviceData(String deviceName, Long startTime, 
                                               Long endTime, int page, int size) {
        // 实现数据查询逻辑
        return Map.of(
            "code", 200,
            "data", sensorDataMapper.selectByDevice(deviceName, startTime, endTime, page, size)
        );
    }
}
```

### 5. AlertService业务逻辑

```java
package com.example.iot.service;

import com.example.iot.dto.OneNetNotifyDTO;
import com.example.iot.entity.AlertEvent;
import com.example.iot.mapper.AlertMapper;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.mail.SimpleMailMessage;
import org.springframework.mail.javamail.JavaMailSender;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.LocalDateTime;
import java.util.Map;

@Slf4j
@Service
public class AlertService {
    
    @Autowired
    private AlertMapper alertMapper;
    
    @Autowired
    private JavaMailSender mailSender;
    
    @Autowired
    private WebSocketService webSocketService;
    
    /**
     * 处理OneNet推送的事件数据（告警事件）
     */
    @Transactional
    public void handleEventData(OneNetNotifyDTO notify) {
        log.info("处理设备事件: {}", notify.getDeviceName());
        
        Map<String, OneNetNotifyDTO.PropertyValue> params = notify.getData().getParams();
        
        // OneNet的事件推送格式：
        // params中会有 "AlertEmergency" 或 "AlertWarning" 键
        // 其value是一个嵌套的对象，包含告警详情
        
        // 检查是否为紧急告警（OneNet标准格式）
        if (params.containsKey("AlertEmergency")) {
            handleAlertEvent(notify, params.get("AlertEmergency"), "EMERGENCY");
        }
        // 检查是否为一般告警（OneNet标准格式）
        else if (params.containsKey("AlertWarning")) {
            handleAlertEvent(notify, params.get("AlertWarning"), "WARNING");
        }
        // 检查是否为紧急断电事件（OneNet标准格式）
        else if (params.containsKey("EmergencyPowerOff")) {
            handleEmergencyPowerOff(notify, params.get("EmergencyPowerOff"));
        }
    }
    
    /**
     * 处理告警事件（OneNet标准格式）
     */
    private void handleAlertEvent(OneNetNotifyDTO notify,
                                  OneNetNotifyDTO.PropertyValue alertValue,
                                  String severity) {
        // alertValue.getValue()是一个Map，包含告警详情
        @SuppressWarnings("unchecked")
        Map<String, Object> alertDetail = (Map<String, Object>) alertValue.getValue();
        
        String alertType = (String) alertDetail.get("alertType");
        String severity = (String) alertDetail.get("severity");
        Float sensorValue = getFloatFromMap(alertDetail, "sensorValue");
        Float threshold = getFloatFromMap(alertDetail, "threshold");
        
        log.warn("收到{}告警: 类型={}, 传感器值={}, 阈值={}", 
                severity, alertType, sensorValue, threshold);
        
        // 创建告警事件实体
        AlertEvent event = new AlertEvent();
        event.setDeviceName(notify.getDeviceName());
        event.setAlertType(alertType);
        event.setSeverity(severity);
        event.setSensorValue(sensorValue);
        event.setThreshold(threshold);
        event.setTimestamp(getLongFromMap(alertDetail, "timestamp"));
        event.setUsername((String) alertDetail.get("username"));
        event.setTemperature(getFloatFromMap(alertDetail, "temperature"));
        event.setHumidity(getFloatFromMap(alertDetail, "humidity"));
        event.setFlameScope(getFloatFromMap(alertDetail, "flameScope"));
        event.setSmokeScope(getFloatFromMap(alertDetail, "smokeScope"));
        event.setCreatedAt(LocalDateTime.now());
        event.setProcessed(false);
        
        // 存储到数据库
        alertMapper.insert(event);
        log.info("告警已保存到数据库，ID: {}", event.getId());
        
        // 根据严重程度执行不同操作
        if ("EMERGENCY".equals(severity)) {
            handleEmergency(event);
        } else {
            handleWarning(event);
        }
        
        // 推送到前端（WebSocket）
        webSocketService.pushAlert(event, severity);
    }
    
    /**
     * 处理紧急断电事件
     */
    private void handleEmergencyPowerOff(OneNetNotifyDTO notify, 
                                         OneNetNotifyDTO.PropertyValue powerOffValue) {
        @SuppressWarnings("unchecked")
        Map<String, Object> detail = (Map<String, Object>) powerOffValue.getValue();
        
        String reason = (String) detail.get("reason");
        Long timestamp = getLongFromMap(detail, "timestamp");
        
        log.error("收到紧急断电事件: 设备={}, 原因={}", notify.getDeviceName(), reason);
        
        // 记录到数据库或发送特殊通知
        // ... 实现具体逻辑
    }
    
    /**
     * 处理紧急告警
     */
    private void handleEmergency(AlertEvent event) {
        log.error("🚨 紧急告警！类型: {}, 设备: {}, 值: {}", 
            event.getAlertType(), event.getDeviceId(), event.getSensorValue());
        
        // 1. 发送邮件通知
        sendEmailAlert(event, true);
        
        // 2. 发送短信通知（需集成短信服务）
        // smsService.sendAlert(event);
        
        // 3. 推送App通知
        // pushService.sendAppNotification(event);
        
        // 4. 触发自动化处理
        triggerAutomation(event);
    }
    
    /**
     * 处理一般告警
     */
    private void handleWarning(AlertEvent event) {
        log.warn("⚠️ 一般告警：类型: {}, 设备: {}, 值: {}", 
            event.getAlertType(), event.getDeviceName(), event.getSensorValue());
        
        // 仅发送邮件通知
        sendEmailAlert(event, false);
    }
    
    // 辅助方法：从Map中提取Float值
    private Float getFloatFromMap(Map<String, Object> map, String key) {
        Object value = map.get(key);
        if (value == null) return null;
        if (value instanceof Number) {
            return ((Number) value).floatValue();
        }
        return null;
    }
    
    // 辅助方法：从Map中提取Long值
    private Long getLongFromMap(Map<String, Object> map, String key) {
        Object value = map.get(key);
        if (value == null) return null;
        if (value instanceof Number) {
            return ((Number) value).longValue();
        }
        return null;
    }
    
    /**
     * 发送邮件告警
     */
    private void sendEmailAlert(AlertEvent event, boolean isUrgent) {
        try {
            SimpleMailMessage message = new SimpleMailMessage();
            message.setFrom("noreply@company.com");
            message.setTo("admin@company.com");
            
            if (isUrgent) {
                message.setSubject("【紧急】工位异常告警 - " + event.getAlertType());
            } else {
                message.setSubject("【提醒】工位告警 - " + event.getAlertType());
            }
            
            message.setText(String.format(
                "设备名称: %s\n" +
                "告警类型: %s\n" +
                "严重程度: %s\n" +
                "传感器值: %.2f (阈值: %.2f)\n" +
                "温度: %.2f°C\n" +
                "湿度: %.1f%%\n" +
                "火焰强度: %.1f%%\n" +
                "烟雾浓度: %.1f%%\n" +
                "用户ID: %s\n" +
                "时间: %s\n\n" +
                "请立即处理！",
                event.getDeviceName(),
                event.getAlertType(),
                event.getSeverity(),
                event.getSensorValue(),
                event.getThreshold(),
                event.getTemperature(),
                event.getHumidity(),
                event.getFlameScope(),
                event.getSmokeScope(),
                event.getUsername(),
                event.getCreatedAt()
            ));
            
            mailSender.send(message);
            log.info("告警邮件已发送");
        } catch (Exception e) {
            log.error("发送告警邮件失败", e);
        }
    }
    
    /**
     * 触发自动化处理
     */
    private void triggerAutomation(AlertEvent event) {
        // 根据告警类型执行自动化操作
        switch (event.getAlertType()) {
            case "FIRE_EMERGENCY":
            case "SMOKE_EMERGENCY":
                // 1. 通知消防部门
                // 2. 启动自动喷水系统
                // 3. 疏散警报
                log.info("触发火灾应急预案");
                break;
                
            case "TEMP_EMERGENCY":
            case "HEAT_EMERGENCY":
                // 启动降温系统
                log.info("触发降温措施");
                break;
                
            default:
                log.debug("无需自动化处理");
        }
    }
    
    /**
     * 查询告警历史
     */
    public Map<String, Object> queryAlerts(String deviceName, String severity, 
                                          int page, int size) {
        return Map.of(
            "code", 200,
            "data", alertMapper.selectAlerts(deviceName, severity, page, size)
        );
    }
}
```

### 6. Mapper接口

```java
package com.example.iot.mapper;

import com.example.iot.entity.AlertEvent;
import com.example.iot.entity.SensorData;
import org.apache.ibatis.annotations.*;

import java.util.List;

/**
 * 传感器数据Mapper
 */
@Mapper
public interface SensorDataMapper {
    
    @Insert("INSERT INTO sensor_data " +
            "(device_name, product_id, temperature, humidity, flame_scope, " +
            "smoke_scope, light_lux, heat_scope, power_status, light_status, " +
            "relay1_status, relay2_status, username, data_time) " +
            "VALUES " +
            "(#{deviceName}, #{productId}, #{temperature}, #{humidity}, " +
            "#{flameScope}, #{smokeScope}, #{lightLux}, #{heatScope}, " +
            "#{powerStatus}, #{lightStatus}, #{relay1Status}, #{relay2Status}, " +
            "#{username}, #{dataTime})")
    @Options(useGeneratedKeys = true, keyProperty = "id")
    int insert(SensorData data);
    
    @Select("<script>" +
            "SELECT * FROM sensor_data " +
            "WHERE device_name = #{deviceName} " +
            "<if test='startTime != null'>AND data_time >= FROM_UNIXTIME(#{startTime}/1000)</if> " +
            "<if test='endTime != null'>AND data_time <= FROM_UNIXTIME(#{endTime}/1000)</if> " +
            "ORDER BY data_time DESC " +
            "LIMIT #{size} OFFSET #{page}" +
            "</script>")
    List<SensorData> selectByDevice(@Param("deviceName") String deviceName,
                                    @Param("startTime") Long startTime,
                                    @Param("endTime") Long endTime,
                                    @Param("page") int page,
                                    @Param("size") int size);
}

/**
 * 告警事件Mapper
 */
@Mapper
public interface AlertMapper {
    
    @Insert("INSERT INTO alert_events " +
            "(device_name, alert_type, severity, sensor_value, threshold, " +
            "timestamp, username, temperature, humidity, flame_scope, " +
            "smoke_scope, created_at, processed) " +
            "VALUES " +
            "(#{deviceName}, #{alertType}, #{severity}, #{sensorValue}, " +
            "#{threshold}, #{timestamp}, #{username}, #{temperature}, " +
            "#{humidity}, #{flameScope}, #{smokeScope}, #{createdAt}, #{processed})")
    @Options(useGeneratedKeys = true, keyProperty = "id")
    int insert(AlertEvent event);
    
    @Select("<script>" +
            "SELECT * FROM alert_events " +
            "WHERE 1=1 " +
            "<if test='deviceName != null'>AND device_name = #{deviceName}</if> " +
            "<if test='severity != null'>AND severity = #{severity}</if> " +
            "ORDER BY created_at DESC " +
            "LIMIT #{size} OFFSET #{offset}" +
            "</script>")
    List<AlertEvent> selectAlerts(@Param("deviceName") String deviceName,
                                  @Param("severity") String severity,
                                  @Param("offset") int offset,
                                  @Param("size") int size);
}
```

### 6. WebSocket实时推送

```java
package com.example.iot.service;

import com.example.iot.entity.AlertEvent;
import com.fasterxml.jackson.databind.ObjectMapper;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.messaging.simp.SimpMessagingTemplate;
import org.springframework.stereotype.Service;

@Slf4j
@Service
public class WebSocketService {
    
    @Autowired
    private SimpMessagingTemplate messagingTemplate;
    
    @Autowired
    private ObjectMapper objectMapper;
    
    /**
     * 推送告警到前端
     */
    public void pushAlert(AlertEvent event, String severity) {
        try {
            // 推送到所有订阅了 /topic/alerts 的客户端
            messagingTemplate.convertAndSend("/topic/alerts", event);

            // 如果是紧急告警，额外推送到紧急通道
            if ("EMERGENCY".equals(severity)) {
                messagingTemplate.convertAndSend("/topic/emergency", event);
            }

            log.info("告警已通过WebSocket推送到前端");
        } catch (Exception e) {
            log.error("WebSocket推送失败", e);
        }
    }
}
```

### 8. 数据库表结构

```sql
-- 传感器数据表（存储正常的数据上报）
CREATE TABLE sensor_data (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    device_name VARCHAR(50) NOT NULL COMMENT '设备名称',
    product_id VARCHAR(50) NOT NULL COMMENT '产品ID',
    temperature FLOAT COMMENT '温度(°C)',
    humidity FLOAT COMMENT '湿度(%)',
    flame_scope FLOAT COMMENT '火焰强度(%)',
    smoke_scope FLOAT COMMENT '烟雾浓度(%)',
    light_lux FLOAT COMMENT '光照强度',
    heat_scope FLOAT COMMENT '热度(%)',
    power_status TINYINT COMMENT '电源状态',
    light_status TINYINT COMMENT '灯光状态',
    relay1_status TINYINT COMMENT '1号继电器状态',
    relay2_status TINYINT COMMENT '2号继电器状态',
    username VARCHAR(50) COMMENT '用户名',
    data_time DATETIME NOT NULL COMMENT '数据时间',
    INDEX idx_device_name (device_name),
    INDEX idx_data_time (data_time),
    INDEX idx_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='传感器数据表';

-- 告警事件表（存储告警事件）
CREATE TABLE alert_events (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    device_name VARCHAR(50) NOT NULL COMMENT '设备名称',
    alert_type VARCHAR(50) NOT NULL COMMENT '告警类型',
    severity VARCHAR(20) NOT NULL COMMENT '严重程度: EMERGENCY/WARNING',
    sensor_value FLOAT NOT NULL COMMENT '传感器值',
    threshold FLOAT NOT NULL COMMENT '阈值',
    timestamp BIGINT NOT NULL COMMENT '事件时间戳',
    username VARCHAR(50) COMMENT '用户名',
    temperature FLOAT COMMENT '温度',
    humidity FLOAT COMMENT '湿度',
    flame_scope FLOAT COMMENT '火焰强度',
    smoke_scope FLOAT COMMENT '烟雾浓度',
    created_at DATETIME NOT NULL COMMENT '创建时间',
    processed BOOLEAN DEFAULT FALSE COMMENT '是否已处理',
    INDEX idx_device_name (device_name),
    INDEX idx_severity (severity),
    INDEX idx_created_at (created_at),
    INDEX idx_alert_type (alert_type)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='告警事件表';
```

---

## 📊 前端展示示例

### Vue.js 实时告警组件

```vue
<template>
  <div class="alert-dashboard">
    <h2>实时告警监控</h2>
    
    <!-- 紧急告警 -->
    <div v-if="emergencyAlerts.length > 0" class="emergency-section">
      <h3>🚨 紧急告警</h3>
      <div v-for="alert in emergencyAlerts" :key="alert.id" class="alert-card emergency">
        <div class="alert-header">
          <span class="alert-type">{{ alert.alertType }}</span>
          <span class="alert-time">{{ formatTime(alert.createdAt) }}</span>
        </div>
        <div class="alert-body">
          <p>设备: {{ alert.deviceId }}</p>
          <p>传感器值: <strong>{{ alert.sensorValue }}</strong> (阈值: {{ alert.threshold }})</p>
          <p>温度: {{ alert.temperature }}°C | 湿度: {{ alert.humidity }}%</p>
        </div>
      </div>
    </div>
    
    <!-- 一般告警 -->
    <div class="warning-section">
      <h3>⚠️ 一般告警</h3>
      <el-table :data="warningAlerts" style="width: 100%">
        <el-table-column prop="alertType" label="类型" width="180" />
        <el-table-column prop="sensorValue" label="传感器值" width="120" />
        <el-table-column prop="threshold" label="阈值" width="100" />
        <el-table-column prop="createdAt" label="时间" width="180" />
        <el-table-column label="操作">
          <template #default="{ row }">
            <el-button size="small" @click="handleAlert(row)">处理</el-button>
          </template>
        </el-table-column>
      </el-table>
    </div>
  </div>
</template>

<script>
import SockJS from 'sockjs-client'
import Stomp from 'stompjs'

export default {
  data() {
    return {
      stompClient: null,
      emergencyAlerts: [],
      warningAlerts: []
    }
  },
  
  mounted() {
    this.connectWebSocket()
  },
  
  methods: {
    connectWebSocket() {
      const socket = new SockJS('http://localhost:8080/ws')
      this.stompClient = Stomp.over(socket)
      
      this.stompClient.connect({}, () => {
        // 订阅紧急告警
        this.stompClient.subscribe('/topic/emergency', (message) => {
          const alert = JSON.parse(message.body)
          this.emergencyAlerts.unshift(alert)
          this.showNotification(alert, true)
        })

        // 订阅一般告警
        this.stompClient.subscribe('/topic/alerts', (message) => {
          const alert = JSON.parse(message.body)
          if (alert.severity === 'WARNING') {
            this.warningAlerts.unshift(alert)
            this.showNotification(alert, false)
          }
        })
      })
    },
    
    showNotification(alert, isUrgent) {
      const title = isUrgent ? '🚨 紧急告警' : '⚠️ 一般告警'
      this.$notify({
        title: title,
        message: `${alert.alertType}: ${alert.sensorValue}`,
        type: isUrgent ? 'error' : 'warning',
        duration: isUrgent ? 0 : 5000
      })
      
      // 播放告警音
      if (isUrgent) {
        this.playAlertSound()
      }
    },
    
    playAlertSound() {
      const audio = new Audio('/alert.mp3')
      audio.play()
    }
  }
}
</script>
```

---

## 🔔 完整工作流程

```
ESP32边缘检测
      ↓
  触发告警
      ↓
MQTT发送到OneNet
      ↓
OneNet规则引擎
   ↙      ↘
邮件通知  HTTP转发
           ↓
    SpringBoot后端
      ↓   ↓   ↓
    数据库 邮件 WebSocket
                ↓
              前端实时显示
```

这就是完整的边缘到云端的告警处理方案！

## OneNet平台配置说明

### 1. HTTP推送配置（OneNet标准）
OneNet平台只能配置一个HTTP推送回调URL，所有消息都会推送到这个URL：

**推送URL配置：**
```
http://your-domain.com/api/onenet/notify
```

**消息类型区分（OneNet标准）：**
- OneNet通过`messageType`字段区分消息类型
- `messageType="property"`: 属性上报（正常传感器数据）
- `messageType="event"`: 事件上报（告警事件）

**推送规则配置：**
- 在OneNet平台配置推送规则
- 选择需要推送的数据类型（属性上报、事件上报）
- 所有消息都会推送到同一个URL，后端根据`messageType`字段进行分发处理

**事件格式说明：**
- 紧急告警：`params.AlertEmergency` 嵌套对象
- 一般告警：`params.AlertWarning` 嵌套对象
- 紧急断电：`params.EmergencyPowerOff` 嵌套对象

### 2. 数据流说明
```
ESP32设备 → MQTT → OneNet平台 → HTTP推送 → SpringBoot后端
                                    ↓
                              根据messageType分发
                                    ↓
                        属性数据 → SensorDataService
                        事件数据 → AlertService
```

### 3. 测试验证（OneNet标准）
1. **属性上报测试**：ESP32正常上报传感器数据，后端应收到`messageType="property"`的消息
2. **事件上报测试**：ESP32触发告警，后端应收到`messageType="event"`的消息，`params`中包含`AlertEmergency`或`AlertWarning`
3. **紧急断电测试**：ESP32紧急断电，后端应收到`messageType="event"`的消息，`params`中包含`EmergencyPowerOff`
4. **日志验证**：查看后端日志确认消息类型分发正确

**预期日志输出：**
```
收到OneNet推送: notifyType=event, messageType=event, deviceName=ESP_01
处理设备事件: ESP_01
收到EMERGENCY告警: 类型=FIRE_EMERGENCY, 传感器值=75.0, 阈值=70.0
```
