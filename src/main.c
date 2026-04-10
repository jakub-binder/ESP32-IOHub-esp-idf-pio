#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "app_log.h"
#include "app_command_transport.h"
#include "fixtures/fixture.h"

void app_main(void)
{
    APP_LOGI(APP_LOG_TAG, "Application boot");

    fixture_setup();

    /* Initialise both command transport ports (debug: UART0, prod: UART1).
     * Each port manages its own endpoint, line buffer, and output callback. */
    app_command_transport_init();

    while (1)
    {
        fixture_loop();
        vTaskDelay(pdMS_TO_TICKS(APP_MAIN_LOOP_DELAY_MS));
    }
}
