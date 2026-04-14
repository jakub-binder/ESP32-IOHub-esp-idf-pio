# temp_lm75bdp

Komponenta `temp_lm75bdp` poskytuje práci s teplotním čidlem LM75BDP přes I2C.
Obsahuje i vlastní CLI parser uvnitř komponenty.

## Veřejné API

- `esp_err_t temp_lm75bdp_init(temp_lm75bdp_t *ctx, const temp_lm75bdp_cfg_t *cfg);`
- `esp_err_t temp_lm75bdp_read_temperature_c(temp_lm75bdp_t *ctx, float *out_temp_c);`
- `esp_err_t temp_lm75bdp_set_thresholds_c(temp_lm75bdp_t *ctx, float thyst_c, float tos_c);`
- `void temp_lm75bdp_register_commands(temp_lm75bdp_t *ctx);`

## Konfigurace

`temp_lm75bdp_cfg_t` obsahuje:

- `i2c_port` – číslo I2C portu
- `dev_addr` – 7bit I2C adresa zařízení

Default adresa LM75BDP je `0x48`.

Poznámka:
- Komponenta neinicializuje I2C bus, používá už připravenou sběrnici.

## Dostupné commandy

- `temp.help`
- `temp.read`
- `temp.set_thresholds <thyst_c> <tos_c>`

## Druhá iterace (datasheet-driven)

- Init ověřuje dostupnost čidla čtením `CONF` registru.
- Při initu je provedeno jedno zahazovací čtení `TEMP` registru (první čtení po selectu pointeru může být neplatné).
- `TEMP` registr je dekódován jako 11bit two's complement s rozlišením `0.125 °C`.
- `Tos`/`Thyst` jsou enkódovány v krocích `0.5 °C` do bitů `D8..D0` (`[15:7]`).
- `temp.set_thresholds` validuje datasheet rozsah `-55 .. 125 °C` a pravidlo `thyst < tos`.

## Stručná integrace do fixture

1. V `fixture_setup()` inicializovat I2C bus přes `i2c_bus`.
2. Připravit `temp_lm75bdp_cfg_t` s `i2c_port = i2c_bus_port(...)` a `dev_addr = 0x48`.
3. Zavolat `temp_lm75bdp_init(...)`.
4. Ve `fixture_register_commands()` zavolat `temp_lm75bdp_register_commands(...)`.

## Omezení

- Komponenta zatím neposkytuje veřejné API pro detailní práci s jednotlivými bity `CONF` registru.
