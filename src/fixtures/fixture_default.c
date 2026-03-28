#include "fixtures/fixture_default.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "app_config.h"
#include "board_pins.h"

static const fixture_info_t g_fixture_info =
{
    .name = "FIXTURE_DEFAULT"
};

static int64_t g_last_log_time_us = 0;

const fixture_info_t *fixture_default_get_info(void)
{
    return &g_fixture_info;
}

void fixture_default_setup(void)
{
    ESP_LOGI(APP_FIXTURE_LOG_TAG, "fixture_default_setup()");
    ESP_LOGI(APP_FIXTURE_LOG_TAG, "Running on board: %s", BOARD_NAME);
}

void fixture_default_loop(void)
{
    const int64_t now_us = esp_timer_get_time();
    const int64_t period_us = 1000000; /* 1 s */

    if ((now_us - g_last_log_time_us) >= period_us)
    {
        g_last_log_time_us = now_us;
        ESP_LOGI(APP_FIXTURE_LOG_TAG, "[DEFAULT] heartbeat");
    }
}