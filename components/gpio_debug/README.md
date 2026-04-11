# gpio_debug

Komponenta `gpio_debug` poskytuje diagnostické ovládání GPIO přes command systém.
GPIO commandy jsou obsluhované přímo komponentou a jsou dostupné jen na debug portu.

## Veřejné API

- `esp_err_t gpio_debug_init(gpio_debug_t *self, const gpio_debug_config_t *cfg);`
- `esp_err_t gpio_debug_set_input(gpio_debug_t *self, int pin, gpio_debug_pull_t pull);`
- `esp_err_t gpio_debug_set_output(gpio_debug_t *self, int pin, gpio_debug_level_t level);`
- `esp_err_t gpio_debug_get(gpio_debug_t *self, int pin, int *out_level);`
- `esp_err_t gpio_debug_register_commands(gpio_debug_t *self);`

## Policy

- `allowed_mask` určuje, které piny může `gpio_debug` obsluhovat.
- `protected_mask` určuje piny, které nelze přepínat přes `gpio.in` a `gpio.out`.
- Pin musí být validní GPIO a současně povolený policy, jinak API vrací chybu.

## Dostupné commandy

- `gpio.help`
- `gpio.get <pin>`
- `gpio.in <pin> <none|up|down>`
- `gpio.out <pin> <0|1>`

## Stručný příklad integrace do fixture

1. V `fixture_setup()` (resp. setup implementaci fixtury) inicializovat komponentu:
   - připravit `gpio_debug_config_t` s `allowed_mask` a `protected_mask`
   - zavolat `gpio_debug_init(...)`
2. Ve `fixture_register_commands()` (resp. register_commands implementaci fixtury):
   - zavolat `gpio_debug_register_commands(...)`

