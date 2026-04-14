# temp_lm75bdp

Komponenta `temp_lm75bdp` poskytuje základní práci s teplotním čidlem LM75BDP přes I2C.
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

Default adresa LM75BDP v této iteraci je `0x48`.

Poznámka:
- Komponenta neinicializuje I2C bus, používá už připravenou sběrnici.

## Dostupné commandy

- `temp.help`
- `temp.read`
- `temp.set_thresholds <thyst_c> <tos_c>`

## Stručná integrace do fixture

1. V `fixture_setup()` inicializovat I2C bus přes `i2c_bus`.
2. Připravit `temp_lm75bdp_cfg_t` s `i2c_port = i2c_bus_port(...)` a `dev_addr = 0x48`.
3. Zavolat `temp_lm75bdp_init(...)`.
4. Ve `fixture_register_commands()` zavolat `temp_lm75bdp_register_commands(...)`.

## Omezení první iterace

- Převod teploty i threshold encoding jsou best-effort podle legacy reference.
- Neobsahuje plné pokrytí `CONF` registru.
- Datasheet-driven zpřesnění převodů je plánované v další iteraci.
