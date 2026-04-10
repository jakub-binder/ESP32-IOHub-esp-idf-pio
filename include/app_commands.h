#ifndef APP_COMMANDS_H
#define APP_COMMANDS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Callback type used to send command responses to the active channel. */
typedef void (*app_command_output_fn)(const char *text);

/**
 * Initialize the command layer.
 * Must be called before app_commands_handle_line().
 * @param output_cb  Function used to write command responses; may be NULL.
 */
void app_commands_init(app_command_output_fn output_cb);

/**
 * Process one complete text line (without trailing newline / carriage-return).
 * Parses the first whitespace-delimited token as the command name and
 * dispatches to the matching handler.
 */
void app_commands_handle_line(const char *line);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMMANDS_H */
