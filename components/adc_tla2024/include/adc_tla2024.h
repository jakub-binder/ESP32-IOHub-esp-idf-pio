#ifndef ADC_TLA2024_H
#define ADC_TLA2024_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_TLA2024_DEFAULT_DEV_ADDR       0x48U
#define ADC_TLA2024_REG_CONVERSION         0x00U
#define ADC_TLA2024_REG_CONFIG             0x01U

/**
 * @brief Runtime device context for TLA2024 operations.
 */
typedef struct
{
    /** I2C port index used for communication with the device. */
    int i2c_port;
    /** 7-bit I2C device address. */
    uint8_t dev_addr;
    /** I2C operation timeout in milliseconds. */
    uint32_t i2c_timeout_ms;
    /** True after successful initialization and probe. */
    bool initialized;
} adc_tla2024_t;

/**
 * @brief Runtime configuration for TLA2024 context initialization.
 */
typedef struct
{
    /** I2C port index used for communication with the device. */
    int i2c_port;
    /** 7-bit I2C device address (default is @ref ADC_TLA2024_DEFAULT_DEV_ADDR). */
    uint8_t dev_addr;
    /** I2C operation timeout in milliseconds (0 uses component default). */
    uint32_t i2c_timeout_ms;
} adc_tla2024_cfg_t;

/**
 * @brief Initialize TLA2024 component context and probe device availability.
 *
 * Stores validated configuration into the context and reads config register as probe.
 *
 * Implemented in: components/adc_tla2024/adc_tla2024.c
 *
 * @param ctx Device context to initialize.
 * @param cfg Initialization configuration.
 * @return ESP_OK on success, otherwise esp_err_t error code.
 */
esp_err_t adc_tla2024_init(adc_tla2024_t *ctx,
                           const adc_tla2024_cfg_t *cfg);

/**
 * @brief Read 16-bit value from selected TLA2024 register.
 *
 * Implemented in: components/adc_tla2024/adc_tla2024.c
 *
 * @param ctx Initialized device context.
 * @param reg_addr Register address to read.
 * @param out_value Output pointer for read 16-bit register value.
 * @return ESP_OK on success, otherwise esp_err_t error code.
 */
esp_err_t adc_tla2024_read_reg16(adc_tla2024_t *ctx,
                                 uint8_t reg_addr,
                                 uint16_t *out_value);

/**
 * @brief Write 16-bit value to selected TLA2024 register.
 *
 * Implemented in: components/adc_tla2024/adc_tla2024.c
 *
 * @param ctx Initialized device context.
 * @param reg_addr Register address to write.
 * @param value 16-bit value written to register.
 * @return ESP_OK on success, otherwise esp_err_t error code.
 */
esp_err_t adc_tla2024_write_reg16(adc_tla2024_t *ctx,
                                  uint8_t reg_addr,
                                  uint16_t value);

/**
 * @brief Read raw 12-bit ADC value from selected TLA2024 channel.
 *
 * Starts single-shot conversion with fixed first-iteration settings and reads conversion register.
 *
 * Implemented in: components/adc_tla2024/adc_tla2024.c
 *
 * @param ctx Initialized device context.
 * @param channel ADC channel index (currently 0 or 1).
 * @param out_raw12 Output pointer for raw 12-bit ADC value.
 * @return ESP_OK on success, otherwise esp_err_t error code.
 */
esp_err_t adc_tla2024_read_channel_raw12(adc_tla2024_t *ctx,
                                         uint8_t channel,
                                         uint16_t *out_raw12);

/**
 * @brief Register TLA2024 CLI command handler in app command framework.
 *
 * Implemented in: components/adc_tla2024/adc_tla2024.c
 *
 * @param ctx Initialized device context used by registered command handler.
 */
void adc_tla2024_register_commands(adc_tla2024_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* ADC_TLA2024_H */
