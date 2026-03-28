#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------
 * Application defaults
 * --------------------------- */

#define APP_MAIN_LOOP_DELAY_MS        10
#define APP_LOG_TAG                   "APP"
#define APP_FIXTURE_LOG_TAG           "FIXTURE"

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