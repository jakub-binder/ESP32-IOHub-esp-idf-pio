#include "eeprom_24cs02.h"

#include "app_commands.h"
#include "driver/i2c.h"

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define EEPROM_24CS02_MEM_SIZE_BYTES      256U
#define EEPROM_24CS02_PAGE_SIZE_BYTES     8U
#define EEPROM_24CS02_READ16_BYTES        16U
#define EEPROM_24CS02_CMD_BUF_SIZE        128U
#define EEPROM_24CS02_WRITE_CYCLE_DELAY_MS 5U
#define EEPROM_24CS02_I2C_TIMEOUT_MS      100U

static bool eeprom_24cs02_is_ready(const eeprom_24cs02_t *ctx)
{
    return (ctx != NULL) && ctx->initialized;
}

static bool eeprom_24cs02_range_is_valid(uint8_t mem_addr, size_t len)
{
    const size_t start = (size_t)mem_addr;

    if (len == 0U || len > EEPROM_24CS02_MEM_SIZE_BYTES)
    {
        return false;
    }

    if (start >= EEPROM_24CS02_MEM_SIZE_BYTES)
    {
        return false;
    }

    if (len > (EEPROM_24CS02_MEM_SIZE_BYTES - start))
    {
        return false;
    }

    return true;
}

esp_err_t eeprom_24cs02_init(eeprom_24cs02_t *ctx, const eeprom_24cs02_cfg_t *cfg)
{
    if (ctx == NULL || cfg == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ctx->i2c_port = cfg->i2c_port;
    ctx->dev_addr = cfg->dev_addr;
    ctx->initialized = true;

    return ESP_OK;
}

esp_err_t eeprom_24cs02_read(eeprom_24cs02_t *ctx,
                             uint8_t mem_addr,
                             void *out,
                             size_t len)
{
    if (!eeprom_24cs02_is_ready(ctx) || out == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!eeprom_24cs02_range_is_valid(mem_addr, len))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_write_read_device((i2c_port_t)ctx->i2c_port,
                                        ctx->dev_addr,
                                        &mem_addr,
                                        sizeof(mem_addr),
                                        out,
                                        len,
                                        pdMS_TO_TICKS(EEPROM_24CS02_I2C_TIMEOUT_MS));
}

esp_err_t eeprom_24cs02_write(eeprom_24cs02_t *ctx,
                              uint8_t mem_addr,
                              const void *data,
                              size_t len)
{
    const uint8_t *src = (const uint8_t *)data;
    size_t offset = 0;

    if (!eeprom_24cs02_is_ready(ctx) || data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!eeprom_24cs02_range_is_valid(mem_addr, len))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if ((mem_addr % EEPROM_24CS02_PAGE_SIZE_BYTES) != 0U ||
        (len % EEPROM_24CS02_PAGE_SIZE_BYTES) != 0U)
    {
        return ESP_ERR_INVALID_ARG;
    }

    while (offset < len)
    {
        uint8_t tx[1 + EEPROM_24CS02_PAGE_SIZE_BYTES];
        const uint8_t addr = (uint8_t)((size_t)mem_addr + offset);
        esp_err_t err;

        tx[0] = addr;
        memcpy(&tx[1], &src[offset], EEPROM_24CS02_PAGE_SIZE_BYTES);

        err = i2c_master_write_to_device((i2c_port_t)ctx->i2c_port,
                                         ctx->dev_addr,
                                         tx,
                                         sizeof(tx),
                                         pdMS_TO_TICKS(EEPROM_24CS02_I2C_TIMEOUT_MS));
        if (err != ESP_OK)
        {
            return err;
        }

        offset += EEPROM_24CS02_PAGE_SIZE_BYTES;
        vTaskDelay(pdMS_TO_TICKS(EEPROM_24CS02_WRITE_CYCLE_DELAY_MS));
    }

    return ESP_OK;
}

static void eeprom_24cs02_printf(app_command_output_fn output, const char *fmt, ...)
{
    char buf[EEPROM_24CS02_CMD_BUF_SIZE];
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

static bool eeprom_24cs02_parse_u8(const char *text, uint8_t *out_value)
{
    char *endptr;
    unsigned long parsed;

    if (text == NULL || out_value == NULL || *text == '\0')
    {
        return false;
    }

    errno = 0;
    parsed = strtoul(text, &endptr, 0);
    if (*endptr != '\0' || errno == ERANGE || parsed > UCHAR_MAX)
    {
        return false;
    }

    *out_value = (uint8_t)parsed;
    return true;
}

static bool eeprom_24cs02_handle_help(const app_command_ctx_t *cmd_ctx)
{
    eeprom_24cs02_printf(cmd_ctx->output, "eeprom.help\r\n");
    eeprom_24cs02_printf(cmd_ctx->output, "eeprom.read16 <mem_addr>\r\n");
    return true;
}

static bool eeprom_24cs02_handle_read16(const app_command_ctx_t *cmd_ctx,
                                        eeprom_24cs02_t *ctx,
                                        char *args)
{
    char *saveptr = NULL;
    char *addr_str = strtok_r(args, " \t", &saveptr);
    uint8_t mem_addr;
    uint8_t data[EEPROM_24CS02_READ16_BYTES];
    esp_err_t err;

    if (!eeprom_24cs02_parse_u8(addr_str, &mem_addr))
    {
        eeprom_24cs02_printf(cmd_ctx->output,
                             "ERR usage: eeprom.read16 <mem_addr>\r\n");
        return true;
    }

    err = eeprom_24cs02_read(ctx, mem_addr, data, sizeof(data));
    if (err != ESP_OK)
    {
        eeprom_24cs02_printf(cmd_ctx->output, "ERR %d\r\n", (int)err);
        return true;
    }

    eeprom_24cs02_printf(
        cmd_ctx->output,
        "0x%02X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
        mem_addr,
        data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
        data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
    return true;
}

static bool eeprom_24cs02_command_handler(const app_command_ctx_t *cmd_ctx,
                                          const char *cmd,
                                          char *args,
                                          void *user_ctx)
{
    eeprom_24cs02_t *ctx = (eeprom_24cs02_t *)user_ctx;

    if (cmd_ctx == NULL || cmd == NULL)
    {
        return false;
    }

    if (strcmp(cmd, "eeprom.help") != 0 &&
        strcmp(cmd, "eeprom.read16") != 0)
    {
        return false;
    }

    if (!cmd_ctx->allow_debug_commands)
    {
        eeprom_24cs02_printf(cmd_ctx->output, "ERR not allowed\r\n");
        return true;
    }

    if (!eeprom_24cs02_is_ready(ctx))
    {
        eeprom_24cs02_printf(cmd_ctx->output, "ERR not available\r\n");
        return true;
    }

    if (strcmp(cmd, "eeprom.help") == 0)
    {
        return eeprom_24cs02_handle_help(cmd_ctx);
    }

    return eeprom_24cs02_handle_read16(cmd_ctx, ctx, args);
}

void eeprom_24cs02_register_commands(eeprom_24cs02_t *ctx)
{
    if (!eeprom_24cs02_is_ready(ctx))
    {
        return;
    }

    (void)app_commands_register_custom_handler(eeprom_24cs02_command_handler, ctx);
}
