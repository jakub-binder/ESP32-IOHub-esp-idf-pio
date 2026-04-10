#include "app_commands.h"
#include "app_command_system.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define APP_COMMANDS_LOG_TAG    "CMD"
#define APP_COMMANDS_BUF_SIZE   128

static app_command_output_fn g_output = NULL;

/* Send a formatted string to the active command output channel. */
static void app_commands_printf(const char *fmt, ...)
{
    char buf[APP_COMMANDS_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (g_output != NULL)
    {
        g_output(buf);
    }
}

void app_commands_init(app_command_output_fn output_cb)
{
    g_output = output_cb;
}

void app_commands_handle_line(const char *line)
{
    char buf[APP_COMMANDS_BUF_SIZE];
    const char *cmd;

    if (line == NULL || line[0] == '\0')
    {
        return;
    }

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

    /* Dispatch – direct strcmp chain; extend here as new commands are added. */
    if (strcmp(cmd, "firmware") == 0)
    {
        app_command_system_firmware(g_output);
    }
    else if (strcmp(cmd, "restart") == 0)
    {
        app_command_system_restart(g_output);
    }
    else if (strcmp(cmd, "init") == 0)
    {
        app_command_system_init(g_output);
    }
    else if (strcmp(cmd, "info") == 0)
    {
        app_command_system_info(g_output);
    }
    else
    {
        app_commands_printf("ERR unknown command: %s\r\n", cmd);
    }
}
