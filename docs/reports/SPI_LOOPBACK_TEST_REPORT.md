# SPI_LOOPBACK_TEST_REPORT

## Upravené soubory

- `src/fixtures/fixture_default.c`
- `docs/reports/SPI_LOOPBACK_TEST_REPORT.md`

## Jak test funguje

- CLI příkaz `spi-loopback-test` je registrovaný ve fixture `FIXTURE_DEFAULT`.
- Při spuštění se inicializuje SPI bus přes `spi_bus` a přidá se jedno zařízení.
- Odešle se testovací sekvence `0x55 0xAA 0x12 0x34`, přijme se odpověď a porovná se TX/RX.
- Pokud jsou data shodná, vypíše se `PASS`.
- Pokud se data liší, vypíše se `FAIL` a řádky s `TX:` a `RX:`.
- Na konci testu se vždy provede `spi_device_deinit()` a `spi_bus_deinit()`.

## Jak zapojit MOSI a MISO

- Propoj fyzicky MOSI a MISO piny stejné SPI sběrnice (primárně VSPI / SPI3).
- Konkrétní piny pro desku jsou definované v `include/board/board_*.h` jako:
  - `BOARD_VSPI_MOSI_PIN`
  - `BOARD_VSPI_MISO_PIN`
- Pokud by VSPI nebylo dostupné, test použije HSPI (`BOARD_HSPI_MOSI_PIN`, `BOARD_HSPI_MISO_PIN`).

## Jak spustit test

1. Nahraj firmware s fixturou `FIXTURE_DEFAULT`.
2. Připoj se na debug command port (UART0 / USB CDC podle konfigurace).
3. Pošli příkaz:
   - `spi-loopback-test`
4. Očekávej odpověď `PASS` při správném loopback propojení, jinak `FAIL` s TX/RX.
