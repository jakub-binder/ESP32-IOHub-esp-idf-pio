#include "temp_lm75bdp.h"

#include "app_commands.h"
#include "driver/i2c.h"
#include "esp_log.h"

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TEMP_LM75BDP_REG_TEMP         0x00U
#define TEMP_LM75BDP_REG_CONF         0x01U
#define TEMP_LM75BDP_REG_THYST        0x02U
#define TEMP_LM75BDP_REG_TOS          0x03U
#define TEMP_LM75BDP_I2C_TIMEOUT_MS   100U
#define TEMP_LM75BDP_CMD_BUF_SIZE     128U

static const char *const TEMP_LM75BDP_CMD_HELP = "temp.help";
static const char *const TEMP_LM75BDP_CMD_READ = "temp.read";
static const char *const TEMP_LM75BDP_CMD_SET_THRESHOLDS = "temp.set_thresholds";
static const char *const TEMP_LM75BDP_TAG = "temp_lm75bdp";

static bool temp_lm75bdp_is_ready(const temp_lm75bdp_t *ctx)
{
    return (ctx != NULL) && ctx->initialized;
}

static bool temp_lm75bdp_dev_addr_is_valid(uint8_t dev_addr)
{
    return (dev_addr != 0U) && ((dev_addr & 0x80U) == 0U);
}

static esp_err_t temp_lm75bdp_read_register16(temp_lm75bdp_t *ctx,
                                              uint8_t reg_addr,
                                              uint16_t *out_value)
{
    uint8_t reg = reg_addr;
    uint8_t rx[2];
    esp_err_t err;

    if (!temp_lm75bdp_is_ready(ctx) || out_value == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    err = i2c_master_write_read_device((i2c_port_t)ctx->i2c_port,
                                       ctx->dev_addr,
                                       &reg,
                                       sizeof(reg),
                                       rx,
                                       sizeof(rx),
                                       pdMS_TO_TICKS(TEMP_LM75BDP_I2C_TIMEOUT_MS));
    if (err != ESP_OK)
    {
        ESP_LOGE(TEMP_LM75BDP_TAG, "read reg 0x%02X failed: %d", reg_addr, (int)err);
        return err;
    }

    *out_value = ((uint16_t)rx[0] << 8U) | (uint16_t)rx[1];
    return ESP_OK;
}

static esp_err_t temp_lm75bdp_write_register16(temp_lm75bdp_t *ctx,
                                               uint8_t reg_addr,
                                               uint16_t value)
{
    uint8_t tx[3];
    esp_err_t err;

    if (!temp_lm75bdp_is_ready(ctx))
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
                                     pdMS_TO_TICKS(TEMP_LM75BDP_I2C_TIMEOUT_MS));
    if (err != ESP_OK)
    {
        ESP_LOGE(TEMP_LM75BDP_TAG, "write reg 0x%02X failed: %d", reg_addr, (int)err);
    }

    return err;
}

static uint16_t temp_lm75bdp_encode_threshold_c(float temp_c)
{
    const float steps = temp_c * 2.0f;
    const int16_t raw = (int16_t)roundf(steps);

    return ((uint16_t)raw) << 7U;
}

esp_err_t temp_lm75bdp_init(temp_lm75bdp_t *ctx,
                            const temp_lm75bdp_cfg_t *cfg)
{
    if (ctx == NULL || cfg == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (cfg->i2c_port < 0 || cfg->i2c_port >= I2C_NUM_MAX)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!temp_lm75bdp_dev_addr_is_valid(cfg->dev_addr))
    {
        return ESP_ERR_INVALID_ARG;
    }

    ctx->i2c_port = cfg->i2c_port;
    ctx->dev_addr = cfg->dev_addr;
    ctx->initialized = true;

    return ESP_OK;
}

esp_err_t temp_lm75bdp_read_temperature_c(temp_lm75bdp_t *ctx,
                                          float *out_temp_c)
{
    uint16_t raw16 = 0U;
    int16_t signed_raw;
    esp_err_t err;

    if (out_temp_c == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!temp_lm75bdp_is_ready(ctx))
    {
        return ESP_ERR_INVALID_STATE;
    }

    err = temp_lm75bdp_read_register16(ctx, TEMP_LM75BDP_REG_TEMP, &raw16);
    if (err != ESP_OK)
    {
        return err;
    }

    signed_raw = (int16_t)raw16;
    signed_raw >>= 5;
    *out_temp_c = (float)signed_raw * 0.125f;

    return ESP_OK;
}

esp_err_t temp_lm75bdp_set_thresholds_c(temp_lm75bdp_t *ctx,
                                        float thyst_c,
                                        float tos_c)
{
    esp_err_t err;

    if (!temp_lm75bdp_is_ready(ctx))
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!(thyst_c < tos_c))
    {
        return ESP_ERR_INVALID_ARG;
    }

    err = temp_lm75bdp_write_register16(ctx,
                                        TEMP_LM75BDP_REG_THYST,
                                        temp_lm75bdp_encode_threshold_c(thyst_c));
    if (err != ESP_OK)
    {
        return err;
    }

    err = temp_lm75bdp_write_register16(ctx,
                                        TEMP_LM75BDP_REG_TOS,
                                        temp_lm75bdp_encode_threshold_c(tos_c));
    if (err != ESP_OK)
    {
        return err;
    }

    return ESP_OK;
}

static void temp_lm75bdp_printf(app_command_output_fn output, const char *fmt, ...)
{
    char buf[TEMP_LM75BDP_CMD_BUF_SIZE];
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

static bool temp_lm75bdp_parse_float(const char *text, float *out_value)
{
    char *endptr;

    if (text == NULL || out_value == NULL || *text == '\0')
    {
        return false;
    }

    errno = 0;
    *out_value = strtof(text, &endptr);
    if (*endptr != '\0' || errno == ERANGE || !isfinite(*out_value))
    {
        return false;
    }

    return true;
}

static bool temp_lm75bdp_is_supported_command(const char *cmd)
{
    return (strcmp(cmd, TEMP_LM75BDP_CMD_HELP) == 0) ||
           (strcmp(cmd, TEMP_LM75BDP_CMD_READ) == 0) ||
           (strcmp(cmd, TEMP_LM75BDP_CMD_SET_THRESHOLDS) == 0);
}

static bool temp_lm75bdp_handle_help(const app_command_ctx_t *cmd_ctx)
{
    app_commands_respond_ok_with_count(cmd_ctx->output, 3U);
    temp_lm75bdp_printf(cmd_ctx->output, "temp.help\r\n");
    temp_lm75bdp_printf(cmd_ctx->output, "temp.read\r\n");
    temp_lm75bdp_printf(cmd_ctx->output, "temp.set_thresholds <thyst_c> <tos_c>\r\n");
    return true;
}

static bool temp_lm75bdp_handle_read(const app_command_ctx_t *cmd_ctx,
                                     temp_lm75bdp_t *ctx,
                                     char *args)
{
    char *saveptr = NULL;
    char *extra = strtok_r(args, " \t", &saveptr);
    float temp_c;
    esp_err_t err;

    if (extra != NULL)
    {
        temp_lm75bdp_printf(cmd_ctx->output, "ERR usage: temp.read\r\n");
        return true;
    }

    err = temp_lm75bdp_read_temperature_c(ctx, &temp_c);
    if (err != ESP_OK)
    {
        temp_lm75bdp_printf(cmd_ctx->output, "ERR %d\r\n", (int)err);
        return true;
    }

    app_commands_respond_ok_with_count(cmd_ctx->output, 1U);
    temp_lm75bdp_printf(cmd_ctx->output, "%.3f\r\n", temp_c);
    return true;
}

static bool temp_lm75bdp_handle_set_thresholds(const app_command_ctx_t *cmd_ctx,
                                               temp_lm75bdp_t *ctx,
                                               char *args)
{
    char *saveptr = NULL;
    char *thyst_str = strtok_r(args, " \t", &saveptr);
    char *tos_str = strtok_r(NULL, " \t", &saveptr);
    char *extra = strtok_r(NULL, " \t", &saveptr);
    float thyst_c;
    float tos_c;
    esp_err_t err;

    if (extra != NULL ||
        !temp_lm75bdp_parse_float(thyst_str, &thyst_c) ||
        !temp_lm75bdp_parse_float(tos_str, &tos_c))
    {
        temp_lm75bdp_printf(cmd_ctx->output,
                            "ERR usage: temp.set_thresholds <thyst_c> <tos_c>\r\n");
        return true;
    }

    err = temp_lm75bdp_set_thresholds_c(ctx, thyst_c, tos_c);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_INVALID_ARG)
        {
            temp_lm75bdp_printf(cmd_ctx->output, "ERR invalid thresholds (thyst < tos required)\r\n");
            return true;
        }
        temp_lm75bdp_printf(cmd_ctx->output, "ERR %d\r\n", (int)err);
        return true;
    }

    app_commands_respond_ok_with_count(cmd_ctx->output, 1U);
    temp_lm75bdp_printf(cmd_ctx->output, "thresholds updated\r\n");
    return true;
}

static bool temp_lm75bdp_command_handler(const app_command_ctx_t *cmd_ctx,
                                         const char *cmd,
                                         char *args,
                                         void *user_ctx)
{
    temp_lm75bdp_t *ctx = (temp_lm75bdp_t *)user_ctx;

    if (cmd_ctx == NULL || cmd == NULL)
    {
        return false;
    }

    if (!temp_lm75bdp_is_supported_command(cmd))
    {
        return false;
    }

    if (!temp_lm75bdp_is_ready(ctx))
    {
        temp_lm75bdp_printf(cmd_ctx->output, "ERR not available\r\n");
        return true;
    }

    if (strcmp(cmd, TEMP_LM75BDP_CMD_HELP) == 0)
    {
        return temp_lm75bdp_handle_help(cmd_ctx);
    }

    if (strcmp(cmd, TEMP_LM75BDP_CMD_READ) == 0)
    {
        return temp_lm75bdp_handle_read(cmd_ctx, ctx, args);
    }

    if (strcmp(cmd, TEMP_LM75BDP_CMD_SET_THRESHOLDS) == 0)
    {
        return temp_lm75bdp_handle_set_thresholds(cmd_ctx, ctx, args);
    }

    return false;
}

void temp_lm75bdp_register_commands(temp_lm75bdp_t *ctx)
{
    bool registered;

    if (!temp_lm75bdp_is_ready(ctx))
    {
        return;
    }

    registered = app_commands_register_custom_handler(temp_lm75bdp_command_handler, ctx);
    if (!registered)
    {
        ESP_LOGE(TEMP_LM75BDP_TAG, "app_commands_register_custom_handler failed");
    }
}
