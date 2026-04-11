#include "app_commands.h"
#include "app_command_system.h"
#include "gpio_debug.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define APP_COMMANDS_BUF_SIZE   128

/* Legacy single-output fallback (used only by app_commands_handle_line). */
static app_command_output_fn g_output = NULL;

static bool app_commands_parse_int(const char *text, int *out_value)
{
    char *endptr;
    long parsed;

    if (text == NULL || out_value == NULL)
    {
        return false;
    }

    parsed = strtol(text, &endptr, 10);
    if (*text == '\0' || *endptr != '\0')
    {
        return false;
    }

    *out_value = (int)parsed;
    return true;
}

/* Send a formatted string to a specific command output channel. */
static void app_commands_printf_to(app_command_output_fn output,
                                   const char *fmt, ...)
{
    char buf[APP_COMMANDS_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (output != NULL)
    {
        output(buf);
    }
}

void app_commands_init(app_command_output_fn output_cb)
{
    g_output = output_cb;
}

void app_commands_handle_line_ctx(const app_command_ctx_t *ctx, const char *line)
{
    char buf[APP_COMMANDS_BUF_SIZE];
    const char *cmd;
    app_command_output_fn output;

    if (ctx == NULL || line == NULL || line[0] == '\0')
    {
        return;
    }

    output = ctx->output;

    /* Copy to mutable buffer for strtok_r; ignore overlong lines. */
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* First whitespace-delimited token is the command name. */
    char *saveptr = NULL;
    cmd = strtok_r(buf, " \t", &saveptr);
    if (cmd == NULL)
    {
        return;
    }

    /* --- Common commands (available on all ports) --- */
    if (strcmp(cmd, "firmware") == 0)
    {
        app_command_system_firmware(output);
    }
    else if (strcmp(cmd, "restart") == 0)
    {
        app_command_system_restart(output);
    }
    else if (strcmp(cmd, "init") == 0)
    {
        app_command_system_init(output);
    }
    else if (strcmp(cmd, "info") == 0)
    {
        app_command_system_info(output);
    }
    /* --- Debug-only commands (available only on debug port) --- */
    else if (strcmp(cmd, "help") == 0)
    {
        if (!ctx->allow_debug_commands)
        {
            app_commands_printf_to(output, "ERR not allowed\r\n");
            return;
        }
        app_commands_printf_to(output, "common: firmware, restart, init, info\r\n");
        app_commands_printf_to(output, "debug: help, ports, debug, gpio.get, gpio.in, gpio.out\r\n");
    }
    else if (strcmp(cmd, "ports") == 0)
    {
        if (!ctx->allow_debug_commands)
        {
            app_commands_printf_to(output, "ERR not allowed\r\n");
            return;
        }
        app_commands_printf_to(output, "debug_port=UART0\r\n");
        app_commands_printf_to(output, "production_port=UART1\r\n");
        app_commands_printf_to(output, "allow_debug_commands=%d\r\n",
                               ctx->allow_debug_commands ? 1 : 0);
    }
    else if (strcmp(cmd, "debug") == 0)
    {
        if (!ctx->allow_debug_commands)
        {
            app_commands_printf_to(output, "ERR not allowed\r\n");
            return;
        }
        app_commands_printf_to(output, "DEBUG OK\r\n");
        app_commands_printf_to(output, "source=%d\r\n", (int)ctx->source);
    }
    else if (strcmp(cmd, "gpio.get") == 0)
    {
        gpio_debug_t *gpio_debug;
        char *pin_str;
        int pin;
        int level;
        esp_err_t err;

        if (!ctx->allow_debug_commands)
        {
            app_commands_printf_to(output, "ERR not allowed\r\n");
            return;
        }

        gpio_debug = gpio_debug_get_registered();
        if (gpio_debug == NULL)
        {
            app_commands_printf_to(output, "ERR not available\r\n");
            return;
        }

        pin_str = strtok_r(NULL, " \t", &saveptr);
        if (!app_commands_parse_int(pin_str, &pin))
        {
            app_commands_printf_to(output, "ERR usage: gpio.get <pin>\r\n");
            return;
        }

        err = gpio_debug_get(gpio_debug, pin, &level);
        if (err != ESP_OK)
        {
            app_commands_printf_to(output, "ERR %d\r\n", (int)err);
            return;
        }

        app_commands_printf_to(output, "GPIO %d = %d\r\n", pin, level);
    }
    else if (strcmp(cmd, "gpio.in") == 0)
    {
        gpio_debug_t *gpio_debug;
        char *pin_str;
        char *pull_str;
        int pin;
        gpio_debug_pull_t pull;
        esp_err_t err;

        if (!ctx->allow_debug_commands)
        {
            app_commands_printf_to(output, "ERR not allowed\r\n");
            return;
        }

        gpio_debug = gpio_debug_get_registered();
        if (gpio_debug == NULL)
        {
            app_commands_printf_to(output, "ERR not available\r\n");
            return;
        }

        pin_str = strtok_r(NULL, " \t", &saveptr);
        pull_str = strtok_r(NULL, " \t", &saveptr);

        if (!app_commands_parse_int(pin_str, &pin) || pull_str == NULL)
        {
            app_commands_printf_to(output, "ERR usage: gpio.in <pin> <none|up|down>\r\n");
            return;
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
            app_commands_printf_to(output, "ERR usage: gpio.in <pin> <none|up|down>\r\n");
            return;
        }

        err = gpio_debug_set_input(gpio_debug, pin, pull);
        if (err != ESP_OK)
        {
            app_commands_printf_to(output, "ERR %d\r\n", (int)err);
            return;
        }

        app_commands_printf_to(output, "OK\r\n");
    }
    else if (strcmp(cmd, "gpio.out") == 0)
    {
        gpio_debug_t *gpio_debug;
        char *pin_str;
        char *level_str;
        int pin;
        int level_raw;
        gpio_debug_level_t level;
        esp_err_t err;

        if (!ctx->allow_debug_commands)
        {
            app_commands_printf_to(output, "ERR not allowed\r\n");
            return;
        }

        gpio_debug = gpio_debug_get_registered();
        if (gpio_debug == NULL)
        {
            app_commands_printf_to(output, "ERR not available\r\n");
            return;
        }

        pin_str = strtok_r(NULL, " \t", &saveptr);
        level_str = strtok_r(NULL, " \t", &saveptr);

        if (!app_commands_parse_int(pin_str, &pin) || !app_commands_parse_int(level_str, &level_raw))
        {
            app_commands_printf_to(output, "ERR usage: gpio.out <pin> <0|1>\r\n");
            return;
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
            app_commands_printf_to(output, "ERR usage: gpio.out <pin> <0|1>\r\n");
            return;
        }

        err = gpio_debug_set_output(gpio_debug, pin, level);
        if (err != ESP_OK)
        {
            app_commands_printf_to(output, "ERR %d\r\n", (int)err);
            return;
        }

        app_commands_printf_to(output, "OK\r\n");
    }
    else
    {
        app_commands_printf_to(output, "ERR unknown command: %s\r\n", cmd);
    }
}

void app_commands_handle_line(const char *line)
{
    /* Legacy wrapper: build a context from the global output function. */
    const app_command_ctx_t ctx = {
        .source               = APP_CMD_SOURCE_UART0,
        .output               = g_output,
        .allow_debug_commands = true,
    };
    app_commands_handle_line_ctx(&ctx, line);
}
