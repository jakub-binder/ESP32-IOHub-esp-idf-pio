#ifndef TEMP_LM75BDP_H
#define TEMP_LM75BDP_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    /* I2C port selected by fixture via i2c_bus_port(...). */
    int i2c_port;
    /* 7-bit I2C device address. */
    uint8_t dev_addr;
    /* Internal state set by temp_lm75bdp_init(). */
    bool initialized;
} temp_lm75bdp_t;

typedef struct
{
    /* Valid range: 0..(I2C_NUM_MAX - 1). */
    int i2c_port;
    /* LM75BDP 7-bit I2C address (default 0x48). */
    uint8_t dev_addr;
} temp_lm75bdp_cfg_t;

esp_err_t temp_lm75bdp_init(temp_lm75bdp_t *ctx,
                            const temp_lm75bdp_cfg_t *cfg);

esp_err_t temp_lm75bdp_read_temperature_c(temp_lm75bdp_t *ctx,
                                          float *out_temp_c);

esp_err_t temp_lm75bdp_set_thresholds_c(temp_lm75bdp_t *ctx,
                                        float thyst_c,
                                        float tos_c);

void temp_lm75bdp_register_commands(temp_lm75bdp_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* TEMP_LM75BDP_H */
