#include "app_command_endpoint.h"
#include "app_commands.h"

#include <string.h>

void app_command_endpoint_init_ex(app_command_endpoint_t *ep,
                                  app_command_ctx_t ctx)
{
    if (ep == NULL)
    {
        return;
    }
    memset(ep->line_buf, 0, sizeof(ep->line_buf));
    ep->pos = 0;
    ep->ctx = ctx;
}

void app_command_endpoint_on_char(app_command_endpoint_t *ep, char c)
{
    if (ep == NULL)
    {
        return;
    }

    if (c == '\r' || c == '\n')
    {
        if (ep->pos > 0)
        {
            ep->line_buf[ep->pos] = '\0';
            app_commands_handle_line_ctx(&ep->ctx, ep->line_buf);
            ep->pos = 0;
        }
        return;
    }

    if (ep->pos < APP_COMMAND_LINE_BUFFER_SIZE - 1)
    {
        ep->line_buf[ep->pos++] = c;
    }
    /* Silently drop characters that overflow the buffer. */
}
