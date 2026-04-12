#include "fixtures/fixture_default.h"

#include "driver/i2c.h"
#include "esp_timer.h"

#include "app_config.h"
#include "app_log.h"
#include "board/board_pins.h"
#include "eeprom_24cs02.h"
#include "gpio_debug.h"

static int64_t g_last_log_time_us = 0;
static gpio_debug_t g_gpio_debug;
static eeprom_24cs02_t g_eeprom_24cs02;

#define FIXTURE_DEFAULT_I2C_PORT          I2C_NUM_0
#define FIXTURE_DEFAULT_I2C_FREQ_HZ       100000

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
    bool i2c_ready = false;
    const i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BOARD_I2C1_SDA_PIN,
        .scl_io_num = BOARD_I2C1_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = FIXTURE_DEFAULT_I2C_FREQ_HZ,
        .clk_flags = 0,
    };
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
        .i2c_port = FIXTURE_DEFAULT_I2C_PORT,
        .data_dev_addr = 0x56,
        .serial_dev_addr = 0x5E,
    };
    esp_err_t err;

    APP_LOGI(APP_FIXTURE_LOG_TAG, "fixture_default_setup()");
    APP_LOGI(APP_FIXTURE_LOG_TAG, "Running on board: %s", BOARD_NAME);

    err = i2c_param_config(FIXTURE_DEFAULT_I2C_PORT, &i2c_cfg);
    if (err != ESP_OK)
    {
        APP_LOGE(APP_FIXTURE_LOG_TAG, "i2c_param_config failed: %d", (int)err);
    }
    else
    {
        err = i2c_driver_install(FIXTURE_DEFAULT_I2C_PORT,
                                 i2c_cfg.mode,
                                 0,
                                 0,
                                 0);
        if (err != ESP_OK)
        {
            APP_LOGE(APP_FIXTURE_LOG_TAG, "i2c_driver_install failed: %d", (int)err);
        }
        else
        {
            i2c_ready = true;
        }
    }

    err = gpio_debug_init(&g_gpio_debug, &gpio_debug_cfg);
    if (err != ESP_OK)
    {
        APP_LOGE(APP_FIXTURE_LOG_TAG, "gpio_debug_init failed: %d", (int)err);
    }

    if (i2c_ready)
    {
        err = eeprom_24cs02_init(&g_eeprom_24cs02, &eeprom_cfg);
        if (err != ESP_OK)
        {
            APP_LOGE(APP_FIXTURE_LOG_TAG, "eeprom_24cs02_init failed: %d", (int)err);
        }
    }
    else
    {
        APP_LOGW(APP_FIXTURE_LOG_TAG, "EEPROM init skipped: I2C bus not ready");
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

    eeprom_24cs02_register_commands(&g_eeprom_24cs02);
}
