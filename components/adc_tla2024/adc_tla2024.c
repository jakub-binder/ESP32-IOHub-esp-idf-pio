#include "adc_tla2024.h"

#include "app_commands.h"
#include "driver/i2c.h"
#include "esp_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ADC_TLA2024_DEFAULT_TIMEOUT_MS     100U
#define ADC_TLA2024_CMD_BUF_SIZE           96U
#define ADC_TLA2024_CONVERSION_WAIT_MS     2U

#define ADC_TLA2024_OS_BIT                 (1U << 15)
#define ADC_TLA2024_MUX_SHIFT              12U
#define ADC_TLA2024_MUX_CH0_SINGLE         0x4U
#define ADC_TLA2024_MUX_CH1_SINGLE         0x5U
#define ADC_TLA2024_PGA_SHIFT              9U
#define ADC_TLA2024_PGA_FS_4V096           0x1U
#define ADC_TLA2024_MODE_SINGLE_SHOT       (1U << 8)
#define ADC_TLA2024_DR_SHIFT               5U
#define ADC_TLA2024_DR_1600_SPS            0x4U
#define ADC_TLA2024_RESERVED_MASK          0x0003U

static const char *const ADC_TLA2024_CMD_READ = "TLA2024:READ";
static const char *const ADC_TLA2024_CMD_READALL = "TLA2024:READALL";
static const char *const ADC_TLA2024_CMD_REG = "TLA2024:REG";
static const char *const ADC_TLA2024_TAG = "adc_tla2024";

static bool adc_tla2024_is_ready(const adc_tla2024_t *ctx)
{
    return (ctx != NULL) && ctx->initialized;
}

static bool adc_tla2024_dev_addr_is_valid(uint8_t dev_addr)
{
    return (dev_addr != 0U) && ((dev_addr & 0x80U) == 0U);
}

static bool adc_tla2024_channel_is_valid(uint8_t channel)
{
    return (channel == 0U) || (channel == 1U);
}

static TickType_t adc_tla2024_timeout_ticks(const adc_tla2024_t *ctx)
{
    const uint32_t timeout_ms = (ctx->i2c_timeout_ms == 0U)
                                    ? ADC_TLA2024_DEFAULT_TIMEOUT_MS
                                    : ctx->i2c_timeout_ms;
    return pdMS_TO_TICKS(timeout_ms);
}

static uint16_t adc_tla2024_build_config_single_shot(uint8_t channel)
{
    uint16_t mux_bits = ADC_TLA2024_MUX_CH0_SINGLE;

    if (channel == 1U)
    {
        mux_bits = ADC_TLA2024_MUX_CH1_SINGLE;
    }

    return ADC_TLA2024_OS_BIT |
           (uint16_t)(mux_bits << ADC_TLA2024_MUX_SHIFT) |
           (uint16_t)(ADC_TLA2024_PGA_FS_4V096 << ADC_TLA2024_PGA_SHIFT) |
           ADC_TLA2024_MODE_SINGLE_SHOT |
           (uint16_t)(ADC_TLA2024_DR_1600_SPS << ADC_TLA2024_DR_SHIFT) |
           ADC_TLA2024_RESERVED_MASK;
}

static void adc_tla2024_printf(app_command_output_fn output, const char *fmt, ...)
{
    char buf[ADC_TLA2024_CMD_BUF_SIZE];
    va_list args;

    if (output == NULL)
    {
        return;
    }

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    output(buf);
}

static bool adc_tla2024_parse_channel(const char *text, uint8_t *out_channel)
{
    if (text == NULL || out_channel == NULL)
    {
        return false;
    }

    if (strcmp(text, "0") == 0)
    {
        *out_channel = 0U;
        return true;
    }

    if (strcmp(text, "1") == 0)
    {
        *out_channel = 1U;
        return true;
    }

    return false;
}

static bool adc_tla2024_parse_reg_hex(const char *text, uint8_t *out_reg)
{
    if (text == NULL || out_reg == NULL)
    {
        return false;
    }

    if (strcmp(text, "00") == 0)
    {
        *out_reg = ADC_TLA2024_REG_CONVERSION;
        return true;
    }

    if (strcmp(text, "01") == 0)
    {
        *out_reg = ADC_TLA2024_REG_CONFIG;
        return true;
    }

    return false;
}

esp_err_t adc_tla2024_read_reg16(adc_tla2024_t *ctx,
                                 uint8_t reg_addr,
                                 uint16_t *out_value)
{
    uint8_t reg = reg_addr;
    uint8_t rx[2];
    esp_err_t err;

    if (out_value == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!adc_tla2024_is_ready(ctx))
    {
        return ESP_ERR_INVALID_STATE;
    }

    err = i2c_master_write_read_device((i2c_port_t)ctx->i2c_port,
                                       ctx->dev_addr,
                                       &reg,
                                       sizeof(reg),
                                       rx,
                                       sizeof(rx),
                                       adc_tla2024_timeout_ticks(ctx));
    if (err != ESP_OK)
    {
        ESP_LOGE(ADC_TLA2024_TAG, "read reg 0x%02X failed: %d", reg_addr, (int)err);
        return err;
    }

    *out_value = ((uint16_t)rx[0] << 8U) | (uint16_t)rx[1];
    return ESP_OK;
}

esp_err_t adc_tla2024_write_reg16(adc_tla2024_t *ctx,
                                  uint8_t reg_addr,
                                  uint16_t value)
{
    uint8_t tx[3];
    esp_err_t err;

    if (!adc_tla2024_is_ready(ctx))
    {
        return ESP_ERR_INVALID_STATE;
    }

    tx[0] = reg_addr;
    tx[1] = (uint8_t)((value >> 8U) & 0xFFU);
    tx[2] = (uint8_t)(value & 0xFFU);

    err = i2c_master_write_to_device((i2c_port_t)ctx->i2c_port,
                                     ctx->dev_addr,
                                     tx,
                                     sizeof(tx),
                                     adc_tla2024_timeout_ticks(ctx));
    if (err != ESP_OK)
    {
        ESP_LOGE(ADC_TLA2024_TAG, "write reg 0x%02X failed: %d", reg_addr, (int)err);
    }

    return err;
}

esp_err_t adc_tla2024_init(adc_tla2024_t *ctx,
                           const adc_tla2024_cfg_t *cfg)
{
    uint16_t probe_config = 0U;
    esp_err_t err;

    if (ctx == NULL || cfg == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (cfg->i2c_port < 0 || cfg->i2c_port >= I2C_NUM_MAX)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!adc_tla2024_dev_addr_is_valid(cfg->dev_addr))
    {
        return ESP_ERR_INVALID_ARG;
    }

    ctx->i2c_port = cfg->i2c_port;
    ctx->dev_addr = cfg->dev_addr;
    ctx->i2c_timeout_ms = (cfg->i2c_timeout_ms == 0U)
                              ? ADC_TLA2024_DEFAULT_TIMEOUT_MS
                              : cfg->i2c_timeout_ms;
    ctx->initialized = true;

    err = adc_tla2024_read_reg16(ctx, ADC_TLA2024_REG_CONFIG, &probe_config);
    if (err != ESP_OK)
    {
        ctx->initialized = false;
        return err;
    }

    ESP_LOGD(ADC_TLA2024_TAG, "init probe ok (config=0x%04X)", probe_config);
    return ESP_OK;
}

esp_err_t adc_tla2024_read_channel_raw12(adc_tla2024_t *ctx,
                                         uint8_t channel,
                                         uint16_t *out_raw12)
{
    const uint16_t config = adc_tla2024_build_config_single_shot(channel);
    TickType_t wait_ticks = pdMS_TO_TICKS(ADC_TLA2024_CONVERSION_WAIT_MS);
    uint16_t raw16 = 0U;
    esp_err_t err;

    if (out_raw12 == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!adc_tla2024_is_ready(ctx))
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!adc_tla2024_channel_is_valid(channel))
    {
        return ESP_ERR_INVALID_ARG;
    }

    err = adc_tla2024_write_reg16(ctx, ADC_TLA2024_REG_CONFIG, config);
    if (err != ESP_OK)
    {
        return err;
    }

    if (wait_ticks == 0)
    {
        wait_ticks = 1;
    }
    vTaskDelay(wait_ticks);

    err = adc_tla2024_read_reg16(ctx, ADC_TLA2024_REG_CONVERSION, &raw16);
    if (err != ESP_OK)
    {
        return err;
    }

    *out_raw12 = (uint16_t)(raw16 >> 4U);
    return ESP_OK;
}

static bool adc_tla2024_is_supported_command(const char *cmd)
{
    return (strcmp(cmd, ADC_TLA2024_CMD_READ) == 0) ||
           (strcmp(cmd, ADC_TLA2024_CMD_READALL) == 0) ||
           (strcmp(cmd, ADC_TLA2024_CMD_REG) == 0);
}

static bool adc_tla2024_handle_read(const app_command_ctx_t *cmd_ctx,
                                    adc_tla2024_t *ctx,
                                    char *args)
{
    char *saveptr = NULL;
    char *channel_str = strtok_r(args, " \t", &saveptr);
    char *extra = strtok_r(NULL, " \t", &saveptr);
    uint8_t channel = 0U;
    uint16_t raw12 = 0U;
    esp_err_t err;

    if (extra != NULL || !adc_tla2024_parse_channel(channel_str, &channel))
    {
        adc_tla2024_printf(cmd_ctx->output, "ERR usage: TLA2024:READ <0|1>\r\n");
        return true;
    }

    err = adc_tla2024_read_channel_raw12(ctx, channel, &raw12);
    if (err != ESP_OK)
    {
        adc_tla2024_printf(cmd_ctx->output, "ERR %d\r\n", (int)err);
        return true;
    }

    app_commands_respond_ok_with_count(cmd_ctx->output, 1U);
    adc_tla2024_printf(cmd_ctx->output, "TLA2024 %u %u\r\n", (unsigned int)channel, (unsigned int)raw12);
    return true;
}

static bool adc_tla2024_handle_read_all(const app_command_ctx_t *cmd_ctx,
                                        adc_tla2024_t *ctx,
                                        char *args)
{
    char *saveptr = NULL;
    char *extra = strtok_r(args, " \t", &saveptr);
    uint16_t raw0 = 0U;
    uint16_t raw1 = 0U;
    esp_err_t err;

    if (extra != NULL)
    {
        adc_tla2024_printf(cmd_ctx->output, "ERR usage: TLA2024:READALL\r\n");
        return true;
    }

    err = adc_tla2024_read_channel_raw12(ctx, 0U, &raw0);
    if (err != ESP_OK)
    {
        adc_tla2024_printf(cmd_ctx->output, "ERR %d\r\n", (int)err);
        return true;
    }

    err = adc_tla2024_read_channel_raw12(ctx, 1U, &raw1);
    if (err != ESP_OK)
    {
        adc_tla2024_printf(cmd_ctx->output, "ERR %d\r\n", (int)err);
        return true;
    }

    app_commands_respond_ok_with_count(cmd_ctx->output, 2U);
    adc_tla2024_printf(cmd_ctx->output, "TLA2024 0 %u\r\n", (unsigned int)raw0);
    adc_tla2024_printf(cmd_ctx->output, "TLA2024 1 %u\r\n", (unsigned int)raw1);
    return true;
}

static bool adc_tla2024_handle_reg(const app_command_ctx_t *cmd_ctx,
                                   adc_tla2024_t *ctx,
                                   char *args)
{
    char *saveptr = NULL;
    char *reg_str = strtok_r(args, " \t", &saveptr);
    char *extra = strtok_r(NULL, " \t", &saveptr);
    uint8_t reg = 0U;
    uint16_t value = 0U;
    esp_err_t err;

    if (extra != NULL || !adc_tla2024_parse_reg_hex(reg_str, &reg))
    {
        adc_tla2024_printf(cmd_ctx->output, "ERR usage: TLA2024:REG <00|01>\r\n");
        return true;
    }

    err = adc_tla2024_read_reg16(ctx, reg, &value);
    if (err != ESP_OK)
    {
        adc_tla2024_printf(cmd_ctx->output, "ERR %d\r\n", (int)err);
        return true;
    }

    app_commands_respond_ok_with_count(cmd_ctx->output, 1U);
    adc_tla2024_printf(cmd_ctx->output, "TLA2024 REG %02X %04X\r\n", reg, value);
    return true;
}

static bool adc_tla2024_command_handler(const app_command_ctx_t *cmd_ctx,
                                        const char *cmd,
                                        char *args,
                                        void *user_ctx)
{
    adc_tla2024_t *ctx = (adc_tla2024_t *)user_ctx;

    if (cmd_ctx == NULL || cmd == NULL)
    {
        return false;
    }

    if (!adc_tla2024_is_supported_command(cmd))
    {
        return false;
    }

    if (!adc_tla2024_is_ready(ctx))
    {
        adc_tla2024_printf(cmd_ctx->output, "ERR not available\r\n");
        return true;
    }

    if (strcmp(cmd, ADC_TLA2024_CMD_READ) == 0)
    {
        return adc_tla2024_handle_read(cmd_ctx, ctx, args);
    }

    if (strcmp(cmd, ADC_TLA2024_CMD_READALL) == 0)
    {
        return adc_tla2024_handle_read_all(cmd_ctx, ctx, args);
    }

    if (strcmp(cmd, ADC_TLA2024_CMD_REG) == 0)
    {
        return adc_tla2024_handle_reg(cmd_ctx, ctx, args);
    }

    return false;
}

void adc_tla2024_register_commands(adc_tla2024_t *ctx)
{
    bool registered;

    if (!adc_tla2024_is_ready(ctx))
    {
        return;
    }

    registered = app_commands_register_custom_handler(adc_tla2024_command_handler, ctx);
    if (!registered)
    {
        ESP_LOGE(ADC_TLA2024_TAG, "app_commands_register_custom_handler failed");
    }
}
