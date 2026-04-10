#ifndef APP_COMMAND_ENDPOINT_H
#define APP_COMMAND_ENDPOINT_H

#include "app_commands.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the active command endpoint.
 * Passes output_fn to the command layer and starts an internal FreeRTOS
 * task that reads characters from the console (getchar) and feeds them
 * into app_command_endpoint_on_char().
 *
 * Call once from app_main(), after fixture_setup().
 *
 * @param output_fn  Function used to write command responses to the channel.
 */
void app_command_endpoint_init(app_command_output_fn output_fn);

/**
 * Feed one received character into the endpoint line buffer.
 * On '\n' or '\r', the accumulated line is dispatched to app_commands_handle_line().
 *
 * Intended to be called by a low-level RX driver (UART ISR, VFS task, etc.).
 * Do NOT call this from fixture code.
 */
void app_command_endpoint_on_char(char c);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMMAND_ENDPOINT_H */
