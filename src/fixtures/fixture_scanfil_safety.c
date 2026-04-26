#include "fixtures/fixture_scanfil_safety.h"

#include "esp_timer.h"

#include "app_config.h"
#include "app_log.h"
#include "board/board_pins.h"
#include "adc_tla2024.h"
#include "eeprom_24c64.h"
#include "i2c_bus.h"

static int64_t g_last_log_time_us = 0;
static i2c_bus_t g_i2c_bus;
static eeprom_24c64_t g_eeprom_24c64;
static adc_tla2024_t g_adc_tla2024;

#define FIXTURE_SCANFIL_SAFETY_I2C_PORT           I2C_NUM_0
#define FIXTURE_SCANFIL_SAFETY_I2C_FREQ_HZ        100000U
#define FIXTURE_SCANFIL_SAFETY_EEPROM_DEV_ADDR    0x53U
#define FIXTURE_SCANFIL_SAFETY_ADC_DEV_ADDR       0x48U

static void fixture_scanfil_safety_setup_impl(void);
static void fixture_scanfil_safety_loop_impl(void);
static void fixture_scanfil_safety_register_commands_impl(void);

const fixture_t fixture_scanfil_safety =
{
    .name = "FIXTURE_SCANFIL_SAFETY",
    .setup = fixture_scanfil_safety_setup_impl,
    .loop = fixture_scanfil_safety_loop_impl,
    .register_commands = fixture_scanfil_safety_register_commands_impl
};

static void fixture_scanfil_safety_setup_impl(void)
{
    bool i2c_ready = false;
    const i2c_bus_config_t i2c_bus_cfg = {
        .port = FIXTURE_SCANFIL_SAFETY_I2C_PORT,
        .sda_pin = BOARD_I2C1_SDA_PIN,
        .scl_pin = BOARD_I2C1_SCL_PIN,
        .frequency_hz = FIXTURE_SCANFIL_SAFETY_I2C_FREQ_HZ,
        .pullup_enable = true,
    };
    esp_err_t err;

    APP_LOGI(APP_FIXTURE_LOG_TAG, "fixture_scanfil_safety_setup()");
    APP_LOGI(APP_FIXTURE_LOG_TAG, "Running on board: %s", BOARD_NAME);

    err = i2c_bus_init(&g_i2c_bus, &i2c_bus_cfg);
    if (err != ESP_OK)
    {
        APP_LOGE(APP_FIXTURE_LOG_TAG, "i2c_bus_init failed: %d", (int)err);
    }
    i2c_ready = i2c_bus_is_initialized(&g_i2c_bus);

    if (i2c_ready)
    {
        const eeprom_24c64_cfg_t eeprom_cfg = {
            .i2c_port = i2c_bus_port(&g_i2c_bus),
            .dev_addr = FIXTURE_SCANFIL_SAFETY_EEPROM_DEV_ADDR,
            .ack_poll_timeout_ms = 20U,
        };
        const adc_tla2024_cfg_t adc_cfg = {
            .i2c_port = i2c_bus_port(&g_i2c_bus),
            .dev_addr = FIXTURE_SCANFIL_SAFETY_ADC_DEV_ADDR,
            .i2c_timeout_ms = 100U,
        };

        err = eeprom_24c64_init(&g_eeprom_24c64, &eeprom_cfg);
        if (err != ESP_OK)
        {
            APP_LOGE(APP_FIXTURE_LOG_TAG, "eeprom_24c64_init failed: %d", (int)err);
        }

        err = adc_tla2024_init(&g_adc_tla2024, &adc_cfg);
        if (err != ESP_OK)
        {
            APP_LOGE(APP_FIXTURE_LOG_TAG, "adc_tla2024_init failed: %d", (int)err);
        }
    }
    else
    {
        APP_LOGW(APP_FIXTURE_LOG_TAG, "EEPROM64 init skipped: I2C bus not ready");
        APP_LOGW(APP_FIXTURE_LOG_TAG, "TLA2024 init skipped: I2C bus not ready");
    }
}

static void fixture_scanfil_safety_loop_impl(void)
{
    const int64_t now_us = esp_timer_get_time();
    const int64_t period_us = 1000000; /* 1 s */

    if ((now_us - g_last_log_time_us) >= period_us)
    {
        g_last_log_time_us = now_us;
        //APP_LOGI(APP_FIXTURE_LOG_TAG, "[SCANFIL_SAFETY] heartbeat");
    }
}

static void fixture_scanfil_safety_register_commands_impl(void)
{
    eeprom_24c64_register_commands(&g_eeprom_24c64);
    adc_tla2024_register_commands(&g_adc_tla2024);
}
