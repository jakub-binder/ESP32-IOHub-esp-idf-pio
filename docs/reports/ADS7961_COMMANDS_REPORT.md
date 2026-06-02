# ADS7961 – report CLI příkazů

## Vytvořené soubory

- `components/ads7961/ads7961_commands.c`
- `docs/reports/ADS7961_COMMANDS_REPORT.md`

## Upravené soubory

- `components/ads7961/CMakeLists.txt`
- `components/ads7961/include/ads7961.h`
- `components/ads7961/README.md`

## Přidané API

- `void ads7961_register_commands(ads7961_t *ctx);`

## CLI příkazy

- `ADC:HELP`
- `ADC:READ-CH <0..15>`
- `ADC:READAVG-CH <0..15>`

## Formát odpovědi

- úspěch: `OK-1` + jeden řádek s hodnotou napětí (volty)
- chyba: `ERR-1` + jeden řádek s popisem chyby

## Poznámky k implementaci

- parsing argumentů je implementovaný přímo v `ads7961_commands.c`
- registrace je řešena přes `app_commands_register_custom_handler(...)`
- driver `ads7961.c` zůstává beze změn

## Doporučené HW testy

- ověřit `ADC:READ-CH 0` a `ADC:READAVG-CH 0` na známém vstupu
- ověřit, že neplatný kanál vrací `ERR-1` s popisem
