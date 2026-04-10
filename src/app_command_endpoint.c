#include "app_command_endpoint.h"
#include "app_commands.h"

/* Maximum number of characters in one command line (including NUL terminator). */
#define APP_COMMAND_LINE_BUFFER_SIZE    128

/* g_line_buf and g_pos are written exclusively from the transport RX task
 * (single writer).  If app_command_endpoint_on_char() is ever called from
 * an ISR or a second task, a mutex / critical section must be added here. */
static char   g_line_buf[APP_COMMAND_LINE_BUFFER_SIZE];
static int    g_pos = 0;

void app_command_endpoint_on_char(char c)
{
    if (c == '\r' || c == '\n')
    {
        if (g_pos > 0)
        {
            g_line_buf[g_pos] = '\0';
            app_commands_handle_line(g_line_buf);
            g_pos = 0;
        }
        return;
    }

    if (g_pos < APP_COMMAND_LINE_BUFFER_SIZE - 1)
    {
        g_line_buf[g_pos++] = c;
    }
    /* Silently drop characters that overflow the buffer. */
}

void app_command_endpoint_init(app_command_output_fn output_fn)
{
    /* Initialise the command layer with the transport's output callback.
     * Character input is handled by the transport module (app_command_transport),
     * which calls app_command_endpoint_on_char() for each received byte. */
    app_commands_init(output_fn);
}
