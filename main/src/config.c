#include "config.h"
#include <string.h>

#define KEY_POLL_INTERVAL (10U)

WorkstationStatus g_CurrentWorkStaStatus;

void Init_Config(void)
{
    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // 关闭断电检测
    ESP_ERROR_CHECK(gpio_config(&(gpio_config_t){.mode = GPIO_MODE_OUTPUT,
                                                 .pull_up_en = GPIO_PULLUP_DISABLE,
                                                 .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                                 .intr_type = GPIO_INTR_DISABLE,
                                                 .pin_bit_mask = (1ULL << GPIO_NUM_17) |
                                                                 (1ULL << GPIO_NUM_27)}));
    ESP_ERROR_CHECK(gpio_config(&(gpio_config_t){.mode = GPIO_MODE_OUTPUT,
                                                 .pull_up_en = GPIO_PULLUP_DISABLE,
                                                 .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                                 .intr_type = GPIO_INTR_DISABLE,
                                                 .pin_bit_mask = (1ULL << GPIO_NUM_16)}));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_17 | GPIO_NUM_27, 1));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_16, 0));
    
    // 配置继电器控制引脚为输出模式（GPIO0, 2, 4, 13）
    ESP_ERROR_CHECK(gpio_config(&(gpio_config_t){.mode = GPIO_MODE_OUTPUT,
                                                 .pull_up_en = GPIO_PULLUP_DISABLE,
                                                 .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                                 .intr_type = GPIO_INTR_DISABLE,
                                                 .pin_bit_mask = (1ULL << GPIO_NUM_0) |
                                                                 (1ULL << GPIO_NUM_2) |
                                                                 (1ULL << GPIO_NUM_4) |
                                                                 (1ULL << GPIO_NUM_13)}));
    // 初始化时关闭所有继电器
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_0, 0));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_2, 0));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_4, 0));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_13, 0));

    g_CurrentWorkStaStatus.workstationCode[0] = '\0'; // 工位编号默认空字符串（自动补 \0）
    strncpy(g_CurrentWorkStaStatus.curUsername, "null", sizeof(g_CurrentWorkStaStatus.curUsername) - 1);
    g_CurrentWorkStaStatus.curUsername[sizeof(g_CurrentWorkStaStatus.curUsername) - 1] = '\0'; // 初始无卡片时设置为"null"
    g_CurrentWorkStaStatus.powerStatus = 0;           // 默认：电源关闭
    g_CurrentWorkStaStatus.lightStatus = 0;           // 默认：灯光关闭
    g_CurrentWorkStaStatus.relayNum1Status = 0;       // 默认：1号继电器关闭
    g_CurrentWorkStaStatus.relayNum2Status = 0;       // 默认：2号继电器关闭
    g_CurrentWorkStaStatus.lightIntensity = -1;       // 默认：未采集光线
    g_CurrentWorkStaStatus.smokeScope = -1;           // 默认：未采集烟雾
    g_CurrentWorkStaStatus.flameScope = -1;           // 默认：未检测火焰
    g_CurrentWorkStaStatus.humidity = -1;             // 默认：未采集湿度
    g_CurrentWorkStaStatus.temperature = -1;          // 默认：未采集温度
}
