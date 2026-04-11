#ifndef GPIO_DEBUG_H
#define GPIO_DEBUG_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gpio_debug gpio_debug_t;

typedef enum
{
    GPIO_DEBUG_PULL_FLOATING = 0,
    GPIO_DEBUG_PULL_UP,
    GPIO_DEBUG_PULL_DOWN,
} gpio_debug_pull_t;

typedef enum
{
    GPIO_DEBUG_LEVEL_LOW = 0,
    GPIO_DEBUG_LEVEL_HIGH = 1,
} gpio_debug_level_t;

typedef struct
{
    uint64_t allowed_mask;
    uint64_t protected_mask;
} gpio_debug_policy_t;

typedef struct
{
    gpio_debug_policy_t policy;
} gpio_debug_config_t;

struct gpio_debug
{
    gpio_debug_config_t config;
    bool initialized;
};

esp_err_t gpio_debug_init(gpio_debug_t *self, const gpio_debug_config_t *cfg);

esp_err_t gpio_debug_set_input(gpio_debug_t *self, int pin, gpio_debug_pull_t pull);
esp_err_t gpio_debug_set_output(gpio_debug_t *self, int pin, gpio_debug_level_t level);
esp_err_t gpio_debug_get(gpio_debug_t *self, int pin, int *out_level);

esp_err_t gpio_debug_register_commands(gpio_debug_t *self);
gpio_debug_t *gpio_debug_get_registered(void);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_DEBUG_H */
