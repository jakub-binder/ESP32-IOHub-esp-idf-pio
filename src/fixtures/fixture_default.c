#include "fixtures/fixture_default.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "esp_timer.h"

#include "app_commands.h"
#include "app_config.h"
#include "app_log.h"
#include "board/board_pins.h"
#include "eeprom_24cs02.h"
#include "gpio_debug.h"
#include "i2c_bus.h"
#include "spi_bus.h"
#include "temp_lm75bdp.h"

static int64_t g_last_log_time_us = 0;
static gpio_debug_t g_gpio_debug;
static i2c_bus_t g_i2c_bus;
static eeprom_24cs02_t g_eeprom_24cs02;
static temp_lm75bdp_t g_temp_lm75bdp;

#define FIXTURE_DEFAULT_I2C_PORT          I2C_NUM_0
#define FIXTURE_DEFAULT_I2C_FREQ_HZ       100000
#define FIXTURE_DEFAULT_COMMAND_OUTPUT_BUFFER_SIZE 256U
#define FIXTURE_DEFAULT_SPI_CLOCK_HZ      1000000U
#define FIXTURE_DEFAULT_SPI_MODE          0U
#define FIXTURE_DEFAULT_SPI_TEST_DATA_SIZE 4U

#if defined(BOARD_HAS_VSPI) && BOARD_HAS_VSPI
#define FIXTURE_DEFAULT_SPI_HOST          SPI3_HOST
#define FIXTURE_DEFAULT_SPI_MISO_PIN      BOARD_VSPI_MISO_PIN
#define FIXTURE_DEFAULT_SPI_MOSI_PIN      BOARD_VSPI_MOSI_PIN
#define FIXTURE_DEFAULT_SPI_SCK_PIN       BOARD_VSPI_SCK_PIN
#define FIXTURE_DEFAULT_SPI_CS_PIN        BOARD_VSPI_SS_PIN
#elif defined(BOARD_HAS_HSPI) && BOARD_HAS_HSPI
#define FIXTURE_DEFAULT_SPI_HOST          SPI2_HOST
#define FIXTURE_DEFAULT_SPI_MISO_PIN      BOARD_HSPI_MISO_PIN
#define FIXTURE_DEFAULT_SPI_MOSI_PIN      BOARD_HSPI_MOSI_PIN
#define FIXTURE_DEFAULT_SPI_SCK_PIN       BOARD_HSPI_SCK_PIN
#define FIXTURE_DEFAULT_SPI_CS_PIN        BOARD_HSPI_SS_PIN
#endif

static void fixture_default_setup_impl(void);
static void fixture_default_loop_impl(void);
static void fixture_default_register_commands_impl(void);
static bool fixture_default_spi_loopback_command_handler(const app_command_ctx_t *ctx,
                                                         const char *cmd,
                                                         char *args,
                                                         void *user_ctx);

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
    const i2c_bus_config_t i2c_bus_cfg = {
        .port = FIXTURE_DEFAULT_I2C_PORT,
        .sda_pin = BOARD_I2C1_SDA_PIN,
        .scl_pin = BOARD_I2C1_SCL_PIN,
        .frequency_hz = FIXTURE_DEFAULT_I2C_FREQ_HZ,
        .pullup_enable = true,
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
    esp_err_t err;

    APP_LOGI(APP_FIXTURE_LOG_TAG, "fixture_default_setup()");
    APP_LOGI(APP_FIXTURE_LOG_TAG, "Running on board: %s", BOARD_NAME);

    err = i2c_bus_init(&g_i2c_bus, &i2c_bus_cfg);
    if (err != ESP_OK)
    {
        APP_LOGE(APP_FIXTURE_LOG_TAG, "i2c_bus_init failed: %d", (int)err);
    }
    i2c_ready = i2c_bus_is_initialized(&g_i2c_bus);

    err = gpio_debug_init(&g_gpio_debug, &gpio_debug_cfg);
    if (err != ESP_OK)
    {
        APP_LOGE(APP_FIXTURE_LOG_TAG, "gpio_debug_init failed: %d", (int)err);
    }

    if (i2c_ready)
    {
        const eeprom_24cs02_cfg_t eeprom_cfg = {
            .i2c_port = i2c_bus_port(&g_i2c_bus),
            .data_dev_addr = 0x56,
            .serial_dev_addr = 0x5E,
        };
        const temp_lm75bdp_cfg_t temp_cfg = {
            .i2c_port = i2c_bus_port(&g_i2c_bus),
            .dev_addr = 0x48,
        };
        err = eeprom_24cs02_init(&g_eeprom_24cs02, &eeprom_cfg);
        if (err != ESP_OK)
        {
            APP_LOGE(APP_FIXTURE_LOG_TAG, "eeprom_24cs02_init failed: %d", (int)err);
        }

        err = temp_lm75bdp_init(&g_temp_lm75bdp, &temp_cfg);
        if (err != ESP_OK)
        {
            APP_LOGE(APP_FIXTURE_LOG_TAG, "temp_lm75bdp_init failed: %d", (int)err);
        }
    }
    else
    {
        APP_LOGW(APP_FIXTURE_LOG_TAG, "EEPROM init skipped: I2C bus not ready");
        APP_LOGW(APP_FIXTURE_LOG_TAG, "LM75BDP init skipped: I2C bus not ready");
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

static void fixture_default_command_printf(const app_command_ctx_t *ctx,
                                           const char *fmt, ...)
{
    char buf[FIXTURE_DEFAULT_COMMAND_OUTPUT_BUFFER_SIZE];
    va_list args;

    if (ctx == NULL || ctx->output == NULL)
    {
        return;
    }

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ctx->output(buf);
}

static void fixture_default_print_bytes(const app_command_ctx_t *ctx,
                                        const char *label,
                                        const uint8_t *data,
                                        size_t length)
{
    size_t i;

    if (ctx == NULL || ctx->output == NULL || label == NULL || data == NULL)
    {
        return;
    }

    fixture_default_command_printf(ctx, "%s", label);
    for (i = 0; i < length; i++)
    {
        fixture_default_command_printf(ctx, "%s%02X", (i == 0) ? "" : " ", data[i]);
    }
    fixture_default_command_printf(ctx, "\r\n");
}

static bool fixture_default_spi_loopback_command_handler(const app_command_ctx_t *ctx,
                                                         const char *cmd,
                                                         char *args,
                                                         void *user_ctx)
{
    /* Alternating bit patterns + mixed values for basic loopback coverage. */
    static const uint8_t tx_data[FIXTURE_DEFAULT_SPI_TEST_DATA_SIZE] = {0x55, 0xAA, 0x12, 0x34};
    uint8_t rx_data[FIXTURE_DEFAULT_SPI_TEST_DATA_SIZE] = {0};
    spi_bus_t bus = {0};
    spi_device_t dev = {0};
    bool match = false;
    esp_err_t err;

    (void)args;
    (void)user_ctx;

    if (ctx == NULL || cmd == NULL)
    {
        return false;
    }

    if (strcmp(cmd, "spi-loopback-test") != 0)
    {
        return false;
    }

    if (!ctx->allow_debug_commands)
    {
        fixture_default_command_printf(ctx, "ERR not allowed\r\n");
        return true;
    }

#if !defined(FIXTURE_DEFAULT_SPI_HOST)
    fixture_default_command_printf(ctx, "FAIL\r\n");
    fixture_default_command_printf(ctx, "ERR SPI host not available\r\n");
    return true;
#else
    const spi_bus_cfg_t bus_cfg = {
        .host_id = FIXTURE_DEFAULT_SPI_HOST,
        .sclk_pin = FIXTURE_DEFAULT_SPI_SCK_PIN,
        .mosi_pin = FIXTURE_DEFAULT_SPI_MOSI_PIN,
        .miso_pin = FIXTURE_DEFAULT_SPI_MISO_PIN,
    };
    const spi_device_cfg_t dev_cfg = {
        .bus = &bus,
        .cs_pin = FIXTURE_DEFAULT_SPI_CS_PIN,
        .mode = FIXTURE_DEFAULT_SPI_MODE,
        .clock_hz = FIXTURE_DEFAULT_SPI_CLOCK_HZ,
    };

    err = spi_bus_init(&bus, &bus_cfg);
    if (err != ESP_OK)
    {
        fixture_default_command_printf(ctx, "FAIL\r\n");
        fixture_default_command_printf(ctx, "ERR spi_bus_init=%d\r\n", (int)err);
        goto cleanup;
    }

    err = spi_device_init(&dev, &dev_cfg);
    if (err != ESP_OK)
    {
        fixture_default_command_printf(ctx, "FAIL\r\n");
        fixture_default_command_printf(ctx, "ERR spi_device_init=%d\r\n", (int)err);
        goto cleanup;
    }

    err = spi_device_transfer(&dev, tx_data, rx_data, sizeof(tx_data));
    if (err != ESP_OK)
    {
        fixture_default_command_printf(ctx, "FAIL\r\n");
        fixture_default_command_printf(ctx, "ERR spi_device_transfer=%d\r\n", (int)err);
        goto cleanup;
    }

    match = (memcmp(tx_data, rx_data, sizeof(tx_data)) == 0);
    if (match)
    {
        fixture_default_command_printf(ctx, "PASS\r\n");
    }
    else
    {
        fixture_default_command_printf(ctx, "FAIL\r\n");
        fixture_default_print_bytes(ctx, "TX: ", tx_data, sizeof(tx_data));
        fixture_default_print_bytes(ctx, "RX: ", rx_data, sizeof(rx_data));
    }

cleanup:
    if (spi_device_is_initialized(&dev))
    {
        err = spi_device_deinit(&dev);
        if (err != ESP_OK)
        {
            fixture_default_command_printf(ctx, "ERR spi_device_deinit=%d\r\n", (int)err);
        }
    }
    if (spi_bus_is_initialized(&bus))
    {
        err = spi_bus_deinit(&bus);
        if (err != ESP_OK)
        {
            fixture_default_command_printf(ctx, "ERR spi_bus_deinit=%d\r\n", (int)err);
        }
    }
    return true;
#endif
}

static void fixture_default_register_commands_impl(void)
{
    const esp_err_t err = gpio_debug_register_commands(&g_gpio_debug);
    if (err != ESP_OK)
    {
        APP_LOGE(APP_FIXTURE_LOG_TAG, "gpio_debug_register_commands failed: %d", (int)err);
    }

    eeprom_24cs02_register_commands(&g_eeprom_24cs02);
    temp_lm75bdp_register_commands(&g_temp_lm75bdp);

    if (!app_commands_register_custom_handler(fixture_default_spi_loopback_command_handler, NULL))
    {
        APP_LOGE(APP_FIXTURE_LOG_TAG, "spi-loopback-test register failed");
    }
}
