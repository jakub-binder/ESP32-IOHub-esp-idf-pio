#include "fixtures/fixture_default.h"

#include "esp_timer.h"

#include "app_config.h"
#include "app_log.h"
#include "board/board_pins.h"

static const fixture_info_t g_fixture_info =
{
    .name = "FIXTURE_DEFAULT"
};

static int64_t g_last_log_time_us = 0;

static void fixture_default_setup_impl(void);
static void fixture_default_loop_impl(void);
static void fixture_default_register_commands_impl(void);

const fixture_t fixture_default =
{
    .name = "FIXTURE_DEFAULT",
    .setup = fixture_default_setup_impl,
    .loop = fixture_default_loop_impl,
    .register_commands = fixture_default_register_commands_impl
};

const fixture_info_t *fixture_default_get_info(void)
{
    return &g_fixture_info;
}

static void fixture_default_setup_impl(void)
{
    APP_LOGI(APP_FIXTURE_LOG_TAG, "fixture_default_setup()");
    APP_LOGI(APP_FIXTURE_LOG_TAG, "Running on board: %s", BOARD_NAME);
}

static void fixture_default_loop_impl(void)
{
    const int64_t now_us = esp_timer_get_time();
    const int64_t period_us = 1000000; /* 1 s */

    if ((now_us - g_last_log_time_us) >= period_us)
    {
        g_last_log_time_us = now_us;
        //APP_LOGI(APP_FIXTURE_LOG_TAG, "[DEFAULT] heartbeat");
    }
}

static void fixture_default_register_commands_impl(void)
{
}
