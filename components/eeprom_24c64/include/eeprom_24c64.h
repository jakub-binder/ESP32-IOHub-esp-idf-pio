#ifndef EEPROM_24C64_H
#define EEPROM_24C64_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Runtime context komponenty EEPROM 24C64.
 *
 * Struktura drží parametry zařízení používané při čtení/zápisu
 * a příznak úspěšné inicializace.
 */
typedef struct
{
    /** @brief I2C port použitý pro komunikaci se zařízením. */
    int i2c_port;
    /** @brief 7bit I2C adresa EEPROM zařízení. */
    uint8_t dev_addr;
    /** @brief Timeout ACK polling po page write operaci (ms). */
    uint32_t ack_poll_timeout_ms;
    /** @brief Příznak, že kontext byl úspěšně inicializován. */
    bool initialized;
} eeprom_24c64_t;

/**
 * @brief Konfigurace pro inicializaci komponenty EEPROM 24C64.
 */
typedef struct
{
    /** @brief I2C port použitý pro komunikaci se zařízením. */
    int i2c_port;
    /** @brief 7bit I2C adresa EEPROM zařízení. */
    uint8_t dev_addr;
    /** @brief Timeout ACK polling po page write operaci (ms). */
    uint32_t ack_poll_timeout_ms;
} eeprom_24c64_cfg_t;

/**
 * @brief Inicializuje kontext zařízení EEPROM 24C64.
 *
 * Uloží I2C port, adresu zařízení a časování ACK polling.
 * Neinicializuje I2C bus; ten musí být připravený externě (např. přes i2c_bus).
 *
 * Implemented in: components/eeprom_24c64/eeprom_24c64.c
 *
 * @param ctx Kontext zařízení, který se má inicializovat.
 * @param cfg Vstupní konfigurace zařízení.
 * @return ESP_OK při úspěchu, jinak esp_err_t chybový kód.
 */
esp_err_t eeprom_24c64_init(eeprom_24c64_t *ctx, const eeprom_24c64_cfg_t *cfg);

/**
 * @brief Načte data z EEPROM 24C64.
 *
 * Čte `len` bytů od paměťové adresy `mem_addr` do bufferu `out`.
 *
 * Implemented in: components/eeprom_24c64/eeprom_24c64.c
 *
 * @param ctx Inicializovaný kontext zařízení.
 * @param mem_addr Počáteční adresa v EEPROM.
 * @param out Výstupní buffer pro načtená data.
 * @param len Počet bytů ke čtení.
 * @return ESP_OK při úspěchu, jinak esp_err_t chybový kód.
 */
esp_err_t eeprom_24c64_read(eeprom_24c64_t *ctx,
                            uint16_t mem_addr,
                            void *out,
                            size_t len);

/**
 * @brief Zapíše data do EEPROM 24C64.
 *
 * Zapíše `len` bytů z bufferu `data` od paměťové adresy `mem_addr`.
 * Zápis respektuje page size zařízení.
 *
 * Implemented in: components/eeprom_24c64/eeprom_24c64.c
 *
 * @param ctx Inicializovaný kontext zařízení.
 * @param mem_addr Počáteční adresa v EEPROM.
 * @param data Vstupní data pro zápis.
 * @param len Počet bytů k zápisu.
 * @return ESP_OK při úspěchu, jinak esp_err_t chybový kód.
 */
esp_err_t eeprom_24c64_write(eeprom_24c64_t *ctx,
                             uint16_t mem_addr,
                             const void *data,
                             size_t len);

/**
 * @brief Registruje CLI command handler pro EEPROM64 příkazy.
 *
 * Zaregistruje custom command handler do app_commands subsystemu.
 *
 * Implemented in: components/eeprom_24c64/eeprom_24c64.c
 *
 * @param ctx Inicializovaný kontext zařízení.
 */
void eeprom_24c64_register_commands(eeprom_24c64_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* EEPROM_24C64_H */
