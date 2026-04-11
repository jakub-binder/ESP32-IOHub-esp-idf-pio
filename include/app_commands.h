#ifndef APP_COMMANDS_H
#define APP_COMMANDS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Callback type used to send command responses to the active channel. */
typedef void (*app_command_output_fn)(const char *text);

/* -----------------------------------------------------------------------
 * Command source – identifies which port a command arrived from.
 * ----------------------------------------------------------------------- */
typedef enum
{
    APP_CMD_SOURCE_UART0 = 0,
    APP_CMD_SOURCE_UART1,
    APP_CMD_SOURCE_USB_CDC,
} app_command_source_t;

/* -----------------------------------------------------------------------
 * Command context – passed to the dispatcher so that responses go back to
 * the correct port and debug-only commands can be gated by a single flag.
 * ----------------------------------------------------------------------- */
typedef struct
{
    app_command_source_t  source;
    app_command_output_fn output;
    bool                  allow_debug_commands;
} app_command_ctx_t;

typedef bool (*app_command_custom_handler_fn)(const app_command_ctx_t *ctx,
                                              const char *cmd,
                                              char *args,
                                              void *user_ctx);

/**
 * Process one complete text line using a full per-port context.
 * The context carries the output callback (response routing) and the
 * debug-command gate (allow_debug_commands).
 * This is the primary API; call from the endpoint layer.
 */
void app_commands_handle_line_ctx(const app_command_ctx_t *ctx, const char *line);

/**
 * Initialize the command layer (legacy single-output mode).
 * @param output_cb  Function used to write command responses; may be NULL.
 * @deprecated  Use app_commands_handle_line_ctx() with a per-port context.
 */
void app_commands_init(app_command_output_fn output_cb);

/**
 * Process one complete text line (legacy single-output mode).
 * Wraps app_commands_handle_line_ctx() using the output set by
 * app_commands_init().
 * @deprecated  Use app_commands_handle_line_ctx() directly.
 */
void app_commands_handle_line(const char *line);

bool app_commands_register_custom_handler(app_command_custom_handler_fn handler,
                                          void *user_ctx);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMMANDS_H */
