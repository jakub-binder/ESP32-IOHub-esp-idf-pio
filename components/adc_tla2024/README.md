# adc_tla2024

Komponenta `adc_tla2024` poskytuje první iteraci obsluhy ADC TLA2024 přes I2C.
Obsahuje i vlastní parser commandů uvnitř komponenty.

## Veřejné API

- `esp_err_t adc_tla2024_init(adc_tla2024_t *ctx, const adc_tla2024_cfg_t *cfg);`
- `esp_err_t adc_tla2024_read_reg16(adc_tla2024_t *ctx, uint8_t reg_addr, uint16_t *out_value);`
- `esp_err_t adc_tla2024_write_reg16(adc_tla2024_t *ctx, uint8_t reg_addr, uint16_t value);`
- `esp_err_t adc_tla2024_read_channel_raw12(adc_tla2024_t *ctx, uint8_t channel, uint16_t *out_raw12);`
- `void adc_tla2024_register_commands(adc_tla2024_t *ctx);`

## Konfigurace

`adc_tla2024_cfg_t` obsahuje:

- `i2c_port` – číslo I2C portu
- `dev_addr` – 7bit I2C adresa zařízení (default `0x48`)
- `i2c_timeout_ms` – timeout I2C operací v ms (`0` = default)

Poznámka:
- Komponenta neinicializuje I2C bus, používá už připravenou sběrnici.

## Dostupné commandy

- `TLA2024:READ <0|1>`
- `TLA2024:READALL`
- `TLA2024:REG <00|01>`

## Formát odpovědí

Datové řádky vrací pouze data (bez prefixu `TLA2024`, čísla kanálu nebo názvu registru).

- `TLA2024:READ <0|1>`
  - success:
    - `OK-1`
    - `<raw12_decimal>`
  - HW/device error:
    - `OK-1`
    - `Error: <detail>`

- `TLA2024:READALL`
  - success:
    - `OK-2`
    - `<raw12_ch0_decimal>`
    - `<raw12_ch1_decimal>`
  - partial error:
    - `OK-2`
    - `Error: CH0 <detail>` nebo `<raw12_ch0_decimal>`
    - `Error: CH1 <detail>` nebo `<raw12_ch1_decimal>`

- `TLA2024:REG <00|01>`
  - success:
    - `OK-1`
    - `<REG_VALUE_4_HEX_DIGITS>`
  - HW/device error:
    - `OK-1`
    - `Error: <detail>`
