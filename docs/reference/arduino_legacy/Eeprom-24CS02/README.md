# Eeprom-24CS02

## Název knihovny
`Eeprom-24CS02`

## Účel
CLI knihovna pro práci s EEPROM 24CS02 přes I2C (včetně čtení sériového čísla/USN oblasti podle implementace modulu).

## Architektura
- Modul má vlastní parser příkazů `i2c-eeprom-*` a `eeprom-*`.
- Funkce `Eeprom_ApplyCommand(...)` vrací výstupní řádky (`std::vector<String>`), které tiskne fixture.
- I2C lifecycle je integrován přes HAL (`hal_i2c_acquire/release`) pro sdílení sběrnice.

## CLI příkazy
| Příkaz | Parametry | Úspěch | Chyba |
|---|---|---|---|
| `i2c-eeprom-init` | `<bus> <sda> <scl> <freqHz> <addrEE> <addrUSN>` | `OK-1` + READY text | `ERR-1`/`Error: ...` dle routingu |
| `i2c-eeprom-deinit` | bez | `OK-1` | `ERR-1`/`Error: ...` |
| `eeprom-read-data` | bez nebo `<address>` | `OK-16` / `OK-1` + data | `ERR-1`/`Error: ...` |
| `eeprom-read-sn` | bez | `OK-1` + SN | `ERR-1`/`Error: ...` |
| `eeprom-write` | `<address> <hexData>` | `OK-0` | `ERR-1`/`Error: ...` |
| `eepromExt-write` | `<address> <hexData>` | `OK-0` | `ERR-1`/`Error: ...` |

## Response contract
- `OK-*` značí úspěšné zpracování příkazu (počet/datové řádky dle modulu).
- `ERR-1` značí chybu na úrovni příkazu.
- `Error: ...` je textový detail chyby vrácený routerem nebo modulem.

## Příklady CLI použití a očekávané odpovědi
```text
> i2c-eeprom-init 0 16 15 100000 0x56 0x5E
OK-1
EEPROM BUS READY (bus=0 SDA=16 SCL=15 100000Hz EE=0x56 USN=0x5E)
```

```text
> eeprom-read-data 0x20
OK-1
0x20: 11 22 33 44
```

```text
> eeprom-write 0x20 A1B2C3D4
OK-0
```

```text
> i2c-eeprom-init 0 16 15 100000 0x80 0x5E
ERR-1
Error: invalid I2C address
```

## Integrace
### Include
```cpp
#include <Eeprom-24CS02/xlib-Eeprom-24CS02.h>
```

### Směrování v fixture
```cpp
if (cmd.startsWith("i2c-eeprom-") || cmd.startsWith("eeprom-")) {
  std::vector<String> lines;
  String err;
  if (Eeprom_ApplyCommand(cmd, lines, err)) {
    for (const auto &line : lines) cmdPort.println(line.c_str());
  } else {
    cmdPort.println("ERR-1");
    cmdPort.println(("Error: " + err).c_str());
  }
}
```

`src/main.cpp` zůstává jen pro výběr fixture přes makra `FIXTURE_*`.

## Závislosti
- Arduino
- I2C (`Wire`/`Wire1`)
- HAL (`hal_i2c`)
