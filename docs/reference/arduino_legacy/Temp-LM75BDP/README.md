# Temp-LM75BDP

## Název knihovny
`Temp-LM75BDP`

## Účel
Knihovna pro teplotní senzor LM75BDP přes I2C. Umožňuje inicializaci, deinitializaci, čtení teploty a zápis prahů `Thyst`/`Tos`.

## Architektura
- `TempLM75_ApplyCommand(...)` zpracovává `temp-*` příkazy a vrací textový výstup/chybu.
- Modul drží vlastní stav senzoru a konfigurace sběrnice.
- Bez globálního CLI routeru; routování zajišťuje fixture.

## CLI příkazy
| Příkaz | Parametry | Úspěch | Chyba |
|---|---|---|---|
| `temp-init` | bez | `OK-1` + `TEMP INIT OK` | `ERR-1`/`Error: ...` dle routingu |
| `temp-init` | `<bus> <sda> <scl> <freqHz> <addr>` | `OK-1` + `TEMP INIT OK` | `ERR-1`/`Error: ...` |
| `temp-deinit` | bez | `OK-1` + `TEMP DE-INIT OK` | `ERR-1`/`Error: ...` |
| `temp-read-data` | bez | `OK-1` + teplota | `ERR-1`/`Error: ...` |
| `temp-set-treshold` | `<Thyst> <Tos>` | `OK-1` + `SET TRESHOLD OK` | `ERR-1`/`Error: ...` |

Pozn.: Název příkazu je záměrně `temp-set-treshold` (kompatibilita s implementací).

## Response contract
- `OK-*` značí úspěšné zpracování příkazu (počet/datové řádky dle modulu).
- `ERR-1` značí chybu na úrovni příkazu.
- `Error: ...` je textový detail chyby vrácený routerem nebo modulem.

## Příklady CLI použití a očekávané odpovědi
```text
> temp-init 0 16 15 100000 0x48
OK-1
TEMP INIT OK
```

```text
> temp-read-data
OK-1
24.875
```

```text
> temp-set-treshold 19.0 20.0
OK-1
SET TRESHOLD OK
```

```text
> temp-set-treshold 30.0 20.0
ERR-1
Error: Thyst must be lower than Tos
```

## Integrace
### Include
```cpp
#include <Temp-LM75BDP/xlib-Temp-LM75BDP.h>
```

### Směrování v fixture
```cpp
if (cmd.startsWith("temp-")) {
  String out, err;
  if (TempLM75_ApplyCommand(cmd, out, err)) {
    cmdPort.println("OK-1");
    if (out.length()) cmdPort.println(out.c_str());
  } else {
    cmdPort.println("ERR-1");
    cmdPort.println(("Error: " + err).c_str());
  }
}
```

## Závislosti
- Arduino
- I2C (`Wire`/`Wire1`)
- Bez přímé závislosti na SPI
