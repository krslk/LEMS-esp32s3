#ifndef __KEY_H__
#define __KEY_H__

#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define KEY_TOTAL_COUNT 2

#ifndef KEY_PIN_NUM
#define KEY_PIN_NUM

#define KEY_GPIO_PORT_PIN_1 GPIO_NUM_21
#define KEY_GPIO_PORT_PIN_2 GPIO_NUM_22
#define KEY_GPIO_PORT_PIN_3 GPIO_NUM_23
#define KEY_GPIO_PORT_PIN_4 GPIO_NUM_25

#define KEY_DEBOUNCE_TIME_TICKS (2U)    // 消抖时间计数
#define KEY_LONG_PRESS_TIME_TICKS (50U) // 长按确认时间计数

#endif

typedef enum // 按键ID
{
    KEY_ID_1 = 1, // KEY1
    KEY_ID_2,     // KEY2
    KEY_ID_3,     // KEY2
    KEY_ID_4,     // KEY2
} KEYId_t;

typedef enum // 按键事件类型
{
    KEY_EVT_NONE,          // 无事件
    KEY_EVT_SHORT_PRESSED, // 点击
    KEY_EVT_LONG_PRESSED,  // 长按
} KEYEvent_t;

typedef struct // 按键回调数据类型
{
    KEYId_t keyId;
    KEYEvent_t keyEvent;
} KEYCallBackBase_t;

/**
 *
 * @brief 初始化所有按键硬件和状态机
 * @note 应在系统启动时调用一次
 *
 */
void Key_Init(void);

/**
 *
 * @brief 按键状态机周期性处理更新（10ms）
 * @note 需在定时器中断或主循环中定期调用
 *
 */
void Key_UpdateState(void *arg);

/**
 * @brief 按键事件回调函数
 * @param id 触发事件的按键ID
 * @param event 触发的事件类型
 * @note 由按键驱动模块在检测到有效事件时调用,可在其他文件中重定义该函数以实现自定义功能
 */
void Key_EventHandler(KEYId_t key_id, KEYEvent_t key_event);

#endif // KEY_H