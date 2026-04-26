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

typedef struct
{
    int i2c_port;
    uint8_t dev_addr;
    uint32_t i2c_timeout_ms;
    bool initialized;
} adc_tla2024_t;

typedef struct
{
    int i2c_port;
    uint8_t dev_addr;
    uint32_t i2c_timeout_ms;
} adc_tla2024_cfg_t;

esp_err_t adc_tla2024_init(adc_tla2024_t *ctx,
                           const adc_tla2024_cfg_t *cfg);

esp_err_t adc_tla2024_read_reg16(adc_tla2024_t *ctx,
                                 uint8_t reg_addr,
                                 uint16_t *out_value);

esp_err_t adc_tla2024_write_reg16(adc_tla2024_t *ctx,
                                  uint8_t reg_addr,
                                  uint16_t value);

esp_err_t adc_tla2024_read_channel_raw12(adc_tla2024_t *ctx,
                                         uint8_t channel,
                                         uint16_t *out_raw12);

void adc_tla2024_register_commands(adc_tla2024_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* ADC_TLA2024_H */
