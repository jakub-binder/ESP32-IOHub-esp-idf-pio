#include "app_commands.h"
#include "app_command_system.h"

#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define APP_COMMANDS_BUF_SIZE   128
#define APP_COMMANDS_MAX_CUSTOM_HANDLERS 8

/* Legacy single-output fallback (used only by app_commands_handle_line). */
static app_command_output_fn g_output = NULL;

typedef struct
{
    app_command_custom_handler_fn handler;
    void *user_ctx;
} app_commands_custom_handler_entry_t;

static app_commands_custom_handler_entry_t
    g_custom_handlers[APP_COMMANDS_MAX_CUSTOM_HANDLERS];
static size_t g_custom_handler_count = 0;

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

bool app_commands_register_custom_handler(app_command_custom_handler_fn handler,
                                          void *user_ctx)
{
    size_t i;

    if (handler == NULL)
    {
        return false;
    }

    for (i = 0; i < g_custom_handler_count; i++)
    {
        if (g_custom_handlers[i].handler == handler &&
            g_custom_handlers[i].user_ctx == user_ctx)
        {
            return true;
        }
    }

    if (g_custom_handler_count >= APP_COMMANDS_MAX_CUSTOM_HANDLERS)
    {
        return false;
    }

    g_custom_handlers[g_custom_handler_count].handler = handler;
    g_custom_handlers[g_custom_handler_count].user_ctx = user_ctx;
    g_custom_handler_count++;

    return true;
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
        app_commands_printf_to(output, "debug: help, ports, debug\r\n");
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
    else
    {
        size_t i;
        for (i = 0; i < g_custom_handler_count; i++)
        {
            char args_buf[APP_COMMANDS_BUF_SIZE];
            char *args;
            bool handled;

            if (saveptr != NULL)
            {
                strncpy(args_buf, saveptr, sizeof(args_buf) - 1);
                args_buf[sizeof(args_buf) - 1] = '\0';
            }
            else
            {
                args_buf[0] = '\0';
            }

            args = args_buf;
            handled = g_custom_handlers[i].handler(ctx, cmd, args,
                                                   g_custom_handlers[i].user_ctx);
            if (handled)
            {
                return;
            }
        }
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
