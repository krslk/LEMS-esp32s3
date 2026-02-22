# 🚀 ESP32边缘异常检测 - 快速上手指南

## 📦 当前配置（开发模式）

您的系统已配置为**开发模式**，特点如下：

✅ **检测所有异常** - 火焰、烟雾、温度、湿度等  
✅ **上报告警消息** - 发送到OneNet云平台  
✅ **继续正常运行** - 不会自动断电  
❌ **不自动断电** - 方便开发调试  

## 🔧 如何切换到生产模式？

当开发测试完成，准备正式部署时：

### 步骤1：修改配置
打开 `main/main.c`，找到第52行：

```c
#define ENABLE_AUTO_POWER_OFF 0  // 改为 1 启用生产模式
```

改为：

```c
#define ENABLE_AUTO_POWER_OFF 1  // 生产模式：启用自动断电保护
```

### 步骤2：重新编译烧录
```bash
idf.py build
idf.py flash
```

### 步骤3：验证模式
启动后查看日志，应该显示：
```
[ALERT] Mode: PRODUCTION (Auto power-off enabled)
```

---

## 📊 异常检测阈值对照表

### 🚨 紧急异常（会触发告警，生产模式下会断电）

| 传感器 | 阈值 | 开发模式行为 | 生产模式行为 |
|-------|------|------------|-------------|
| 🔥 火焰 | > 70% | 告警 + 继续运行 | 告警 + **立即断电** |
| 💨 烟雾 | > 60% | 告警 + 继续运行 | 告警 + **立即断电** |
| 🌡️ 温度 | > 50℃ | 告警 + 继续运行 | 告警 + **立即断电** |
| 🔥 热度 | > 80% | 告警 + 继续运行 | 告警 + **立即断电** |

### ⚠️ 一般告警（两种模式都只告警，不断电）

| 传感器 | 阈值 | 行为 |
|-------|------|------|
| 火焰 | > 40% | 告警 |
| 烟雾 | > 30% | 告警 |
| 温度（高） | > 35℃ | 告警 |
| 温度（低） | < 10℃ | 告警 |
| 湿度（高） | > 80% | 告警 |
| 湿度（低） | < 20% | 告警 |

---

## 🎯 预留接口使用指南

### 接口1：`emergency_power_off()` - 紧急断电

**功能：** 无论当前是什么模式，都可以强制断电

**使用场景：**
- 远程紧急关机
- 其他安全机制触发
- 手动测试断电功能

**代码示例：**
```c
// 在需要紧急断电的地方调用
emergency_power_off("Remote emergency command");
```

**在MQTT消息处理中集成：**
```c
case MQTT_EVENT_DATA:
    if (strstr(event->data, "\"EmergencyShutdown\"") != NULL) {
        emergency_power_off("Remote emergency shutdown");
    }
    break;
```

---

### 接口2：`remote_power_control()` - 远程电源控制

**功能：** 远程开启/关闭设备电源

**使用场景：**
- 远程控制设备开关
- 定时开关机
- 紧急断电后的恢复

**代码示例：**
```c
// 关闭电源
remote_power_control(0);

// 开启电源
remote_power_control(1);
```

**在MQTT消息处理中集成：**

已在 `main.c` 的 `mqtt_event_handler()` 中预留了示例代码（第787-803行）：

```c
case MQTT_EVENT_DATA:
    // 解析OneNet下发的控制指令
    if (strstr(event->topic, "property/set") != NULL) {
        // 解析JSON，提取PowerControl字段
        if (strstr(event->data, "\"PowerControl\"") != NULL) {
            // 提取value值（需要添加JSON解析代码）
            int value = parse_json_value(event->data, "PowerControl");
            
            // 调用远程控制接口
            remote_power_control(value);
        }
    }
    break;
```

**完整实现示例：**

```c
// 简单的JSON解析函数（示例）
int parse_power_control_value(const char *json) {
    // 查找 "PowerControl":{"value":X}
    const char *ptr = strstr(json, "\"PowerControl\"");
    if (ptr == NULL) return -1;
    
    ptr = strstr(ptr, "\"value\"");
    if (ptr == NULL) return -1;
    
    ptr = strchr(ptr, ':');
    if (ptr == NULL) return -1;
    
    int value;
    if (sscanf(ptr + 1, "%d", &value) == 1) {
        return value;
    }
    return -1;
}

// 在MQTT_EVENT_DATA中调用
case MQTT_EVENT_DATA:
    if (strstr(event->topic, "property/set") != NULL) {
        int power_value = parse_power_control_value(event->data);
        if (power_value == 0 || power_value == 1) {
            ESP_LOGI(MQTT_LOG, "Remote power control: %d", power_value);
            remote_power_control(power_value);
        }
    }
    break;
```

---

## 📡 OneNet云平台配置

### 添加可下发属性

在OneNet物模型中添加可下发属性：

```json
{
  "identifier": "PowerControl",
  "name": "电源控制",
  "accessMode": "rw",
  "dataType": {
    "type": "int32",
    "specs": {
      "min": 0,
      "max": 1,
      "step": 1
    }
  }
}
```

### 测试远程控制

在OneNet控制台，设备调试界面：

**关闭电源：**
```json
{
  "id": "123",
  "version": "1.0",
  "params": {
    "PowerControl": {
      "value": 0
    }
  }
}
```

**开启电源：**
```json
{
  "id": "123",
  "version": "1.0",
  "params": {
    "PowerControl": {
      "value": 1
    }
  }
}
```

---

## 🧪 测试建议

### 开发阶段测试（当前配置）

1. **测试告警检测：**
   - 用打火机靠近火焰传感器 → 观察日志输出告警
   - 加热温度传感器 → 观察温度告警
   - 检查MQTT消息是否正常发送

2. **验证不会断电：**
   - 触发紧急阈值（如火焰>70%）
   - 确认日志显示 "Development mode: Power-off disabled"
   - 确认设备继续运行，没有断电

3. **测试预留接口：**
   ```c
   // 在代码中临时添加测试调用
   void test_interfaces() {
       ESP_LOGI("TEST", "Testing emergency_power_off...");
       emergency_power_off("Test");
       vTaskDelay(pdMS_TO_TICKS(2000));
       
       ESP_LOGI("TEST", "Testing remote_power_control ON...");
       remote_power_control(1);
       vTaskDelay(pdMS_TO_TICKS(2000));
       
       ESP_LOGI("TEST", "Testing remote_power_control OFF...");
       remote_power_control(0);
   }
   ```

### 生产部署前测试

1. **切换到生产模式**
2. **测试自动断电：**
   - 触发紧急阈值
   - 确认设备立即断电
   - 确认告警消息已发送

3. **测试恢复机制：**
   - 断电后重启设备
   - 或通过远程控制恢复

---

## 📝 调整阈值

根据实际环境修改 `main.c` 中的阈值（第55-66行）：

```c
// 紧急异常阈值
#define THRESHOLD_FLAME_EMERGENCY 70.0f   // 根据实际火焰传感器调整
#define THRESHOLD_SMOKE_EMERGENCY 60.0f   // 根据实际烟雾传感器调整
#define THRESHOLD_TEMP_EMERGENCY 50.0f    // 根据实际使用环境调整
#define THRESHOLD_HEAT_EMERGENCY 80.0f    // 根据实际热敏传感器调整

// 一般告警阈值
#define THRESHOLD_FLAME_WARNING 40.0f
#define THRESHOLD_SMOKE_WARNING 30.0f
#define THRESHOLD_TEMP_HIGH 35.0f         // 夏季可能需要调高
#define THRESHOLD_TEMP_LOW 10.0f          // 冬季可能需要调低
#define THRESHOLD_HUMIDITY_HIGH 80.0f     // 潮湿环境需要调整
#define THRESHOLD_HUMIDITY_LOW 20.0f      // 干燥环境需要调整
```

**调整步骤：**
1. 在开发模式下运行，观察正常情况下的传感器数值
2. 根据正常值设置合理的告警阈值（留有余量）
3. 测试验证阈值是否合理
4. 多次调整直到满意

---

## 🔍 常见问题

### Q1: 如何知道当前是什么模式？
**A:** 启动时看日志：
- `Mode: DEVELOPMENT` = 开发模式
- `Mode: PRODUCTION` = 生产模式

### Q2: 开发模式会不会影响告警检测？
**A:** 不会！开发模式：
- ✅ 所有异常都会检测
- ✅ 所有告警都会上报
- ❌ 只是不会自动断电

### Q3: 如何手动触发紧急断电？
**A:** 调用 `emergency_power_off()` 函数，或通过OneNet下发紧急关机指令

### Q4: 生产模式断电后如何恢复？
**A:** 三种方式：
1. 手动重启ESP32设备
2. 通过OneNet下发恢复指令（调用 `remote_power_control(1)`）
3. 通过物理按键（需要硬件支持）

### Q5: 告警消息发送到哪里？
**A:** 
- 紧急告警：`$sys/3HSBa0ZB1R/ESP_01/thing/event/emergency`
- 一般告警：`$sys/3HSBa0ZB1R/ESP_01/thing/event/alert`
- 正常数据：`$sys/3HSBa0ZB1R/ESP_01/thing/property/post`

---

## 📞 技术支持

如遇到问题，请检查：
1. ✅ 串口日志输出
2. ✅ MQTT连接状态
3. ✅ 传感器接线
4. ✅ 阈值配置
5. ✅ OneNet平台配置

**调试技巧：**
```bash
# 查看完整日志
idf.py monitor

# 只看告警日志
idf.py monitor | grep ALERT

# 只看MQTT消息
idf.py monitor | grep MQTT
```

---

## ✅ 检查清单

### 开发阶段
- [ ] 确认 `ENABLE_AUTO_POWER_OFF = 0`
- [ ] 测试所有传感器告警
- [ ] 验证MQTT消息正常发送
- [ ] 确认不会意外断电
- [ ] 校准传感器阈值

### 生产部署前
- [ ] 修改 `ENABLE_AUTO_POWER_OFF = 1`
- [ ] 测试自动断电功能
- [ ] 测试恢复机制
- [ ] 验证OneNet配置
- [ ] 集成后端系统
- [ ] 配置告警通知

---

祝您使用愉快！🎉
