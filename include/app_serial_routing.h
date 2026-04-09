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
#if (APP_COMMAND_ENDPOINT == APP_COMMAND_ENDPOINT_UART0)
#define APP_SERIAL_COMMAND_ENDPOINT      APP_SERIAL_ENDPOINT_UART0
#elif (APP_COMMAND_ENDPOINT == APP_COMMAND_ENDPOINT_UART1)
#define APP_SERIAL_COMMAND_ENDPOINT      APP_SERIAL_ENDPOINT_UART1
#elif (APP_COMMAND_ENDPOINT == APP_COMMAND_ENDPOINT_UART2)
#define APP_SERIAL_COMMAND_ENDPOINT      APP_SERIAL_ENDPOINT_UART2
#elif (APP_COMMAND_ENDPOINT == APP_COMMAND_ENDPOINT_USB_CDC)
#define APP_SERIAL_COMMAND_ENDPOINT      APP_SERIAL_ENDPOINT_USB_CDC
#else
#error "Invalid APP_COMMAND_ENDPOINT value"
#endif

/* ---------------------------
 * Compile-time endpoint validation
 * --------------------------- */
#if (APP_COMMAND_ENDPOINT == APP_COMMAND_ENDPOINT_UART1) && !BOARD_HAS_UART1
#error "APP_COMMAND_ENDPOINT=UART1 is not supported by selected board"
#endif

#if (APP_COMMAND_ENDPOINT == APP_COMMAND_ENDPOINT_UART2) && !BOARD_HAS_UART2
#error "APP_COMMAND_ENDPOINT=UART2 is not supported by selected board"
#endif

#if (APP_COMMAND_ENDPOINT == APP_COMMAND_ENDPOINT_USB_CDC) && !BOARD_HAS_USB_CDC
#error "APP_COMMAND_ENDPOINT=USB_CDC is not supported by selected board"
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
