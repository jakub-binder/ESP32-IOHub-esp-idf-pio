#ifndef APP_COMMAND_ENDPOINT_H
#define APP_COMMAND_ENDPOINT_H

#include "app_commands.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of characters in one command line (including NUL terminator). */
#define APP_COMMAND_LINE_BUFFER_SIZE    128

/**
 * Per-port endpoint state.
 * Each active command port (e.g. UART0, UART1) owns one instance.
 * Initialize with app_command_endpoint_init_ex() before use.
 */
typedef struct
{
    char              line_buf[APP_COMMAND_LINE_BUFFER_SIZE];
    int               pos;
    app_command_ctx_t ctx;
} app_command_endpoint_t;

/**
 * Initialize a command endpoint instance.
 * Sets the context (source, output callback, debug-command gate) that will
 * be forwarded to app_commands_handle_line_ctx() on each complete line.
 *
 * @param ep   Pointer to an endpoint instance (caller-allocated).
 * @param ctx  Context to associate with this endpoint.
 */
void app_command_endpoint_init_ex(app_command_endpoint_t *ep,
                                  app_command_ctx_t ctx);

/**
 * Feed one received character into the endpoint line buffer.
 * On '\n' or '\r', the accumulated line is dispatched via
 * app_commands_handle_line_ctx() using the context stored in ep->ctx.
 *
 * Call this from the low-level RX driver (UART task, etc.).
 * Do NOT call this from fixture code.
 *
 * @param ep  Pointer to the endpoint instance for this port.
 * @param c   Received character.
 */
void app_command_endpoint_on_char(app_command_endpoint_t *ep, char c);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMMAND_ENDPOINT_H */
