#include "gpio_debug.h"

#include "driver/gpio.h"

static gpio_debug_t *s_registered_gpio_debug = NULL;

static bool gpio_debug_pin_is_mask_valid(int pin)
{
    return (pin >= 0) && (pin < 64);
}

static bool gpio_debug_policy_allows_pin(const gpio_debug_t *self, int pin)
{
    const uint64_t bit = (1ULL << (uint32_t)pin);
    return (self->config.policy.allowed_mask & bit) != 0;
}

static bool gpio_debug_policy_protects_pin(const gpio_debug_t *self, int pin)
{
    const uint64_t bit = (1ULL << (uint32_t)pin);
    return (self->config.policy.protected_mask & bit) != 0;
}

static esp_err_t gpio_debug_validate_common(const gpio_debug_t *self, int pin)
{
    if (self == NULL || !self->initialized)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!gpio_debug_pin_is_mask_valid(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!GPIO_IS_VALID_GPIO(pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!gpio_debug_policy_allows_pin(self, pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t gpio_debug_init(gpio_debug_t *self, const gpio_debug_config_t *cfg)
{
    if (self == NULL || cfg == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    self->config = *cfg;
    self->initialized = true;

    return ESP_OK;
}

esp_err_t gpio_debug_set_input(gpio_debug_t *self, int pin, gpio_debug_pull_t pull)
{
    gpio_config_t io_cfg;
    gpio_pull_mode_t pull_mode;
    esp_err_t err;

    err = gpio_debug_validate_common(self, pin);
    if (err != ESP_OK)
    {
        return err;
    }

    if (gpio_debug_policy_protects_pin(self, pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    io_cfg.pin_bit_mask = (1ULL << (uint32_t)pin);
    io_cfg.mode = GPIO_MODE_INPUT;
    io_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    io_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_cfg.intr_type = GPIO_INTR_DISABLE;
    err = gpio_config(&io_cfg);
    if (err != ESP_OK)
    {
        return err;
    }

    err = gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
    if (err != ESP_OK)
    {
        return err;
    }

    switch (pull)
    {
        case GPIO_DEBUG_PULL_FLOATING:
            pull_mode = GPIO_FLOATING;
            break;
        case GPIO_DEBUG_PULL_UP:
            pull_mode = GPIO_PULLUP_ONLY;
            break;
        case GPIO_DEBUG_PULL_DOWN:
            pull_mode = GPIO_PULLDOWN_ONLY;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    return gpio_set_pull_mode((gpio_num_t)pin, pull_mode);
}

esp_err_t gpio_debug_set_output(gpio_debug_t *self, int pin, gpio_debug_level_t level)
{
    gpio_config_t io_cfg;
    esp_err_t err;

    err = gpio_debug_validate_common(self, pin);
    if (err != ESP_OK)
    {
        return err;
    }

    if (gpio_debug_policy_protects_pin(self, pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (level != GPIO_DEBUG_LEVEL_LOW && level != GPIO_DEBUG_LEVEL_HIGH)
    {
        return ESP_ERR_INVALID_ARG;
    }

    io_cfg.pin_bit_mask = (1ULL << (uint32_t)pin);
    io_cfg.mode = GPIO_MODE_OUTPUT;
    io_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    io_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_cfg.intr_type = GPIO_INTR_DISABLE;
    err = gpio_config(&io_cfg);
    if (err != ESP_OK)
    {
        return err;
    }

    err = gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
    if (err != ESP_OK)
    {
        return err;
    }

    return gpio_set_level((gpio_num_t)pin, (level == GPIO_DEBUG_LEVEL_HIGH) ? 1 : 0);
}

esp_err_t gpio_debug_get(gpio_debug_t *self, int pin, int *out_level)
{
    esp_err_t err;

    if (out_level == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    err = gpio_debug_validate_common(self, pin);
    if (err != ESP_OK)
    {
        return err;
    }

    *out_level = gpio_get_level((gpio_num_t)pin);

    return ESP_OK;
}

esp_err_t gpio_debug_register_commands(gpio_debug_t *self)
{
    if (self == NULL || !self->initialized)
    {
        return ESP_ERR_INVALID_ARG;
    }

    s_registered_gpio_debug = self;

    return ESP_OK;
}

gpio_debug_t *gpio_debug_get_registered(void)
{
    return s_registered_gpio_debug;
}
