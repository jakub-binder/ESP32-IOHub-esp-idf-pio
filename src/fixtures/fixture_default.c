#include "fixtures/fixture_default.h"

#include "esp_timer.h"

#include "app_config.h"
#include "app_log.h"
#include "board/board_pins.h"
#include "gpio_debug.h"

static int64_t g_last_log_time_us = 0;
static gpio_debug_t g_gpio_debug;

static uint64_t fixture_default_mask_for_pin(int pin)
{
    if (pin < 0 || pin >= 64)
    {
        return 0;
    }
    return (1ULL << (uint32_t)pin);
}

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

static void fixture_default_setup_impl(void)
{
    const gpio_debug_config_t gpio_debug_cfg = {
        .policy = {
            .allowed_mask = UINT64_MAX,
            .protected_mask =
                fixture_default_mask_for_pin(BOARD_UART0_TX_PIN) |
                fixture_default_mask_for_pin(BOARD_UART0_RX_PIN) |
                fixture_default_mask_for_pin(BOARD_UART1_TX_PIN) |
                fixture_default_mask_for_pin(BOARD_UART1_RX_PIN),
        },
    };
    esp_err_t err;

    APP_LOGI(APP_FIXTURE_LOG_TAG, "fixture_default_setup()");
    APP_LOGI(APP_FIXTURE_LOG_TAG, "Running on board: %s", BOARD_NAME);

    err = gpio_debug_init(&g_gpio_debug, &gpio_debug_cfg);
    if (err != ESP_OK)
    {
        APP_LOGE(APP_FIXTURE_LOG_TAG, "gpio_debug_init failed: %d", (int)err);
    }
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
    const esp_err_t err = gpio_debug_register_commands(&g_gpio_debug);
    if (err != ESP_OK)
    {
        APP_LOGE(APP_FIXTURE_LOG_TAG, "gpio_debug_register_commands failed: %d", (int)err);
    }
}
