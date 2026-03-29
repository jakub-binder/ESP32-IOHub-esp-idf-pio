#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "app_config.h"
#include "board/board_pins.h"
#include "fixtures/fixture.h"

void app_main(void)
{
    const fixture_info_t *info = NULL;

    ESP_LOGI(APP_LOG_TAG, "Application start");
    ESP_LOGI(APP_LOG_TAG, "FW_BOARD_NAME = %s", FW_BOARD_NAME);
    ESP_LOGI(APP_LOG_TAG, "BOARD_NAME    = %s", BOARD_NAME);

#if defined(APP_MODE_DEBUG)
    ESP_LOGI(APP_LOG_TAG, "APP_MODE      = DEBUG");
#elif defined(APP_MODE_PROD)
    ESP_LOGI(APP_LOG_TAG, "APP_MODE      = PROD");
#endif

    ESP_LOGI(APP_LOG_TAG, "UART1 RX/TX   = %d / %d", BOARD_UART1_RX_PIN, BOARD_UART1_TX_PIN);
    ESP_LOGI(APP_LOG_TAG, "I2C1 SDA/SCL  = %d / %d", BOARD_I2C1_SDA_PIN, BOARD_I2C1_SCL_PIN);

    info = fixture_get_info();
    if (info != NULL && info->name != NULL)
    {
        ESP_LOGI(APP_LOG_TAG, "Fixture       = %s", info->name);
    }

    fixture_setup();

    while (1)
    {
        fixture_loop();
        vTaskDelay(pdMS_TO_TICKS(APP_MAIN_LOOP_DELAY_MS));
    }
}