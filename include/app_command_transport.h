#ifndef APP_COMMAND_TRANSPORT_H
#define APP_COMMAND_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the command transport for the active APP_COMMAND_ENDPOINT.
 *
 * Configures the selected peripheral (UART or USB CDC), starts a FreeRTOS
 * task that reads incoming bytes and feeds each byte into
 * app_command_endpoint_on_char(), and makes app_command_transport_write()
 * available for command output.
 *
 * Call once from app_main(), before the main loop.
 */
void app_command_transport_init(void);

/**
 * Write a null-terminated string to the active command transport.
 *
 * Pass this function as the output_fn argument to
 * app_command_endpoint_init().
 *
 * @param text  Null-terminated string to send; may be NULL (no-op).
 */
void app_command_transport_write(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMMAND_TRANSPORT_H */
