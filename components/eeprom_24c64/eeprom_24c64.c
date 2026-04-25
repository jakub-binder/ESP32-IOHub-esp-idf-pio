#include "eeprom_24c64.h"

#include "app_commands.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EEPROM_24C64_MEM_SIZE_BYTES          8192U
#define EEPROM_24C64_PAGE_SIZE_BYTES         32U
#define EEPROM_24C64_DUMP_SIZE_BYTES         256U
#define EEPROM_24C64_DUMP_LINE_SIZE_BYTES    16U
#define EEPROM_24C64_DUMP_LINE_COUNT         (EEPROM_24C64_DUMP_SIZE_BYTES / EEPROM_24C64_DUMP_LINE_SIZE_BYTES)
#define EEPROM_24C64_DEFAULT_ACK_POLL_MS     20U
#define EEPROM_24C64_I2C_TIMEOUT_MS          100U
#define EEPROM_24C64_ACK_POLL_STEP_MS        1U
#define EEPROM_24C64_MAX_READ_CHUNK_BYTES    64U
#define EEPROM_24C64_CMD_BUF_SIZE            128U

static const char *const EEPROM_24C64_CMD_READ = "EEPROM64:READ";
static const char *const EEPROM_24C64_CMD_WRITE = "EEPROM64:WRITE";
static const char *const EEPROM_24C64_CMD_DUMP = "EEPROM64:DUMP";
static const char *const EEPROM_24C64_TAG = "eeprom_24c64";

static bool eeprom_24c64_is_ready(const eeprom_24c64_t *ctx)
{
    return (ctx != NULL) && ctx->initialized;
}

static bool eeprom_24c64_dev_addr_is_valid(uint8_t dev_addr)
{
    return (dev_addr != 0U) && ((dev_addr & 0x80U) == 0U);
}

static bool eeprom_24c64_range_is_valid(uint16_t addr, size_t len)
{
    const size_t start = (size_t)addr;

    if (len == 0U || len > EEPROM_24C64_MEM_SIZE_BYTES)
    {
        return false;
    }

    if (start >= EEPROM_24C64_MEM_SIZE_BYTES)
    {
        return false;
    }

    if (len > (EEPROM_24C64_MEM_SIZE_BYTES - start))
    {
        return false;
    }

    return true;
}

static esp_err_t eeprom_24c64_ack_poll(eeprom_24c64_t *ctx)
{
    const TickType_t timeout_ticks = pdMS_TO_TICKS(ctx->ack_poll_timeout_ms);
    const TickType_t poll_step_ticks = pdMS_TO_TICKS(EEPROM_24C64_ACK_POLL_STEP_MS);
    const TickType_t start_ticks = xTaskGetTickCount();

    while (true)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        esp_err_t err;

        if (cmd == NULL)
        {
            return ESP_ERR_NO_MEM;
        }

        err = i2c_master_start(cmd);
        if (err == ESP_OK)
        {
            err = i2c_master_write_byte(cmd,
                                        (uint8_t)((ctx->dev_addr << 1U) | I2C_MASTER_WRITE),
                                        true);
        }
        if (err == ESP_OK)
        {
            err = i2c_master_stop(cmd);
        }
        if (err == ESP_OK)
        {
            err = i2c_master_cmd_begin((i2c_port_t)ctx->i2c_port,
                                       cmd,
                                       pdMS_TO_TICKS(EEPROM_24C64_I2C_TIMEOUT_MS));
        }

        i2c_cmd_link_delete(cmd);

        if (err == ESP_OK)
        {
            return ESP_OK;
        }

        if ((xTaskGetTickCount() - start_ticks) >= timeout_ticks)
        {
            return ESP_ERR_TIMEOUT;
        }

        vTaskDelay(poll_step_ticks);
    }
}

esp_err_t eeprom_24c64_init(eeprom_24c64_t *ctx, const eeprom_24c64_cfg_t *cfg)
{
    if (ctx == NULL || cfg == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (cfg->i2c_port < 0 || cfg->i2c_port >= I2C_NUM_MAX)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!eeprom_24c64_dev_addr_is_valid(cfg->dev_addr))
    {
        return ESP_ERR_INVALID_ARG;
    }

    ctx->i2c_port = cfg->i2c_port;
    ctx->dev_addr = cfg->dev_addr;
    ctx->ack_poll_timeout_ms = (cfg->ack_poll_timeout_ms == 0U)
                                   ? EEPROM_24C64_DEFAULT_ACK_POLL_MS
                                   : cfg->ack_poll_timeout_ms;
    ctx->initialized = true;

    return ESP_OK;
}

esp_err_t eeprom_24c64_read(eeprom_24c64_t *ctx,
                            uint16_t mem_addr,
                            void *out,
                            size_t len)
{
    uint8_t *dst = (uint8_t *)out;
    size_t offset = 0U;

    if (!eeprom_24c64_is_ready(ctx) || out == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!eeprom_24c64_range_is_valid(mem_addr, len))
    {
        return ESP_ERR_INVALID_ARG;
    }

    while (offset < len)
    {
        const uint16_t current_addr = (uint16_t)((uint32_t)mem_addr + (uint32_t)offset);
        const size_t remaining = len - offset;
        const size_t chunk = (remaining > EEPROM_24C64_MAX_READ_CHUNK_BYTES)
                                 ? EEPROM_24C64_MAX_READ_CHUNK_BYTES
                                 : remaining;
        const uint8_t addr_buf[2] = {
            (uint8_t)(current_addr >> 8U),
            (uint8_t)(current_addr & 0xFFU),
        };
        esp_err_t err = i2c_master_write_read_device((i2c_port_t)ctx->i2c_port,
                                                     ctx->dev_addr,
                                                     addr_buf,
                                                     sizeof(addr_buf),
                                                     &dst[offset],
                                                     chunk,
                                                     pdMS_TO_TICKS(EEPROM_24C64_I2C_TIMEOUT_MS));
        if (err != ESP_OK)
        {
            return err;
        }

        offset += chunk;
    }

    return ESP_OK;
}

esp_err_t eeprom_24c64_write(eeprom_24c64_t *ctx,
                             uint16_t mem_addr,
                             const void *data,
                             size_t len)
{
    const uint8_t *src = (const uint8_t *)data;
    size_t offset = 0U;

    if (!eeprom_24c64_is_ready(ctx) || data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!eeprom_24c64_range_is_valid(mem_addr, len))
    {
        return ESP_ERR_INVALID_ARG;
    }

    while (offset < len)
    {
        uint8_t tx[2U + EEPROM_24C64_PAGE_SIZE_BYTES];
        const uint16_t current_addr = (uint16_t)((uint32_t)mem_addr + (uint32_t)offset);
        const size_t page_offset = (size_t)current_addr % EEPROM_24C64_PAGE_SIZE_BYTES;
        const size_t page_space = EEPROM_24C64_PAGE_SIZE_BYTES - page_offset;
        const size_t remaining = len - offset;
        const size_t chunk = (remaining > page_space) ? page_space : remaining;
        esp_err_t err;

        tx[0] = (uint8_t)(current_addr >> 8U);
        tx[1] = (uint8_t)(current_addr & 0xFFU);
        memcpy(&tx[2], &src[offset], chunk);

        err = i2c_master_write_to_device((i2c_port_t)ctx->i2c_port,
                                         ctx->dev_addr,
                                         tx,
                                         2U + chunk,
                                         pdMS_TO_TICKS(EEPROM_24C64_I2C_TIMEOUT_MS));
        if (err != ESP_OK)
        {
            return err;
        }

        err = eeprom_24c64_ack_poll(ctx);
        if (err != ESP_OK)
        {
            return err;
        }

        offset += chunk;
    }

    return ESP_OK;
}

static void eeprom_24c64_printf(app_command_output_fn output, const char *fmt, ...)
{
    char buf[EEPROM_24C64_CMD_BUF_SIZE];
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

static void eeprom_24c64_respond_error(app_command_output_fn output, const char *fmt, ...)
{
    char detail[EEPROM_24C64_CMD_BUF_SIZE];
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

static int eeprom_24c64_char_to_nibble(char c)
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

static bool eeprom_24c64_parse_u32(const char *text, uint32_t max_value, uint32_t *out_value)
{
    char *endptr = NULL;
    unsigned long parsed;

    if (text == NULL || out_value == NULL || *text == '\0')
    {
        return false;
    }

    errno = 0;
    parsed = strtoul(text, &endptr, 0);
    if (*endptr != '\0' || errno == ERANGE || parsed > (unsigned long)max_value)
    {
        return false;
    }

    *out_value = (uint32_t)parsed;
    return true;
}

static bool eeprom_24c64_parse_hex_bytes(const char *text,
                                         uint8_t *out_bytes,
                                         size_t out_capacity,
                                         size_t *out_len)
{
    const size_t text_len = (text == NULL) ? 0U : strlen(text);
    size_t i;

    if (text == NULL || out_bytes == NULL || out_len == NULL)
    {
        return false;
    }

    if (text_len == 0U || (text_len % 2U) != 0U)
    {
        return false;
    }

    if ((text_len / 2U) > out_capacity)
    {
        return false;
    }

    for (i = 0U; i < text_len; i += 2U)
    {
        const int high = eeprom_24c64_char_to_nibble(text[i]);
        const int low = eeprom_24c64_char_to_nibble(text[i + 1U]);
        if (high < 0 || low < 0)
        {
            return false;
        }
        out_bytes[i / 2U] = (uint8_t)(((unsigned int)high << 4U) | (unsigned int)low);
    }

    *out_len = text_len / 2U;
    return true;
}

static void eeprom_24c64_bytes_to_hex_upper(const uint8_t *data, size_t len, char *out_text)
{
    static const char hex_chars[] = "0123456789ABCDEF";
    size_t i;

    for (i = 0U; i < len; i++)
    {
        out_text[i * 2U] = hex_chars[(data[i] >> 4U) & 0x0FU];
        out_text[(i * 2U) + 1U] = hex_chars[data[i] & 0x0FU];
    }
    out_text[len * 2U] = '\0';
}

static bool eeprom_24c64_is_supported_command(const char *cmd)
{
    return (strcmp(cmd, EEPROM_24C64_CMD_READ) == 0) ||
           (strcmp(cmd, EEPROM_24C64_CMD_WRITE) == 0) ||
           (strcmp(cmd, EEPROM_24C64_CMD_DUMP) == 0);
}

static bool eeprom_24c64_handle_read(const app_command_ctx_t *cmd_ctx,
                                     eeprom_24c64_t *ctx,
                                     char *args)
{
    char *saveptr = NULL;
    char *addr_str = strtok_r(args, " \t", &saveptr);
    char *len_str = strtok_r(NULL, " \t", &saveptr);
    char *extra = strtok_r(NULL, " \t", &saveptr);
    uint32_t addr_u32 = 0U;
    uint32_t len_u32 = 0U;
    size_t read_len;
    uint8_t *buf;
    char *hex_line;
    esp_err_t err;

    if (extra != NULL ||
        !eeprom_24c64_parse_u32(addr_str, EEPROM_24C64_MEM_SIZE_BYTES - 1U, &addr_u32) ||
        !eeprom_24c64_parse_u32(len_str, EEPROM_24C64_MEM_SIZE_BYTES, &len_u32) ||
        (len_u32 == 0U))
    {
        eeprom_24c64_respond_error(cmd_ctx->output, "Invalid arguments");
        return true;
    }

    read_len = (size_t)len_u32;
    if (!eeprom_24c64_range_is_valid((uint16_t)addr_u32, read_len))
    {
        eeprom_24c64_respond_error(cmd_ctx->output, "Range out of bounds");
        return true;
    }

    buf = (uint8_t *)malloc(read_len);
    if (buf == NULL)
    {
        eeprom_24c64_respond_error(cmd_ctx->output, "Out of memory");
        return true;
    }

    hex_line = (char *)malloc((read_len * 2U) + 1U);
    if (hex_line == NULL)
    {
        free(buf);
        eeprom_24c64_respond_error(cmd_ctx->output, "Out of memory");
        return true;
    }

    err = eeprom_24c64_read(ctx, (uint16_t)addr_u32, buf, read_len);
    if (err != ESP_OK)
    {
        free(hex_line);
        free(buf);
        eeprom_24c64_respond_error(cmd_ctx->output, "I2C read failed (%d)", (int)err);
        return true;
    }

    eeprom_24c64_bytes_to_hex_upper(buf, read_len, hex_line);
    app_commands_respond_ok_with_count(cmd_ctx->output, 1U);
    eeprom_24c64_printf(cmd_ctx->output, "%s\r\n", hex_line);

    free(hex_line);
    free(buf);
    return true;
}

static bool eeprom_24c64_handle_write(const app_command_ctx_t *cmd_ctx,
                                      eeprom_24c64_t *ctx,
                                      char *args)
{
    char *saveptr = NULL;
    char *addr_str = strtok_r(args, " \t", &saveptr);
    char *hex_data_str = strtok_r(NULL, " \t", &saveptr);
    char *extra = strtok_r(NULL, " \t", &saveptr);
    uint32_t addr_u32 = 0U;
    uint8_t *data = NULL;
    size_t hex_len = 0U;
    size_t data_len = 0U;
    esp_err_t err;

    if (extra != NULL ||
        !eeprom_24c64_parse_u32(addr_str, EEPROM_24C64_MEM_SIZE_BYTES - 1U, &addr_u32) ||
        hex_data_str == NULL)
    {
        eeprom_24c64_respond_error(cmd_ctx->output, "Invalid arguments");
        return true;
    }

    hex_len = strlen(hex_data_str);
    if (hex_len == 0U || (hex_len % 2U) != 0U)
    {
        eeprom_24c64_respond_error(cmd_ctx->output, "hexData must be even-length valid hex");
        return true;
    }

    data_len = hex_len / 2U;
    if (data_len > EEPROM_24C64_MEM_SIZE_BYTES)
    {
        eeprom_24c64_respond_error(cmd_ctx->output, "Range out of bounds");
        return true;
    }

    data = (uint8_t *)malloc(data_len);
    if (data == NULL)
    {
        eeprom_24c64_respond_error(cmd_ctx->output, "Out of memory");
        return true;
    }

    if (!eeprom_24c64_parse_hex_bytes(hex_data_str, data, data_len, &data_len))
    {
        free(data);
        eeprom_24c64_respond_error(cmd_ctx->output, "hexData must be even-length valid hex");
        return true;
    }

    if (!eeprom_24c64_range_is_valid((uint16_t)addr_u32, data_len))
    {
        free(data);
        eeprom_24c64_respond_error(cmd_ctx->output, "Range out of bounds");
        return true;
    }

    err = eeprom_24c64_write(ctx, (uint16_t)addr_u32, data, data_len);
    free(data);
    if (err != ESP_OK)
    {
        eeprom_24c64_respond_error(cmd_ctx->output, "I2C write failed (%d)", (int)err);
        return true;
    }

    app_commands_respond_ok(cmd_ctx->output);
    return true;
}

static bool eeprom_24c64_handle_dump(const app_command_ctx_t *cmd_ctx,
                                     eeprom_24c64_t *ctx,
                                     char *args)
{
    uint8_t data[EEPROM_24C64_DUMP_SIZE_BYTES];
    char line[(EEPROM_24C64_DUMP_LINE_SIZE_BYTES * 2U) + 1U];
    char *saveptr = NULL;
    char *extra = strtok_r(args, " \t", &saveptr);
    size_t offset;
    esp_err_t err;

    if (extra != NULL)
    {
        eeprom_24c64_respond_error(cmd_ctx->output, "Invalid arguments");
        return true;
    }

    err = eeprom_24c64_read(ctx, 0U, data, sizeof(data));
    if (err != ESP_OK)
    {
        eeprom_24c64_respond_error(cmd_ctx->output, "I2C read failed (%d)", (int)err);
        return true;
    }

    app_commands_respond_ok_with_count(cmd_ctx->output, EEPROM_24C64_DUMP_LINE_COUNT);
    for (offset = 0U; offset < sizeof(data); offset += EEPROM_24C64_DUMP_LINE_SIZE_BYTES)
    {
        eeprom_24c64_bytes_to_hex_upper(&data[offset], EEPROM_24C64_DUMP_LINE_SIZE_BYTES, line);
        eeprom_24c64_printf(cmd_ctx->output, "%s\r\n", line);
    }

    return true;
}

static bool eeprom_24c64_command_handler(const app_command_ctx_t *cmd_ctx,
                                         const char *cmd,
                                         char *args,
                                         void *user_ctx)
{
    eeprom_24c64_t *ctx = (eeprom_24c64_t *)user_ctx;

    if (cmd_ctx == NULL || cmd == NULL)
    {
        return false;
    }

    if (!eeprom_24c64_is_supported_command(cmd))
    {
        return false;
    }

    if (!eeprom_24c64_is_ready(ctx))
    {
        eeprom_24c64_respond_error(cmd_ctx->output, "EEPROM64 not available");
        return true;
    }

    if (strcmp(cmd, EEPROM_24C64_CMD_READ) == 0)
    {
        return eeprom_24c64_handle_read(cmd_ctx, ctx, args);
    }

    if (strcmp(cmd, EEPROM_24C64_CMD_WRITE) == 0)
    {
        return eeprom_24c64_handle_write(cmd_ctx, ctx, args);
    }

    if (strcmp(cmd, EEPROM_24C64_CMD_DUMP) == 0)
    {
        return eeprom_24c64_handle_dump(cmd_ctx, ctx, args);
    }

    return false;
}

void eeprom_24c64_register_commands(eeprom_24c64_t *ctx)
{
    bool registered;

    if (!eeprom_24c64_is_ready(ctx))
    {
        return;
    }

    registered = app_commands_register_custom_handler(eeprom_24c64_command_handler, ctx);
    if (!registered)
    {
        ESP_LOGE(EEPROM_24C64_TAG, "app_commands_register_custom_handler failed");
    }
}
