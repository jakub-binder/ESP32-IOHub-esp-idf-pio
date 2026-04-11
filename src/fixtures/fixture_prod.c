#include "fixtures/fixture_prod.h"

#include "esp_timer.h"

#include "app_config.h"
#include "app_log.h"
#include "board/board_pins.h"

static const fixture_info_t g_fixture_info =
{
    .name = "FIXTURE_PROD"
};

static int64_t g_last_log_time_us = 0;

static void fixture_prod_setup_impl(void);
static void fixture_prod_loop_impl(void);
static void fixture_prod_register_commands_impl(void);

const fixture_t fixture_prod =
{
    .name = "FIXTURE_PROD",
    .setup = fixture_prod_setup_impl,
    .loop = fixture_prod_loop_impl,
    .register_commands = fixture_prod_register_commands_impl
};

const fixture_info_t *fixture_prod_get_info(void)
{
    return &g_fixture_info;
}

static void fixture_prod_setup_impl(void)
{
    APP_LOGI(APP_FIXTURE_LOG_TAG, "fixture_prod_setup()");
    APP_LOGI(APP_FIXTURE_LOG_TAG, "Running on board: %s", BOARD_NAME);
}

static void fixture_prod_loop_impl(void)
{
    const int64_t now_us = esp_timer_get_time();
    const int64_t period_us = 2000000; /* 2 s */

    if ((now_us - g_last_log_time_us) >= period_us)
    {
        g_last_log_time_us = now_us;
        //APP_LOGI(APP_FIXTURE_LOG_TAG, "[PROD] heartbeat");
    }
}

static void fixture_prod_register_commands_impl(void)
{
    /* No fixture-specific commands yet. */
}
