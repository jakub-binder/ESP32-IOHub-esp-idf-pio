#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    i2c_port_t port;
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
    uint32_t frequency_hz;
    bool pullup_enable;
    bool initialized;
} i2c_bus_t;

typedef struct
{
    i2c_port_t port;
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
    uint32_t frequency_hz;
    bool pullup_enable;
} i2c_bus_config_t;

typedef enum
{
    I2C_BUS_PINS_STATE_KEEP = 0,
    I2C_BUS_PINS_STATE_HIZ
} i2c_bus_pins_state_t;

esp_err_t i2c_bus_init(i2c_bus_t *bus, const i2c_bus_config_t *cfg);
esp_err_t i2c_bus_deinit(i2c_bus_t *bus);
esp_err_t i2c_bus_set_pins_state(const i2c_bus_t *bus,
                                 i2c_bus_pins_state_t state);
bool i2c_bus_is_initialized(const i2c_bus_t *bus);
i2c_port_t i2c_bus_port(const i2c_bus_t *bus);

#ifdef __cplusplus
}
#endif

#endif /* I2C_BUS_H */
