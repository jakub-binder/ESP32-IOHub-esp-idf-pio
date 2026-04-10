#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------
 * Application defaults
 * --------------------------- */

#ifndef APP_MAIN_LOOP_DELAY_MS
#define APP_MAIN_LOOP_DELAY_MS            10
#endif

#ifndef APP_LOG_TAG
#define APP_LOG_TAG                       "APP"
#endif

#ifndef APP_FIXTURE_LOG_TAG
#define APP_FIXTURE_LOG_TAG               "FIXTURE"
#endif

#ifndef APP_ENABLE_APPLICATION_LOGS
#define APP_ENABLE_APPLICATION_LOGS       1
#endif

#define APP_COMMAND_ENDPOINT_UART0        0
#define APP_COMMAND_ENDPOINT_UART1        1
#define APP_COMMAND_ENDPOINT_UART2        2
#define APP_COMMAND_ENDPOINT_USB_CDC      3

#ifndef APP_COMMAND_ENDPOINT
#if defined(APP_MODE_DEBUG)
#if defined(BOARD_ESP32S3)
#define APP_COMMAND_ENDPOINT              APP_COMMAND_ENDPOINT_USB_CDC
#else
#define APP_COMMAND_ENDPOINT              APP_COMMAND_ENDPOINT_UART0
#endif
#elif defined(APP_MODE_PROD)
#define APP_COMMAND_ENDPOINT              APP_COMMAND_ENDPOINT_UART1
#else
#error "Application mode is not selected. Define APP_MODE_DEBUG or APP_MODE_PROD."
#endif
#endif

/* ---------------------------
 * Firmware identification
 * --------------------------- */

#ifndef APP_FIRMWARE_ID
#define APP_FIRMWARE_ID                   "ESP32-IOHub " FW_BOARD_NAME
#endif

/* ---------------------------
 * Build-time mode validation
 * --------------------------- */

#if defined(APP_MODE_DEBUG) && defined(APP_MODE_PROD)
#error "Only one of APP_MODE_DEBUG or APP_MODE_PROD can be defined"
#endif

#if !defined(APP_MODE_DEBUG) && !defined(APP_MODE_PROD)
#error "One of APP_MODE_DEBUG or APP_MODE_PROD must be defined"
#endif

/* ---------------------------
 * Fixture validation
 * --------------------------- */

#if defined(FIXTURE_DEFAULT) && defined(FIXTURE_PROD)
#error "Only one fixture can be selected"
#endif

#if !defined(FIXTURE_DEFAULT) && !defined(FIXTURE_PROD)
#error "One fixture must be selected"
#endif

/* ---------------------------
 * Board validation
 * --------------------------- */

#if (defined(BOARD_ESP32S3) + defined(BOARD_ESP32WROOM32) + defined(BOARD_ESP32DEV_16MB)) > 1
#error "Only one board can be selected"
#endif

#if !defined(BOARD_ESP32S3) && !defined(BOARD_ESP32WROOM32) && !defined(BOARD_ESP32DEV_16MB)
#error "One board must be selected"
#endif

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H */
