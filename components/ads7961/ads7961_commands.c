#include "ads7961.h"

#include "app_commands.h"
#include "esp_err.h"
#include "esp_log.h"

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ADS7961_CMD_BUF_SIZE 128U

static const char *const ADS7961_CMD_HELP = "ADC:HELP";
static const char *const ADS7961_CMD_READ_CH = "ADC:READ-CH";
static const char *const ADS7961_CMD_READAVG_CH = "ADC:READAVG-CH";
static const char *const ADS7961_CMD_TAG = "ads7961_cmd";

static void ads7961_cmd_printf(app_command_output_fn output, const char *fmt, ...)
{
    char buf[ADS7961_CMD_BUF_SIZE];
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

static void ads7961_cmd_respond_error(app_command_output_fn output, const char *fmt, ...)
{
    char detail[ADS7961_CMD_BUF_SIZE];
    const char *detail_text = NULL;
    va_list args;

    if (output == NULL)
    {
        return;
    }

    if (fmt != NULL)
    {
        va_start(args, fmt);
        vsnprintf(detail, sizeof(detail), fmt, args);
        va_end(args);
        detail_text = detail;
    }

    output("ERR-1\r\n");
    output("Error: ");
    if (detail_text != NULL)
    {
        output(detail_text);
    }
    else
    {
        output("Unknown error");
    }
    output("\r\n");
}

static bool ads7961_parse_channel(const char *text, uint8_t *out_channel)
{
    char *endptr = NULL;
    unsigned long value;

    if (text == NULL || out_channel == NULL || *text == '\0')
    {
        return false;
    }

    if (*text == '-')
    {
        return false;
    }

    errno = 0;
    value = strtoul(text, &endptr, 10);
    if (errno != 0 || endptr == NULL || *endptr != '\0')
    {
        return false;
    }

    if (value > 15UL)
    {
        return false;
    }

    *out_channel = (uint8_t)value;
    return true;
}

static bool ads7961_is_supported_command(const char *cmd)
{
    return (strcmp(cmd, ADS7961_CMD_HELP) == 0) ||
           (strcmp(cmd, ADS7961_CMD_READ_CH) == 0) ||
           (strcmp(cmd, ADS7961_CMD_READAVG_CH) == 0);
}

static bool ads7961_handle_help(const app_command_ctx_t *cmd_ctx, char *args)
{
    if (args == NULL)
    {
        ads7961_cmd_respond_error(cmd_ctx->output, "Usage: ADC:HELP");
        return true;
    }

    char *saveptr = NULL;
    char *extra = strtok_r(args, " \t", &saveptr);

    if (extra != NULL)
    {
        ads7961_cmd_respond_error(cmd_ctx->output, "Usage: ADC:HELP");
        return true;
    }

    app_commands_respond_ok_with_count(cmd_ctx->output, 3U);
    ads7961_cmd_printf(cmd_ctx->output, "ADC:HELP\r\n");
    ads7961_cmd_printf(cmd_ctx->output, "ADC:READ-CH <0..15>\r\n");
    ads7961_cmd_printf(cmd_ctx->output, "ADC:READAVG-CH <0..15>\r\n");
    return true;
}

static bool ads7961_handle_read_ch(const app_command_ctx_t *cmd_ctx,
                                   ads7961_t *ctx,
                                   char *args)
{
    uint8_t channel = 0U;
    uint8_t code8 = 0U;
    float volts = 0.0f;
    esp_err_t err;

    if (args == NULL)
    {
        ads7961_cmd_respond_error(cmd_ctx->output, "Usage: ADC:READ-CH <0..15>");
        return true;
    }

    char *saveptr = NULL;
    char *channel_str = strtok_r(args, " \t", &saveptr);
    char *extra = strtok_r(NULL, " \t", &saveptr);

    if (extra != NULL || !ads7961_parse_channel(channel_str, &channel))
    {
        ads7961_cmd_respond_error(cmd_ctx->output, "Usage: ADC:READ-CH <0..15>");
        return true;
    }

    err = ads7961_read_channel(ctx, channel, &code8, NULL);
    if (err != ESP_OK)
    {
        ads7961_cmd_respond_error(cmd_ctx->output,
                                  "ADC read failed: %s (%d)",
                                  esp_err_to_name(err),
                                  (int)err);
        return true;
    }

    err = ads7961_code8_to_volts(ctx, code8, &volts);
    if (err != ESP_OK)
    {
        ads7961_cmd_respond_error(cmd_ctx->output,
                                  "Voltage conversion failed: %s (%d)",
                                  esp_err_to_name(err),
                                  (int)err);
        return true;
    }

    app_commands_respond_ok_with_count(cmd_ctx->output, 1U);
    ads7961_cmd_printf(cmd_ctx->output, "%.6f\r\n", volts);
    return true;
}

static bool ads7961_handle_readavg_ch(const app_command_ctx_t *cmd_ctx,
                                      ads7961_t *ctx,
                                      char *args)
{
    uint8_t channel = 0U;
    float avg_volts = 0.0f;
    float avg_code8 = 0.0f;
    esp_err_t err;

    if (args == NULL)
    {
        ads7961_cmd_respond_error(cmd_ctx->output, "Usage: ADC:READAVG-CH <0..15>");
        return true;
    }

    char *saveptr = NULL;
    char *channel_str = strtok_r(args, " \t", &saveptr);
    char *extra = strtok_r(NULL, " \t", &saveptr);

    if (extra != NULL || !ads7961_parse_channel(channel_str, &channel))
    {
        ads7961_cmd_respond_error(cmd_ctx->output, "Usage: ADC:READAVG-CH <0..15>");
        return true;
    }

    err = ads7961_read_channel_avg(ctx, channel, &avg_volts, &avg_code8);
    if (err != ESP_OK)
    {
        ads7961_cmd_respond_error(cmd_ctx->output,
                                  "ADC average read failed: %s (%d)",
                                  esp_err_to_name(err),
                                  (int)err);
        return true;
    }

    app_commands_respond_ok_with_count(cmd_ctx->output, 1U);
    ads7961_cmd_printf(cmd_ctx->output, "%.6f\r\n", avg_volts);
    return true;
}

static bool ads7961_command_handler(const app_command_ctx_t *cmd_ctx,
                                    const char *cmd,
                                    char *args,
                                    void *user_ctx)
{
    ads7961_t *ctx = (ads7961_t *)user_ctx;

    if (cmd_ctx == NULL || cmd == NULL)
    {
        return false;
    }

    if (!ads7961_is_supported_command(cmd))
    {
        return false;
    }

    if (!ads7961_is_initialized(ctx))
    {
        ads7961_cmd_respond_error(cmd_ctx->output, "ADS7961 not available");
        return true;
    }

    if (strcmp(cmd, ADS7961_CMD_HELP) == 0)
    {
        return ads7961_handle_help(cmd_ctx, args);
    }

    if (strcmp(cmd, ADS7961_CMD_READ_CH) == 0)
    {
        return ads7961_handle_read_ch(cmd_ctx, ctx, args);
    }

    if (strcmp(cmd, ADS7961_CMD_READAVG_CH) == 0)
    {
        return ads7961_handle_readavg_ch(cmd_ctx, ctx, args);
    }

    return false;
}

void ads7961_register_commands(ads7961_t *ctx)
{
    bool registered;

    if (!ads7961_is_initialized(ctx))
    {
        return;
    }

    registered = app_commands_register_custom_handler(ads7961_command_handler, ctx);
    if (!registered)
    {
        ESP_LOGE(ADS7961_CMD_TAG, "app_commands_register_custom_handler failed");
    }
}
