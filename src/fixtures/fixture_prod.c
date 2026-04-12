#include "fixtures/fixture_prod.h"

#include "esp_timer.h"

#include "app_config.h"
#include "app_log.h"
#include "board/board_pins.h"
#include "eeprom_24cs02.h"
#include "gpio_debug.h"

static int64_t g_last_log_time_us = 0;
static gpio_debug_t g_gpio_debug;
static eeprom_24cs02_t g_eeprom_24cs02;

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

static void fixture_prod_setup_impl(void)
{
    const gpio_debug_config_t gpio_debug_cfg = {
        .policy = {
            .allowed_mask = UINT64_MAX,
            .protected_mask =
                gpio_debug_pin_to_mask(BOARD_UART0_TX_PIN) |
                gpio_debug_pin_to_mask(BOARD_UART0_RX_PIN) |
                gpio_debug_pin_to_mask(BOARD_UART1_TX_PIN) |
                gpio_debug_pin_to_mask(BOARD_UART1_RX_PIN),
        },
    };
    const eeprom_24cs02_cfg_t eeprom_cfg = {
        .i2c_port = 0,
        .dev_addr = 0x56,
    };
    esp_err_t err;

    APP_LOGI(APP_FIXTURE_LOG_TAG, "fixture_prod_setup()");
    APP_LOGI(APP_FIXTURE_LOG_TAG, "Running on board: %s", BOARD_NAME);

    err = gpio_debug_init(&g_gpio_debug, &gpio_debug_cfg);
    if (err != ESP_OK)
    {
        APP_LOGE(APP_FIXTURE_LOG_TAG, "gpio_debug_init failed: %d", (int)err);
    }

    err = eeprom_24cs02_init(&g_eeprom_24cs02, &eeprom_cfg);
    if (err != ESP_OK)
    {
        APP_LOGE(APP_FIXTURE_LOG_TAG, "eeprom_24cs02_init failed: %d", (int)err);
    }
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
    const esp_err_t err = gpio_debug_register_commands(&g_gpio_debug);
    if (err != ESP_OK)
    {
        APP_LOGE(APP_FIXTURE_LOG_TAG, "gpio_debug_register_commands failed: %d", (int)err);
    }

    eeprom_24cs02_register_commands(&g_eeprom_24cs02);
}
