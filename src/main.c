#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "app_log.h"
#include "app_serial_routing.h"
#include "app_command_transport.h"
#include "app_command_endpoint.h"
#include "board/board_pins.h"
#include "fixtures/fixture.h"

void app_main(void)
{
    const fixture_info_t *info = NULL;

    APP_LOGI(APP_LOG_TAG, "Application start");
    APP_LOGI(APP_LOG_TAG, "FW_BOARD_NAME = %s", FW_BOARD_NAME);
    APP_LOGI(APP_LOG_TAG, "BOARD_NAME    = %s", BOARD_NAME);

#if defined(APP_MODE_DEBUG)
    APP_LOGI(APP_LOG_TAG, "APP_MODE      = DEBUG");
#elif defined(APP_MODE_PROD)
    APP_LOGI(APP_LOG_TAG, "APP_MODE      = PROD");
#endif

    APP_LOGI(APP_LOG_TAG, "UART1 RX/TX   = %d / %d", BOARD_UART1_RX_PIN, BOARD_UART1_TX_PIN);
    APP_LOGI(APP_LOG_TAG, "I2C1 SDA/SCL  = %d / %d", BOARD_I2C1_SDA_PIN, BOARD_I2C1_SCL_PIN);
    APP_LOGI(APP_LOG_TAG, "SERIAL CMD    = %s", app_serial_endpoint_to_string(APP_SERIAL_COMMAND_ENDPOINT));

    info = fixture_get_info();
    if (info != NULL && info->name != NULL)
    {
        APP_LOGI(APP_LOG_TAG, "Fixture       = %s", info->name);
    }

    fixture_setup();

    /* Initialise the peripheral driver for the active command endpoint,
     * then wire its output callback into the command layer. */
    app_command_transport_init();
    app_command_endpoint_init(app_command_transport_write);

    while (1)
    {
        fixture_loop();
        vTaskDelay(pdMS_TO_TICKS(APP_MAIN_LOOP_DELAY_MS));
    }
}
