# ADS7961 – integrace do fixture_default

## Cíl

- odstranit dočasný SPI loopback test z `fixture_default`
- integrovat ADS7961 do `FIXTURE_DEFAULT`
- zaregistrovat ADC CLI příkazy

## Upravené soubory

- `src/fixtures/fixture_default.c`

## Přidané soubory

- `docs/reports/ADS7961_FIXTURE_INTEGRATION_REPORT.md`

## Konfigurace ADS7961

- SPI host: VSPI/HSPI dle board maker (makra `BOARD_HAS_VSPI` / `BOARD_HAS_HSPI`)
- SPI mode: `0`
- SPI clock: `1 000 000 Hz`
- `refp_volts`: `2.5`
- `range2x_ref`: `true`

## Poznámky k implementaci

- SPI loopback test a související helper funkce byly odstraněny z `fixture_default`.
- ADS7961 používá stávající `spi_bus` a je inicializován v `fixture_default_setup_impl`.
- CLI příkazy ADS7961 jsou registrovány voláním `ads7961_register_commands(&g_ads7961)`.
