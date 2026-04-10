#include "app_command_endpoint.h"
#include "app_commands.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>

/* Maximum number of characters in one command line (including NUL terminator). */
#define APP_COMMAND_LINE_BUFFER_SIZE    128

/* g_line_buf and g_pos are accessed exclusively by rx_task (single FreeRTOS
 * task).  If app_command_endpoint_on_char() is later called from an ISR or
 * a different task, a mutex / critical section must be added here. */
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

/* Internal FreeRTOS task: reads characters from the console (getchar)
 * and feeds them into the endpoint.  Replaces the explicit per-character
 * RX callback that the low-level driver would provide once a full UART
 * driver is wired in.
 *
 * TODO: replace with a proper UART/USB-CDC RX driver callback when available.
 */
static void rx_task(void *arg)
{
    int c;
    (void)arg;

    while (1)
    {
        c = getchar();
        if (c >= 0)
        {
            app_command_endpoint_on_char((char)c);
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void app_command_endpoint_init(app_command_output_fn output_fn)
{
    app_commands_init(output_fn);
    xTaskCreate(rx_task, "cmd_rx", 2048, NULL, 5, NULL);
}
