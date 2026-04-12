# i2c_bus

Komponenta `i2c_bus` je malý ESP-IDF wrapper pro lifecycle I2C master sběrnice:

- validuje konfiguraci
- provede `i2c_param_config(...)`
- provede `i2c_driver_install(...)`
- při deinit provede `i2c_driver_delete(...)`
- umožní explicitně nastavit stav SDA/SCL pinů po použití sběrnice

## Co komponenta dělá

- drží stav jedné sběrnice v `i2c_bus_t`
- poskytuje jednoduché API `init/deinit/set_pins_state/is_initialized/port`
- loguje základní úspěch/chyby přes `ESP_LOGI/ESP_LOGE`

## Nové API (iterace 2)

`i2c_bus_set_pins_state(const i2c_bus_t *bus, i2c_bus_pins_state_t state)`

Podporované stavy:

- `I2C_BUS_PINS_STATE_KEEP`:
  - no-op
  - vrací `ESP_OK`
- `I2C_BUS_PINS_STATE_HIZ`:
  - pro `SDA` i `SCL` nastaví GPIO na input
  - vypne interní pull-up
  - vypne interní pull-down

`i2c_bus_deinit()` safe/default stav pinů nevolá automaticky.
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
