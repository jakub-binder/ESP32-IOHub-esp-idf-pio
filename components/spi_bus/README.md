# spi_bus

Komponenta `spi_bus` je malý ESP-IDF wrapper pro lifecycle SPI master sběrnice a zařízení:

- validuje konfiguraci sběrnice i zařízení
- provede `spi_bus_initialize(...)`
- provede `spi_bus_add_device(...)`
- při deinit provede `spi_bus_free(...)` a `spi_bus_remove_device(...)`
- umožní explicitně nastavit stav SCLK/MOSI/MISO pinů po použití sběrnice

## Co komponenta dělá

- drží stav jedné sběrnice v `spi_bus_t`
- drží stav jednoho zařízení v `spi_device_t`
- poskytuje jednoduché API `init/deinit/set_pins_state/is_initialized/transfer`
- loguje základní úspěch/chyby přes `ESP_LOGI/ESP_LOGE`

## Transfer API

`spi_device_transfer(spi_device_t *dev, const void *tx_data, void *rx_data, size_t length)`

- `length` je počet bytů
- `tx_data` nebo `rx_data` může být `NULL` (write-only / read-only)
- `length` musí být > 0

## Pin state

`spi_bus_set_pins_state(const spi_bus_t *bus, spi_bus_pins_state_t state)`

Podporované stavy:

- `SPI_BUS_PINS_STATE_KEEP`:
  - no-op
  - vrací `ESP_OK`
- `SPI_BUS_PINS_STATE_HIZ`:
  - pro `SCLK`, `MOSI` i `MISO` nastaví GPIO na input
  - vypne interní pull-up
  - vypne interní pull-down

`spi_bus_deinit()` safe/default stav pinů nevolá automaticky.
Policy zůstává ve fixture: fixture explicitně rozhoduje, kdy udělat `deinit` a kdy volat `set_pins_state(HIZ)`.

## Co komponenta nedělá

- neřeší bus manager
- neřeší mutexy
- neřeší refcounting
- neřeší ownership framework zařízení
- neřeší registry zařízení

## Scope

- fixture zůstává composition root a rozhoduje, kdy se bus inicializuje, deinitializuje a kdy se přepíná pin state
- komponenta neobsahuje automatickou magii ani ownership framework
