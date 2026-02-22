#ifndef __LED_H__
#define __LED_H__

#include "driver/gpio.h"

#ifndef LED_PIN
#define LED_PIN

#define LED1 GPIO_NUM_32
#define LED2 GPIO_NUM_33
#define LED3 GPIO_NUM_16
#define LED4 GPIO_NUM_16
#define LED5 GPIO_NUM_16
#define LED6 GPIO_NUM_16
#define LED7 GPIO_NUM_16
#define LED8 GPIO_NUM_16

#endif

#define LED_ON(gpio_pin) gpio_set_level(gpio_pin, 1)
#define LED_OFF(gpio_pin) gpio_set_level(gpio_pin, 0)

#endif
