#include "key.h"

// 全局队列句柄（按键事件队列）
QueueHandle_t g_keyCallBackQueue = NULL;

typedef enum // 按键状态
{
    KEY_STATE_IDLE,        // 空闲状态:按键未按下
    KEY_STATE_DEBOUNCE,    // 消抖状态:按键已按下,等待消抖
    KEY_STATE_PRESSED,     // 稳定按下状态:按键已按下,等待松开或长按
    KEY_STATE_LONG_ACTIVE, // 长按激活状态:按键已按下,触发长按事件
} KeyState_t;

typedef struct
{
    uint8_t key_id;
    KeyState_t state;
    uint8_t isPressed;
    uint16_t stateDurationTicks;
    uint8_t isLongPressDetected;
} KeyContext_t;

KeyContext_t keyCtx[KEY_TOTAL_COUNT];

void Key_Init(void)
{
    gpio_config_t gpioConfig = {.mode = GPIO_MODE_INPUT,
                                .pull_up_en = GPIO_PULLDOWN_ENABLE,
                                .pull_down_en = GPIO_PULLUP_DISABLE,
                                .intr_type = GPIO_INTR_DISABLE,
                                .pin_bit_mask = (1ULL << KEY_GPIO_PORT_PIN_1) |
                                                (1ULL << KEY_GPIO_PORT_PIN_2) |
                                                (1ULL << KEY_GPIO_PORT_PIN_3) |
                                                (1ULL << KEY_GPIO_PORT_PIN_4)};
    ESP_ERROR_CHECK(gpio_config(&gpioConfig));
    for (unsigned char i = 0; i < KEY_TOTAL_COUNT; i++)
    {
        keyCtx[i].state = KEY_STATE_IDLE;
        keyCtx[i].key_id = i + 1;
        keyCtx[i].isPressed = 0;
        keyCtx[i].stateDurationTicks = 0;
        keyCtx[i].isLongPressDetected = 0;
    }
}

uint8_t getKeyPhysicalState(unsigned char keyId)
{
    gpio_num_t gpioNum = KEY_GPIO_PORT_PIN_1;
    switch (keyId + 1)
    {
    case KEY_ID_1:
        gpioNum = KEY_GPIO_PORT_PIN_1;
        break;
    case KEY_ID_2:
        gpioNum = KEY_GPIO_PORT_PIN_2;
        break;
    case KEY_ID_3:
        gpioNum = KEY_GPIO_PORT_PIN_3;
        break;
    default:
        gpioNum = KEY_GPIO_PORT_PIN_4;
        break;
    }
    return gpio_get_level(gpioNum);
}

void Key_UpdateState(void *arg)
{
    unsigned char i;
    KEYEvent_t keyEvent;
    for (i = 0; i < KEY_TOTAL_COUNT; i++)
    {
        keyEvent = KEY_EVT_NONE;
        keyCtx[i].isPressed = !getKeyPhysicalState(i);
        switch (keyCtx[i].state)
        {
        case KEY_STATE_IDLE:
            if (keyCtx[i].isPressed)
            {
                keyCtx[i].state = KEY_STATE_DEBOUNCE;
                keyCtx[i].isLongPressDetected = 0;
            }
            keyCtx[i].stateDurationTicks = 0;
            break;
        case KEY_STATE_DEBOUNCE:
            if (keyCtx[i].isPressed)
            {
                if (keyCtx[i].stateDurationTicks >= KEY_DEBOUNCE_TIME_TICKS)
                {
                    keyCtx[i].state = KEY_STATE_PRESSED;
                    keyCtx[i].stateDurationTicks = 0;
                    keyCtx[i].isLongPressDetected = 0;
                }
            }
            else
            {
                keyCtx[i].state = KEY_STATE_IDLE;
            }
            break;
        case KEY_STATE_PRESSED:
            if (!keyCtx[i].isPressed)
            {
                keyEvent = KEY_EVT_SHORT_PRESSED;
                keyCtx[i].state = KEY_STATE_IDLE;
                keyCtx[i].stateDurationTicks = 0;
            }
            else
            {
                if (keyCtx[i].stateDurationTicks >= KEY_LONG_PRESS_TIME_TICKS)
                {
                    if (!keyCtx[i].isLongPressDetected)
                    {
                        keyCtx[i].isLongPressDetected = 1;
                        // 长按激活事件
                        keyEvent = KEY_EVT_LONG_PRESSED; // 长按事件
                    }
                    keyCtx[i].state = KEY_STATE_LONG_ACTIVE;
                }
            }
            break;
        case KEY_STATE_LONG_ACTIVE:
            if (!keyCtx[i].isPressed)
            {
                keyCtx[i].state = KEY_STATE_IDLE;
                keyCtx[i].isLongPressDetected = 0;
            }
            else
            {
                keyCtx[i].stateDurationTicks = 0;
            }
            break;
        default:
            keyCtx[i].state = KEY_STATE_IDLE;
            keyCtx[i].stateDurationTicks = 0;
            break;
        }
        if (keyEvent != KEY_EVT_NONE)
        {
            Key_EventHandler((KEYId_t)keyCtx[i].key_id, keyEvent);
        }
        keyCtx[i].stateDurationTicks++;
    }
}

// weak 函数，可在其他文件中重定义该函数以实现自定义功能
__attribute__((weak)) void Key_EventHandler(KEYId_t key_id, KEYEvent_t key_event)
{
    if (g_keyCallBackQueue != NULL && key_id != KEY_ID_3 && key_id != KEY_ID_4)
    {
        KEYCallBackBase_t keyCallback = {
            .keyId = key_id,
            .keyEvent = key_event,
        };
        xQueueSend(g_keyCallBackQueue, &keyCallback, 0);
    }
}
