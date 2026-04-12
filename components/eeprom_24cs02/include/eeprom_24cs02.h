#ifndef EEPROM_24CS02_H
#define EEPROM_24CS02_H

#include <stdbool.h>
#include <stddef.h>
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
} eeprom_24cs02_t;

typedef struct
{
    int i2c_port;
    uint8_t dev_addr;
} eeprom_24cs02_cfg_t;

esp_err_t eeprom_24cs02_init(eeprom_24cs02_t *ctx, const eeprom_24cs02_cfg_t *cfg);

esp_err_t eeprom_24cs02_read(eeprom_24cs02_t *ctx,
                             uint8_t mem_addr,
                             void *out,
                             size_t len);

esp_err_t eeprom_24cs02_write(eeprom_24cs02_t *ctx,
                              uint8_t mem_addr,
                              const void *data,
                              size_t len);

void eeprom_24cs02_register_commands(eeprom_24cs02_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* EEPROM_24CS02_H */
