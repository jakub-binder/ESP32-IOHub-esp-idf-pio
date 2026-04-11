#include "gpio_debug.h"

#include "app_commands.h"
#include "driver/gpio.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GPIO_DEBUG_CMD_BUF_SIZE 96

static bool gpio_debug_pin_is_mask_valid(int pin)
{
    return (pin >= 0) && (pin < 64);
}

static bool gpio_debug_policy_allows_pin(const gpio_debug_t *self, int pin)
{
    const uint64_t bit = (1ULL << (uint32_t)pin);
    return (self->config.policy.allowed_mask & bit) != 0;
}

static bool gpio_debug_policy_protects_pin(const gpio_debug_t *self, int pin)
{
    const uint64_t bit = (1ULL << (uint32_t)pin);
    return (self->config.policy.protected_mask & bit) != 0;
}

static esp_err_t gpio_debug_validate_common(const gpio_debug_t *self, int pin)
{
    if (self == NULL || !self->initialized)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!gpio_debug_pin_is_mask_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!GPIO_IS_VALID_GPIO(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!gpio_debug_policy_allows_pin(self, pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t gpio_debug_init(gpio_debug_t *self, const gpio_debug_config_t *cfg)
{
    if (self == NULL || cfg == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    self->config = *cfg;
    self->initialized = true;

    return ESP_OK;
}

uint64_t gpio_debug_pin_to_mask(int pin)
{
    if (!gpio_debug_pin_is_mask_valid(pin))
    {
        return 0;
    }

    return (1ULL << (uint32_t)pin);
}

esp_err_t gpio_debug_set_input(gpio_debug_t *self, int pin, gpio_debug_pull_t pull)
{
    gpio_config_t io_cfg;
    gpio_pull_mode_t pull_mode;
    esp_err_t err;

    err = gpio_debug_validate_common(self, pin);
    if (err != ESP_OK)
    {
        return err;
    }

    if (gpio_debug_policy_protects_pin(self, pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    io_cfg.pin_bit_mask = gpio_debug_pin_to_mask(pin);
    io_cfg.mode = GPIO_MODE_INPUT;
    io_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    io_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_cfg.intr_type = GPIO_INTR_DISABLE;
    err = gpio_config(&io_cfg);
    if (err != ESP_OK)
    {
        return err;
    }

    switch (pull)
    {
        case GPIO_DEBUG_PULL_FLOATING:
            pull_mode = GPIO_FLOATING;
            break;
        case GPIO_DEBUG_PULL_UP:
            pull_mode = GPIO_PULLUP_ONLY;
            break;
        case GPIO_DEBUG_PULL_DOWN:
            pull_mode = GPIO_PULLDOWN_ONLY;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    return gpio_set_pull_mode((gpio_num_t)pin, pull_mode);
}

esp_err_t gpio_debug_set_output(gpio_debug_t *self, int pin, gpio_debug_level_t level)
{
    gpio_config_t io_cfg;
    esp_err_t err;

    err = gpio_debug_validate_common(self, pin);
    if (err != ESP_OK)
    {
        return err;
    }

    if (gpio_debug_policy_protects_pin(self, pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (level != GPIO_DEBUG_LEVEL_LOW && level != GPIO_DEBUG_LEVEL_HIGH)
    {
        return ESP_ERR_INVALID_ARG;
    }

    io_cfg.pin_bit_mask = gpio_debug_pin_to_mask(pin);
    io_cfg.mode = GPIO_MODE_OUTPUT;
    io_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    io_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_cfg.intr_type = GPIO_INTR_DISABLE;
    err = gpio_config(&io_cfg);
    if (err != ESP_OK)
    {
        return err;
    }

    return gpio_set_level((gpio_num_t)pin, (level == GPIO_DEBUG_LEVEL_HIGH) ? 1 : 0);
}

esp_err_t gpio_debug_get(gpio_debug_t *self, int pin, int *out_level)
{
    esp_err_t err;

    if (out_level == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    err = gpio_debug_validate_common(self, pin);
    if (err != ESP_OK)
    {
        return err;
    }

    *out_level = gpio_get_level((gpio_num_t)pin);

    return ESP_OK;
}

static void gpio_debug_printf(app_command_output_fn output, const char *fmt, ...)
{
    char buf[GPIO_DEBUG_CMD_BUF_SIZE];
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

static bool gpio_debug_parse_int(const char *text, int *out_value)
{
    char *endptr;
    long parsed;

    if (text == NULL || out_value == NULL || *text == '\0')
    {
        return false;
    }

    parsed = strtol(text, &endptr, 10);
    if (*endptr != '\0')
    {
        return false;
    }

    *out_value = (int)parsed;
    return true;
}

static bool gpio_debug_handle_get(const app_command_ctx_t *ctx,
                                  gpio_debug_t *self,
                                  char *args)
{
    char *saveptr = NULL;
    char *pin_str = strtok_r(args, " \t", &saveptr);
    int pin;
    int level;
    esp_err_t err;

    if (!gpio_debug_parse_int(pin_str, &pin))
    {
        gpio_debug_printf(ctx->output, "ERR usage: gpio.get <pin>\r\n");
        return true;
    }

    err = gpio_debug_get(self, pin, &level);
    if (err != ESP_OK)
    {
        gpio_debug_printf(ctx->output, "ERR %d\r\n", (int)err);
        return true;
    }

    gpio_debug_printf(ctx->output, "GPIO %d = %d\r\n", pin, level);
    return true;
}

static bool gpio_debug_handle_in(const app_command_ctx_t *ctx,
                                 gpio_debug_t *self,
                                 char *args)
{
    char *saveptr = NULL;
    char *pin_str = strtok_r(args, " \t", &saveptr);
    char *pull_str = strtok_r(NULL, " \t", &saveptr);
    int pin;
    gpio_debug_pull_t pull;
    esp_err_t err;

    if (!gpio_debug_parse_int(pin_str, &pin) || pull_str == NULL)
    {
        gpio_debug_printf(ctx->output, "ERR usage: gpio.in <pin> <none|up|down>\r\n");
        return true;
    }

    if (strcmp(pull_str, "none") == 0)
    {
        pull = GPIO_DEBUG_PULL_FLOATING;
    }
    else if (strcmp(pull_str, "up") == 0)
    {
        pull = GPIO_DEBUG_PULL_UP;
    }
    else if (strcmp(pull_str, "down") == 0)
    {
        pull = GPIO_DEBUG_PULL_DOWN;
    }
    else
    {
        gpio_debug_printf(ctx->output, "ERR usage: gpio.in <pin> <none|up|down>\r\n");
        return true;
    }

    err = gpio_debug_set_input(self, pin, pull);
    if (err != ESP_OK)
    {
        gpio_debug_printf(ctx->output, "ERR %d\r\n", (int)err);
        return true;
    }

    gpio_debug_printf(ctx->output, "OK\r\n");
    return true;
}

static bool gpio_debug_handle_out(const app_command_ctx_t *ctx,
                                  gpio_debug_t *self,
                                  char *args)
{
    char *saveptr = NULL;
    char *pin_str = strtok_r(args, " \t", &saveptr);
    char *level_str = strtok_r(NULL, " \t", &saveptr);
    int pin;
    int level_raw;
    gpio_debug_level_t level;
    esp_err_t err;

    if (!gpio_debug_parse_int(pin_str, &pin) ||
        !gpio_debug_parse_int(level_str, &level_raw))
    {
        gpio_debug_printf(ctx->output, "ERR usage: gpio.out <pin> <0|1>\r\n");
        return true;
    }

    if (level_raw == 0)
    {
        level = GPIO_DEBUG_LEVEL_LOW;
    }
    else if (level_raw == 1)
    {
        level = GPIO_DEBUG_LEVEL_HIGH;
    }
    else
    {
        gpio_debug_printf(ctx->output, "ERR usage: gpio.out <pin> <0|1>\r\n");
        return true;
    }

    err = gpio_debug_set_output(self, pin, level);
    if (err != ESP_OK)
    {
        gpio_debug_printf(ctx->output, "ERR %d\r\n", (int)err);
        return true;
    }

    gpio_debug_printf(ctx->output, "OK\r\n");
    return true;
}

static bool gpio_debug_command_handler(const app_command_ctx_t *ctx,
                                       const char *cmd,
                                       char *args,
                                       void *user_ctx)
{
    gpio_debug_t *self = (gpio_debug_t *)user_ctx;

    if (ctx == NULL || cmd == NULL)
    {
        return false;
    }

    if (strcmp(cmd, "gpio.get") != 0 &&
        strcmp(cmd, "gpio.in") != 0 &&
        strcmp(cmd, "gpio.out") != 0)
    {
        return false;
    }

    if (!ctx->allow_debug_commands)
    {
        gpio_debug_printf(ctx->output, "ERR not allowed\r\n");
        return true;
    }

    if (self == NULL || !self->initialized)
    {
        gpio_debug_printf(ctx->output, "ERR not available\r\n");
        return true;
    }

    if (strcmp(cmd, "gpio.get") == 0)
    {
        return gpio_debug_handle_get(ctx, self, args);
    }

    if (strcmp(cmd, "gpio.in") == 0)
    {
        return gpio_debug_handle_in(ctx, self, args);
    }

    return gpio_debug_handle_out(ctx, self, args);
}

esp_err_t gpio_debug_register_commands(gpio_debug_t *self)
{
    bool registered;

    if (self == NULL || !self->initialized)
    {
        return ESP_ERR_INVALID_ARG;
    }

    registered = app_commands_register_custom_handler(gpio_debug_command_handler, self);
    if (!registered)
    {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
