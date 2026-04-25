#ifndef EEPROM_24C64_H
#define EEPROM_24C64_H

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
    uint32_t ack_poll_timeout_ms;
    bool initialized;
} eeprom_24c64_t;

typedef struct
{
    int i2c_port;
    uint8_t dev_addr;
    uint32_t ack_poll_timeout_ms;
} eeprom_24c64_cfg_t;

esp_err_t eeprom_24c64_init(eeprom_24c64_t *ctx, const eeprom_24c64_cfg_t *cfg);

esp_err_t eeprom_24c64_read(eeprom_24c64_t *ctx,
                            uint16_t mem_addr,
                            void *out,
                            size_t len);

esp_err_t eeprom_24c64_write(eeprom_24c64_t *ctx,
                             uint16_t mem_addr,
                             const void *data,
                             size_t len);

void eeprom_24c64_register_commands(eeprom_24c64_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* EEPROM_24C64_H */
