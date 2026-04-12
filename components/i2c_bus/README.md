# i2c_bus

Komponenta `i2c_bus` je malý ESP-IDF wrapper pro lifecycle I2C master sběrnice:

- validuje konfiguraci
- provede `i2c_param_config(...)`
- provede `i2c_driver_install(...)`
- při deinit provede `i2c_driver_delete(...)`

## Co komponenta dělá

- drží stav jedné sběrnice v `i2c_bus_t`
- poskytuje jednoduché API `init/deinit/is_initialized/port`
- loguje základní úspěch/chyby přes `ESP_LOGI/ESP_LOGE`

## Co komponenta nedělá

- neřeší bus manager
- neřeší mutexy
- neřeší refcounting
- neřeší ownership framework zařízení
- neřeší registry zařízení

## Scope iterace 1

- safe/default pin state a Hi-Z policy jsou mimo scope
- fixture zůstává composition root a rozhoduje, kdy se bus inicializuje a používá
