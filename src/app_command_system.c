#include "app_command_system.h"

#include "esp_system.h"

#include "app_config.h"
#include "app_log.h"
#include "app_system_info.h"

#define TAG "CMD_SYS"

void app_command_system_firmware(app_command_output_fn output)
{
    if (output != NULL)
    {
        output(APP_FIRMWARE_ID "\r\n");
    }
}

void app_command_system_restart(app_command_output_fn output)
{
    if (output != NULL)
    {
        output("restarting...\r\n");
    }
    APP_LOGI(TAG, "restart command – calling esp_restart()");
    esp_restart();
}

void app_command_system_init(app_command_output_fn output)
{
    /* Conservative implementation: acknowledge via log and output callback.
     * Full fixture re-initialisation can be wired here once a clear
     * fixture_reinit() API exists. */
    APP_LOGI(TAG, "init command received");
    if (output != NULL)
    {
        output("OK\r\n");
    }
}

void app_command_system_info(app_command_output_fn output)
{
    app_system_info_print(output);
}
