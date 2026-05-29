#ifndef SPI_BUS_H
#define SPI_BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    spi_host_device_t host_id;
    gpio_num_t sclk_pin;
    gpio_num_t mosi_pin;
    gpio_num_t miso_pin;
    bool initialized;
} spi_bus_t;

typedef struct
{
    spi_host_device_t host_id;
    gpio_num_t sclk_pin;
    gpio_num_t mosi_pin;
    gpio_num_t miso_pin;
} spi_bus_cfg_t;

typedef enum
{
    SPI_BUS_PINS_STATE_KEEP = 0,
    SPI_BUS_PINS_STATE_HIZ
} spi_bus_pins_state_t;

typedef struct
{
    spi_bus_t *bus;
    gpio_num_t cs_pin;
    uint8_t mode;
    uint32_t clock_hz;
    bool initialized;
    spi_device_handle_t platform_handle;
} spi_device_t;

typedef struct
{
    spi_bus_t *bus;
    gpio_num_t cs_pin;
    uint8_t mode;
    uint32_t clock_hz;
} spi_device_cfg_t;

esp_err_t spi_bus_init(spi_bus_t *bus, const spi_bus_cfg_t *cfg);
esp_err_t spi_bus_deinit(spi_bus_t *bus);
esp_err_t spi_bus_set_pins_state(const spi_bus_t *bus,
                                 spi_bus_pins_state_t state);
bool spi_bus_is_initialized(const spi_bus_t *bus);

esp_err_t spi_device_init(spi_device_t *dev, const spi_device_cfg_t *cfg);
esp_err_t spi_device_deinit(spi_device_t *dev);
bool spi_device_is_initialized(const spi_device_t *dev);
esp_err_t spi_device_transfer(spi_device_t *dev,
                              const void *tx_data,
                              void *rx_data,
                              size_t length);

#ifdef __cplusplus
}
#endif

#endif /* SPI_BUS_H */
