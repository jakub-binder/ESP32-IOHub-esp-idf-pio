#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "app_log.h"
#include "app_command_transport.h"
#include "app_command_endpoint.h"
#include "fixtures/fixture.h"

void app_main(void)
{
    APP_LOGI(APP_LOG_TAG, "Application boot");

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
