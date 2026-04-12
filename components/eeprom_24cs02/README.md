# eeprom_24cs02

Komponenta `eeprom_24cs02` poskytuje jednoduché API pro práci s EEPROM 24CS02 přes I2C.
Obsahuje i vlastní CLI command parser uvnitř komponenty.

## Veřejné API

- `esp_err_t eeprom_24cs02_init(eeprom_24cs02_t *ctx, const eeprom_24cs02_cfg_t *cfg);`
- `esp_err_t eeprom_24cs02_read(eeprom_24cs02_t *ctx, uint8_t mem_addr, void *out, size_t len);`
- `esp_err_t eeprom_24cs02_write(eeprom_24cs02_t *ctx, uint8_t mem_addr, const void *data, size_t len);`
- `void eeprom_24cs02_register_commands(eeprom_24cs02_t *ctx);`

## Konfigurace

`eeprom_24cs02_cfg_t` obsahuje:

- `i2c_port` – číslo I2C portu
- `data_dev_addr` – I2C adresa data oblasti EEPROM
- `serial_dev_addr` – I2C adresa serial/security oblasti
- `dev_addr` – kompatibilní fallback pro data adresu (legacy)

Poznámka:
- Komponenta neinicializuje I2C bus, používá už připravenou sběrnici.
- Pokud `serial_dev_addr` není nastavená (`0`), command `eeprom.read_sn` vrací chybu.

## Dostupné commandy

- `eeprom.help`
- `eeprom.read16 <mem_addr>` (adresa musí být zarovnaná na 16 B)
- `eeprom.dump`
- `eeprom.write <mem_addr> <hex_data>` (adresa i délka dat musí být násobek 8 B)
- `eeprom.read_sn`

## Stručný příklad integrace do fixture

1. V `fixture_setup()` připravit a inicializovat I2C bus.
2. Vytvořit `eeprom_24cs02_cfg_t` s adresami `data_dev_addr` a `serial_dev_addr`.
3. Zavolat `eeprom_24cs02_init(...)`.
4. Ve `fixture_register_commands()` zavolat `eeprom_24cs02_register_commands(...)`.
