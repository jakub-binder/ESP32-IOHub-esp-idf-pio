#ifndef APP_COMMAND_TRANSPORT_H
#define APP_COMMAND_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize all command transport ports.
 *
 * Configures and starts two simultaneous command input/output paths:
 *   - Debug port    (UART0): full command set, allow_debug_commands = true.
 *   - Production port (UART1): common commands only, allow_debug_commands = false.
 *
 * Each port has its own RX task, line buffer (endpoint instance), and output
 * callback so that responses are always sent back to the originating port.
 *
 * Call once from app_main(), before the main loop.
 */
void app_command_transport_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMMAND_TRANSPORT_H */
