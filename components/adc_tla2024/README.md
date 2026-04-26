# adc_tla2024

## Účel komponenty

Komponenta `adc_tla2024` zajišťuje obsluhu ADC čipu **TLA2024** přes I2C.
Poskytuje:

- veřejné API pro inicializaci, čtení registrů a čtení raw ADC dat
- command handler pro `TLA2024:*` příkazy

## Podporovaný čip

- Texas Instruments **TLA2024**

## Architektura

- komponenta **neinicializuje I2C bus**
- komponenta používá sdílenou vrstvu `i2c_bus`
- fixture je **composition root** (inicializuje bus, vytváří contexty, registruje commandy)

## Veřejné API

- `esp_err_t adc_tla2024_init(adc_tla2024_t *ctx, const adc_tla2024_cfg_t *cfg);`
  - inicializace contextu a ověřovací probe přes čtení config registru
- `esp_err_t adc_tla2024_read_reg16(adc_tla2024_t *ctx, uint8_t reg_addr, uint16_t *out_value);`
  - čtení 16bit registru
- `esp_err_t adc_tla2024_write_reg16(adc_tla2024_t *ctx, uint8_t reg_addr, uint16_t value);`
  - zápis 16bit hodnoty do registru
- `esp_err_t adc_tla2024_read_channel_raw12(adc_tla2024_t *ctx, uint8_t channel, uint16_t *out_raw12);`
  - single-shot měření a výstup raw 12bit hodnoty
- `void adc_tla2024_register_commands(adc_tla2024_t *ctx);`
  - registrace `TLA2024:*` command handleru

## Commandy

- `TLA2024:HELP`
- `TLA2024:READ <0|1>`
- `TLA2024:READALL`
- `TLA2024:REG <00|01>`

## Příklady commandů a odpovědí

### `TLA2024:HELP`

```
OK-7
TLA2024:HELP
TLA2024:READ <0|1>
TLA2024:READALL
TLA2024:REG <00|01>
Examples:
TLA2024:READ 0
TLA2024:REG 01
```

### `TLA2024:READ 0`

Success:

```
OK-1
1234
```

I2C/HW chyba:

```
OK-1
Error: ESP_ERR_TIMEOUT (-107)
```

### `TLA2024:READALL`

Success:

```
OK-2
1234
1201
```

Parciální chyba:

```
OK-2
Error: CH0 ESP_ERR_TIMEOUT (-107)
1189
```

### `TLA2024:REG 01`

Success:

```
OK-1
8583
```

I2C/HW chyba:

```
OK-1
Error: ESP_ERR_TIMEOUT (-107)
```

## Response protokol

- `OK-N` = úspěšná odpověď s `N` datovými řádky
- `ERR-1` = syntaktická/argumentová chyba commandu
- `Error: ...` = detail chyby na datovém řádku (typicky u I2C/HW chyby)

Poznámky:

- syntax/argument chyby vrací `ERR-1` (např. `ERR usage: ...`)
- I2C/HW chyby vrací `OK-N` s řádkem `Error: ...`

## Omezení první iterace

- podporované jsou pouze kanály `0` a `1`
- výstup je `raw12` v decimálním formátu
- PGA je fixní na `±4.096 V`
- data rate je fixní na `1600 SPS`
- zatím není přepočet na volty
- zatím není nastavování gain/data rate přes command
