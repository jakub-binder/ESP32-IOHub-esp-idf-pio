#ifndef ADS7961_H
#define ADS7961_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "spi_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Runtime device context for ADS7961 operations.
 */
typedef struct
{
    /** Prepared SPI device handle managed by fixture. */
    spi_device_t *spi_dev;
    /** True if ADC range is configured as 2xRef. */
    bool range2x_ref;
    /** Positive reference voltage applied to REFP pin (volts). */
    float refp_volts;
    /** True after successful initialization. */
    bool initialized;
} ads7961_t;

/**
 * @brief Configuration used for ADS7961 context initialization.
 */
typedef struct
{
    /** Prepared SPI device handle managed by fixture. */
    spi_device_t *spi_dev;
    /** True if ADC range is configured as 2xRef. */
    bool range2x_ref;
    /** Positive reference voltage applied to REFP pin (volts). */
    float refp_volts;
} ads7961_cfg_t;

/**
 * @brief Initialize ADS7961 component context.
 *
 * Stores validated configuration into the context. SPI device must be created
 * by the fixture using the spi_bus component.
 *
 * Implemented in: components/ads7961/ads7961.c
 *
 * @param ctx Device context to initialize.
 * @param cfg Initialization configuration.
 * @return ESP_OK on success, otherwise esp_err_t error code.
 */
esp_err_t ads7961_init(ads7961_t *ctx, const ads7961_cfg_t *cfg);

/**
 * @brief Check if ADS7961 context has been initialized.
 *
 * Implemented in: components/ads7961/ads7961.c
 *
 * @param ctx Device context.
 * @return true if initialized, otherwise false.
 */
bool ads7961_is_initialized(const ads7961_t *ctx);

/**
 * @brief Read single ADS7961 channel using pipeline sequence.
 *
 * Executes three SPI transfers with the same command word and returns
 * the last received frame as data.
 *
 * Implemented in: components/ads7961/ads7961.c
 *
 * @param ctx Initialized device context.
 * @param channel Channel index (0..15).
 * @param out_code8 Output pointer for extracted 8-bit ADC code.
 * @param out_rx Optional output pointer for raw 16-bit RX frame (may be NULL).
 * @return ESP_OK on success, otherwise esp_err_t error code.
 */
esp_err_t ads7961_read_channel(ads7961_t *ctx,
                               uint8_t channel,
                               uint8_t *out_code8,
                               uint16_t *out_rx);

/**
 * @brief Read ADS7961 channel using 40 samples with 4-sample trimming.
 *
 * Performs the same pipelined sequence as the Arduino reference and returns
 * averaged code and voltage.
 *
 * Implemented in: components/ads7961/ads7961.c
 *
 * @param ctx Initialized device context.
 * @param channel Channel index (0..15).
 * @param out_avg_volts Output pointer for averaged voltage.
 * @param out_avg_code8 Output pointer for averaged 8-bit ADC code.
 * @return ESP_OK on success, otherwise esp_err_t error code.
 */
esp_err_t ads7961_read_channel_avg(ads7961_t *ctx,
                                   uint8_t channel,
                                   float *out_avg_volts,
                                   float *out_avg_code8);

/**
 * @brief Register ADS7961 CLI commands into app_commands subsystem.
 *
 * Implemented in: components/ads7961/ads7961_commands.c
 *
 * @param ctx Initialized device context.
 */
void ads7961_register_commands(ads7961_t *ctx);

/**
 * @brief Convert 8-bit ADC code to voltage using configured reference.
 *
 * Implemented in: components/ads7961/ads7961.c
 *
 * @param ctx Initialized device context.
 * @param code8 8-bit ADC code.
 * @param out_volts Output pointer for converted voltage.
 * @return ESP_OK on success, otherwise esp_err_t error code.
 */
esp_err_t ads7961_code8_to_volts(const ads7961_t *ctx,
                                 uint8_t code8,
                                 float *out_volts);

/**
 * @brief Build ADS7961 manual command word for selected channel.
 *
 * Implements the same bit layout as the Arduino reference.
 *
 * Implemented in: components/ads7961/ads7961.c
 *
 * @param ctx Initialized device context.
 * @param channel Channel index (0..15).
 * @return 16-bit command word (0 when channel is invalid).
 */
uint16_t ads7961_build_manual_cmd(const ads7961_t *ctx, uint8_t channel);

/**
 * @brief Extract 4-bit header from ADS7961 RX frame.
 *
 * Implemented in: components/ads7961/ads7961.c
 *
 * @param rx Raw 16-bit RX frame.
 * @return Header nibble (bits 15..12).
 */
uint8_t ads7961_extract_header4(uint16_t rx);

/**
 * @brief Extract 8-bit ADC data from ADS7961 RX frame.
 *
 * Implemented in: components/ads7961/ads7961.c
 *
 * @param rx Raw 16-bit RX frame.
 * @return 8-bit data (bits 11..4).
 */
uint8_t ads7961_extract_data8(uint16_t rx);

#ifdef __cplusplus
}
#endif

#endif /* ADS7961_H */
