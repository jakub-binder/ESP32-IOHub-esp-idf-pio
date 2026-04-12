#include "eeprom_24cs02.h"

#include "app_commands.h"
#include "driver/i2c.h"
#include "esp_log.h"

#include <ctype.h>
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
#define EEPROM_24CS02_SERIAL_START_ADDR   0x80U
#define EEPROM_24CS02_SERIAL_SIZE_BYTES   16U
#define EEPROM_24CS02_CMD_BUF_SIZE        128U
#define EEPROM_24CS02_WRITE_CYCLE_DELAY_MS 5U
#define EEPROM_24CS02_I2C_TIMEOUT_MS      100U

static const char *const EEPROM_24CS02_CMD_HELP = "eeprom.help";
static const char *const EEPROM_24CS02_CMD_READ16 = "eeprom.read16";
static const char *const EEPROM_24CS02_CMD_DUMP = "eeprom.dump";
static const char *const EEPROM_24CS02_CMD_WRITE = "eeprom.write";
static const char *const EEPROM_24CS02_CMD_READ_SN = "eeprom.read_sn";
static const char *const EEPROM_24CS02_TAG = "eeprom_24cs02";

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

static bool eeprom_24cs02_dev_addr_is_valid(uint8_t dev_addr)
{
    return (dev_addr != 0U) && ((dev_addr & 0x80U) == 0U);
}

esp_err_t eeprom_24cs02_init(eeprom_24cs02_t *ctx, const eeprom_24cs02_cfg_t *cfg)
{
    uint8_t data_dev_addr;

    if (ctx == NULL || cfg == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (cfg->i2c_port < 0 || cfg->i2c_port >= I2C_NUM_MAX)
    {
        return ESP_ERR_INVALID_ARG;
    }

    data_dev_addr = cfg->data_dev_addr;
    if (data_dev_addr == 0U)
    {
        data_dev_addr = cfg->dev_addr;
    }

    if (!eeprom_24cs02_dev_addr_is_valid(data_dev_addr))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if ((cfg->serial_dev_addr != 0U) &&
        !eeprom_24cs02_dev_addr_is_valid(cfg->serial_dev_addr))
    {
        return ESP_ERR_INVALID_ARG;
    }

    ctx->i2c_port = cfg->i2c_port;
    ctx->data_dev_addr = data_dev_addr;
    ctx->serial_dev_addr = cfg->serial_dev_addr;
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
                                        ctx->data_dev_addr,
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
        const size_t current_addr = (size_t)mem_addr + offset;
        uint8_t addr;
        esp_err_t err;

        if (current_addr >= EEPROM_24CS02_MEM_SIZE_BYTES)
        {
            return ESP_ERR_INVALID_ARG;
        }

        addr = (uint8_t)current_addr;
        tx[0] = addr;
        memcpy(&tx[1], &src[offset], EEPROM_24CS02_PAGE_SIZE_BYTES);

        err = i2c_master_write_to_device((i2c_port_t)ctx->i2c_port,
                                         ctx->data_dev_addr,
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

static esp_err_t eeprom_24cs02_read_serial(eeprom_24cs02_t *ctx,
                                           uint8_t mem_addr,
                                           void *out,
                                           size_t len)
{
    if (!eeprom_24cs02_is_ready(ctx) || out == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if ((ctx->serial_dev_addr == 0U) ||
        !eeprom_24cs02_range_is_valid(mem_addr, len))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_write_read_device((i2c_port_t)ctx->i2c_port,
                                        ctx->serial_dev_addr,
                                        &mem_addr,
                                        sizeof(mem_addr),
                                        out,
                                        len,
                                        pdMS_TO_TICKS(EEPROM_24CS02_I2C_TIMEOUT_MS));
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

static bool eeprom_24cs02_is_supported_command(const char *cmd)
{
    return (strcmp(cmd, EEPROM_24CS02_CMD_HELP) == 0) ||
           (strcmp(cmd, EEPROM_24CS02_CMD_READ16) == 0) ||
           (strcmp(cmd, EEPROM_24CS02_CMD_DUMP) == 0) ||
           (strcmp(cmd, EEPROM_24CS02_CMD_WRITE) == 0) ||
           (strcmp(cmd, EEPROM_24CS02_CMD_READ_SN) == 0);
}

static bool eeprom_24cs02_handle_help(const app_command_ctx_t *cmd_ctx)
{
    eeprom_24cs02_printf(cmd_ctx->output, "eeprom.help\r\n");
    eeprom_24cs02_printf(cmd_ctx->output, "eeprom.read16 <mem_addr>\r\n");
    eeprom_24cs02_printf(cmd_ctx->output, "eeprom.dump\r\n");
    eeprom_24cs02_printf(cmd_ctx->output, "eeprom.write <mem_addr> <hex_data>\r\n");
    eeprom_24cs02_printf(cmd_ctx->output, "eeprom.read_sn\r\n");
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
    char *extra = NULL;

    if (!eeprom_24cs02_parse_u8(addr_str, &mem_addr))
    {
        eeprom_24cs02_printf(cmd_ctx->output,
                             "ERR usage: eeprom.read16 <mem_addr>\r\n");
        return true;
    }

    extra = strtok_r(NULL, " \t", &saveptr);
    if (extra != NULL ||
        (mem_addr % EEPROM_24CS02_READ16_BYTES) != 0U ||
        !eeprom_24cs02_range_is_valid(mem_addr, sizeof(data)))
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

static int eeprom_24cs02_hex_nibble_value(char c)
{
    if (c >= '0' && c <= '9')
    {
        return (int)(c - '0');
    }

    if (c >= 'a' && c <= 'f')
    {
        return 10 + (int)(c - 'a');
    }

    if (c >= 'A' && c <= 'F')
    {
        return 10 + (int)(c - 'A');
    }

    return -1;
}

static bool eeprom_24cs02_parse_hex_bytes(const char *text,
                                          uint8_t *out_bytes,
                                          size_t out_capacity,
                                          size_t *out_len)
{
    size_t idx = 0;
    int high_nibble = -1;
    const char *cursor = text;

    if (text == NULL || out_bytes == NULL || out_len == NULL)
    {
        return false;
    }

    while (*cursor != '\0')
    {
        const unsigned char ch = (unsigned char)(*cursor);
        int nibble;

        if (isspace(ch) != 0)
        {
            cursor++;
            continue;
        }

        nibble = eeprom_24cs02_hex_nibble_value((char)ch);
        if (nibble < 0)
        {
            return false;
        }

        if (high_nibble < 0)
        {
            high_nibble = nibble;
        }
        else
        {
            if (idx >= out_capacity)
            {
                return false;
            }

            out_bytes[idx] = (uint8_t)(((unsigned int)high_nibble << 4U) |
                                        (unsigned int)nibble);
            idx++;
            high_nibble = -1;
        }

        cursor++;
    }

    if (high_nibble >= 0)
    {
        return false;
    }

    if (idx == 0U)
    {
        return false;
    }

    *out_len = idx;
    return true;
}

static bool eeprom_24cs02_handle_dump(const app_command_ctx_t *cmd_ctx,
                                      eeprom_24cs02_t *ctx,
                                      char *args)
{
    uint8_t data[EEPROM_24CS02_MEM_SIZE_BYTES];
    size_t offset;
    char *saveptr = NULL;
    char *extra = strtok_r(args, " \t", &saveptr);
    esp_err_t err;

    if (extra != NULL)
    {
        eeprom_24cs02_printf(cmd_ctx->output, "ERR usage: eeprom.dump\r\n");
        return true;
    }

    err = eeprom_24cs02_read(ctx, 0U, data, sizeof(data));
    if (err != ESP_OK)
    {
        eeprom_24cs02_printf(cmd_ctx->output, "ERR %d\r\n", (int)err);
        return true;
    }

    for (offset = 0; offset < EEPROM_24CS02_MEM_SIZE_BYTES; offset += EEPROM_24CS02_READ16_BYTES)
    {
        eeprom_24cs02_printf(
            cmd_ctx->output,
            "0x%02X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
            (unsigned int)offset,
            data[offset + 0U], data[offset + 1U], data[offset + 2U], data[offset + 3U],
            data[offset + 4U], data[offset + 5U], data[offset + 6U], data[offset + 7U],
            data[offset + 8U], data[offset + 9U], data[offset + 10U], data[offset + 11U],
            data[offset + 12U], data[offset + 13U], data[offset + 14U], data[offset + 15U]);
    }

    return true;
}

static bool eeprom_24cs02_handle_write(const app_command_ctx_t *cmd_ctx,
                                       eeprom_24cs02_t *ctx,
                                       char *args)
{
    char *saveptr = NULL;
    char *addr_str = strtok_r(args, " \t", &saveptr);
    char *data_str = strtok_r(NULL, " \t", &saveptr);
    char *extra = strtok_r(NULL, " \t", &saveptr);
    uint8_t mem_addr;
    uint8_t data[EEPROM_24CS02_MEM_SIZE_BYTES];
    size_t data_len = 0;
    esp_err_t err;

    if (extra != NULL ||
        !eeprom_24cs02_parse_u8(addr_str, &mem_addr) ||
        !eeprom_24cs02_parse_hex_bytes(data_str, data, sizeof(data), &data_len))
    {
        eeprom_24cs02_printf(cmd_ctx->output,
                             "ERR usage: eeprom.write <mem_addr> <hex_data>\r\n");
        return true;
    }

    if ((mem_addr % EEPROM_24CS02_PAGE_SIZE_BYTES) != 0U ||
        (data_len % EEPROM_24CS02_PAGE_SIZE_BYTES) != 0U ||
        !eeprom_24cs02_range_is_valid(mem_addr, data_len))
    {
        eeprom_24cs02_printf(cmd_ctx->output, "ERR invalid range/alignment\r\n");
        return true;
    }

    err = eeprom_24cs02_write(ctx, mem_addr, data, data_len);
    if (err != ESP_OK)
    {
        eeprom_24cs02_printf(cmd_ctx->output, "ERR %d\r\n", (int)err);
        return true;
    }

    eeprom_24cs02_printf(cmd_ctx->output, "OK\r\n");
    return true;
}

static bool eeprom_24cs02_handle_read_sn(const app_command_ctx_t *cmd_ctx,
                                         eeprom_24cs02_t *ctx,
                                         char *args)
{
    static const char hex_chars[] = "0123456789ABCDEF";
    char *saveptr = NULL;
    char *extra = strtok_r(args, " \t", &saveptr);
    uint8_t serial[EEPROM_24CS02_SERIAL_SIZE_BYTES];
    char serial_text[(EEPROM_24CS02_SERIAL_SIZE_BYTES * 2U) + 1U];
    size_t i;
    esp_err_t err;

    if (extra != NULL)
    {
        eeprom_24cs02_printf(cmd_ctx->output, "ERR usage: eeprom.read_sn\r\n");
        return true;
    }

    if (ctx->serial_dev_addr == 0U)
    {
        eeprom_24cs02_printf(cmd_ctx->output, "ERR serial address not configured\r\n");
        return true;
    }

    err = eeprom_24cs02_read_serial(ctx,
                                    EEPROM_24CS02_SERIAL_START_ADDR,
                                    serial,
                                    sizeof(serial));
    if (err != ESP_OK)
    {
        eeprom_24cs02_printf(cmd_ctx->output, "ERR %d\r\n", (int)err);
        return true;
    }

    for (i = 0; i < sizeof(serial); i++)
    {
        serial_text[i * 2U] = hex_chars[(serial[i] >> 4U) & 0x0FU];
        serial_text[(i * 2U) + 1U] = hex_chars[serial[i] & 0x0FU];
    }
    serial_text[sizeof(serial_text) - 1U] = '\0';
    eeprom_24cs02_printf(cmd_ctx->output, "%s\r\n", serial_text);
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

    if (!eeprom_24cs02_is_supported_command(cmd))
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

    if (strcmp(cmd, EEPROM_24CS02_CMD_HELP) == 0)
    {
        return eeprom_24cs02_handle_help(cmd_ctx);
    }

    if (strcmp(cmd, EEPROM_24CS02_CMD_READ16) == 0)
    {
        return eeprom_24cs02_handle_read16(cmd_ctx, ctx, args);
    }

    if (strcmp(cmd, EEPROM_24CS02_CMD_DUMP) == 0)
    {
        return eeprom_24cs02_handle_dump(cmd_ctx, ctx, args);
    }

    if (strcmp(cmd, EEPROM_24CS02_CMD_WRITE) == 0)
    {
        return eeprom_24cs02_handle_write(cmd_ctx, ctx, args);
    }

    if (strcmp(cmd, EEPROM_24CS02_CMD_READ_SN) == 0)
    {
        return eeprom_24cs02_handle_read_sn(cmd_ctx, ctx, args);
    }

    return false;
}

void eeprom_24cs02_register_commands(eeprom_24cs02_t *ctx)
{
    bool registered;

    if (!eeprom_24cs02_is_ready(ctx))
    {
        return;
    }

    registered = app_commands_register_custom_handler(eeprom_24cs02_command_handler, ctx);
    if (!registered)
    {
        ESP_LOGE(EEPROM_24CS02_TAG, "app_commands_register_custom_handler failed");
    }
}
