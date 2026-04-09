#ifndef APP_SERIAL_ROUTING_H
#define APP_SERIAL_ROUTING_H

#include "app_config.h"
#include "board/board_pins.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    APP_SERIAL_ENDPOINT_UART0 = 0,
    APP_SERIAL_ENDPOINT_UART1,
    APP_SERIAL_ENDPOINT_UART2,
    APP_SERIAL_ENDPOINT_USB_CDC
} app_serial_endpoint_t;

/* ---------------------------
 * Compile-time endpoint mapping
 * --------------------------- */
#if defined(APP_MODE_DEBUG)
#if BOARD_HAS_USB_CDC
#define APP_SERIAL_COMMAND_ENDPOINT      APP_SERIAL_ENDPOINT_USB_CDC
#else
#define APP_SERIAL_COMMAND_ENDPOINT      APP_SERIAL_ENDPOINT_UART0
#endif
#elif defined(APP_MODE_PROD)
#define APP_SERIAL_COMMAND_ENDPOINT      APP_SERIAL_ENDPOINT_UART1
#else
#error "Application mode is not selected. Define APP_MODE_DEBUG or APP_MODE_PROD."
#endif

static inline const char *app_serial_endpoint_to_string(app_serial_endpoint_t endpoint)
{
    switch (endpoint)
    {
        case APP_SERIAL_ENDPOINT_UART0:
            return "UART0";
        case APP_SERIAL_ENDPOINT_UART1:
            return "UART1";
        case APP_SERIAL_ENDPOINT_UART2:
            return "UART2";
        case APP_SERIAL_ENDPOINT_USB_CDC:
            return "USB_CDC";
        default:
            return "UNKNOWN";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* APP_SERIAL_ROUTING_H */
