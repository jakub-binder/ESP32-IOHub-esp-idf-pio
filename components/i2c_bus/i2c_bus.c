#include "i2c_bus.h"

#include "esp_log.h"

static const char *const I2C_BUS_TAG = "i2c_bus";
static const uint32_t I2C_BUS_MIN_FREQUENCY_HZ = 1U;

static bool i2c_bus_port_is_valid(i2c_port_t port)
{
    return (port >= 0) && (port < I2C_NUM_MAX);
}

static bool i2c_bus_gpio_is_valid(gpio_num_t pin)
{
    return GPIO_IS_VALID_GPIO(pin);
}

static esp_err_t i2c_bus_configure_pins_hiz(gpio_num_t sda_pin, gpio_num_t scl_pin)
{
    const gpio_config_t gpio_cfg = {
        .pin_bit_mask = (1ULL << sda_pin) | (1ULL << scl_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&gpio_cfg);
}

bool i2c_bus_is_initialized(const i2c_bus_t *bus)
{
    return (bus != NULL) && bus->initialized;
}

i2c_port_t i2c_bus_port(const i2c_bus_t *bus)
{
    if (bus == NULL)
    {
        return I2C_NUM_MAX;
    }

    return bus->port;
}

esp_err_t i2c_bus_init(i2c_bus_t *bus, const i2c_bus_config_t *cfg)
{
    i2c_config_t i2c_cfg;
    esp_err_t err;

    if (bus == NULL || cfg == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!i2c_bus_port_is_valid(cfg->port) ||
        !i2c_bus_gpio_is_valid(cfg->sda_pin) ||
        !i2c_bus_gpio_is_valid(cfg->scl_pin) ||
        (cfg->frequency_hz < I2C_BUS_MIN_FREQUENCY_HZ))
    {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cfg.mode = I2C_MODE_MASTER;
    i2c_cfg.sda_io_num = cfg->sda_pin;
    i2c_cfg.scl_io_num = cfg->scl_pin;
    i2c_cfg.sda_pullup_en = cfg->pullup_enable ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    i2c_cfg.scl_pullup_en = cfg->pullup_enable ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    i2c_cfg.master.clk_speed = cfg->frequency_hz;
    i2c_cfg.clk_flags = 0;

    err = i2c_param_config(cfg->port, &i2c_cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(I2C_BUS_TAG, "i2c_param_config failed: %d", (int)err);
        return err;
    }

    err = i2c_driver_install(cfg->port, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(I2C_BUS_TAG, "i2c_driver_install failed: %d", (int)err);
        return err;
    }

    bus->port = cfg->port;
    bus->sda_pin = cfg->sda_pin;
    bus->scl_pin = cfg->scl_pin;
    bus->frequency_hz = cfg->frequency_hz;
    bus->pullup_enable = cfg->pullup_enable;
    bus->initialized = true;

    ESP_LOGI(I2C_BUS_TAG, "initialized: port=%d sda=%d scl=%d hz=%lu",
             (int)bus->port,
             (int)bus->sda_pin,
             (int)bus->scl_pin,
             (unsigned long)bus->frequency_hz);

    return ESP_OK;
}

esp_err_t i2c_bus_deinit(i2c_bus_t *bus)
{
    esp_err_t err;

    if (bus == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!bus->initialized)
    {
        return ESP_OK;
    }

    err = i2c_driver_delete(bus->port);
    if (err != ESP_OK)
    {
        ESP_LOGE(I2C_BUS_TAG, "i2c_driver_delete failed: %d", (int)err);
        return err;
    }

    bus->initialized = false;
    ESP_LOGI(I2C_BUS_TAG, "deinitialized: port=%d", (int)bus->port);
    return ESP_OK;
}

esp_err_t i2c_bus_set_pins_state(const i2c_bus_t *bus,
                                 i2c_bus_pins_state_t state)
{
    esp_err_t err;

    if (bus == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    switch (state)
    {
        case I2C_BUS_PINS_STATE_KEEP:
            return ESP_OK;

        case I2C_BUS_PINS_STATE_HIZ:
            if (!i2c_bus_gpio_is_valid(bus->sda_pin) ||
                !i2c_bus_gpio_is_valid(bus->scl_pin))
            {
                ESP_LOGE(I2C_BUS_TAG, "invalid I2C pins for HIZ: sda=%d scl=%d",
                         (int)bus->sda_pin, (int)bus->scl_pin);
                return ESP_ERR_INVALID_ARG;
            }

            err = i2c_bus_configure_pins_hiz(bus->sda_pin, bus->scl_pin);
            if (err != ESP_OK)
            {
                ESP_LOGE(I2C_BUS_TAG, "failed to set I2C pins to HIZ: sda=%d scl=%d err=%d",
                         (int)bus->sda_pin, (int)bus->scl_pin, (int)err);
                return err;
            }

            ESP_LOGI(I2C_BUS_TAG, "pins set to HIZ: sda=%d scl=%d",
                     (int)bus->sda_pin, (int)bus->scl_pin);
            return ESP_OK;

        default:
            return ESP_ERR_INVALID_ARG;
    }
}
