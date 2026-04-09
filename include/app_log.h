#ifndef APP_LOG_H
#define APP_LOG_H

#include "app_config.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#if APP_ENABLE_APPLICATION_LOGS
#define APP_LOGI(tag, format, ...)      ESP_LOGI(tag, format, ##__VA_ARGS__)
#define APP_LOGW(tag, format, ...)      ESP_LOGW(tag, format, ##__VA_ARGS__)
#define APP_LOGE(tag, format, ...)      ESP_LOGE(tag, format, ##__VA_ARGS__)
#else
#define APP_LOGI(tag, format, ...)      ((void)0)
#define APP_LOGW(tag, format, ...)      ((void)0)
#define APP_LOGE(tag, format, ...)      ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* APP_LOG_H */
