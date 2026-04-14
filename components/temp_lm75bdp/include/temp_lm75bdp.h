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
    int i2c_port;
    uint8_t dev_addr;
    bool initialized;
} temp_lm75bdp_t;

typedef struct
{
    int i2c_port;
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
