# eeprom_24c64

Komponenta `eeprom_24c64` poskytuje jednoduché API pro práci s externí EEPROM přes I2C
a obsahuje registraci vlastních CLI commandů `EEPROM64:*`.

## Podporovaný čip

- 24C64 / M24C64
- kapacita: 8192 bytes
- page size: 32 bytes

## Architektura

- Komponenta **neinicializuje I2C bus**.
- Komponenta používá již připravenou I2C sběrnici (typicky přes `i2c_bus` komponentu).
- Fixture je composition root, který připraví I2C a předá konfiguraci do `eeprom_24c64`.

## Veřejné API

- `esp_err_t eeprom_24c64_init(eeprom_24c64_t *ctx, const eeprom_24c64_cfg_t *cfg);`
- `esp_err_t eeprom_24c64_read(eeprom_24c64_t *ctx, uint16_t mem_addr, void *out, size_t len);`
- `esp_err_t eeprom_24c64_write(eeprom_24c64_t *ctx, uint16_t mem_addr, const void *data, size_t len);`
- `void eeprom_24c64_register_commands(eeprom_24c64_t *ctx);`

## Dostupné commandy

- `EEPROM64:HELP`
- `EEPROM64:READ <addr> <len>`
- `EEPROM64:WRITE <addr> <hex_data>`
- `EEPROM64:DUMP`

Příklady:

- `EEPROM64:HELP`
- `EEPROM64:READ 0x00 16`
- `EEPROM64:WRITE 0x10 00000000`
- `EEPROM64:DUMP`
