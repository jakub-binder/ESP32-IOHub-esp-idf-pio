// xlib-Eeprom-24CS02.cpp
#include <Eeprom-24CS02/xlib-Eeprom-24CS02.h>
#include <hal/hal_i2c.h>

// ===== Definice interniho stavu modulu =====
namespace
{
uint8_t eepromData[256];
uint8_t usnData[16];

uint8_t g_bus = 0; // 0=Wire, 1=Wire1
uint8_t g_addrEE = 0x54;
uint8_t g_addrUSN = 0x5C;

// Interní stav pro řízený init/deinit
bool g_busOwnedByEeprom = false; // knihovna volala acquire() → smí volat release()
int g_ownedSDA = -1;
int g_ownedSCL = -1;
uint32_t g_ownedFreq = 0;

uint8_t busFromWire(TwoWire &w)
{
#if defined(WIRE_INTERFACES_COUNT) && (WIRE_INTERFACES_COUNT > 1)
  if (&w == &Wire1)
  {
    return 1;
  }
#endif
  return 0;
}

TwoWire &eepromI2C()
{
  return hal_i2c(g_bus);
}
} // namespace

// ======= EXT konfigurace =======
static const uint8_t EEPROM_EXT_RETRY_COUNT = 2; // kolikrát opakovat po prvním neúspěchu (2 => max 3 pokusy)
static const uint8_t EEPROM_EXT_BLOCK_SIZE = 16; // zapisujeme a ověřujeme po 16B

// Nastavi sbernici i adresy v jednom kroku
void eepromConfigure(TwoWire &w, uint8_t eepromAddr, uint8_t usnAddr)
{
  g_bus = busFromWire(w);
  g_addrEE = eepromAddr;
  g_addrUSN = usnAddr;
}

// === NOVÉ: řízený init/deinit ===
bool eepromI2CInitOn(TwoWire &w, int sda, int scl, uint32_t freqHz,
                     uint8_t eepromAddr, uint8_t usnAddr, String &err)
{
  err = "";

  // Pokud jsme už dřív vlastnili nějakou sběrnici, nejdřív ji uvolni
  if (g_busOwnedByEeprom)
  {
    hal_i2c_release(g_bus);
    g_busOwnedByEeprom = false;
  }

  const uint8_t bus = busFromWire(w);
  if (!hal_i2c_acquire(bus, sda, scl, freqHz))
  {
    err = "Wire.begin() failed";
    return false;
  }

  // Zapamatovat vlastnictví a parametry
  g_busOwnedByEeprom = true;
  g_ownedSDA = sda;
  g_ownedSCL = scl;
  g_ownedFreq = freqHz;

  // Přepnout knihovnu na tuto sběrnici + adresy
  eepromConfigure(w, eepromAddr, usnAddr);
  return true;
}

bool eepromI2CDeinit(String &err)
{
  err = "";
  // Deinit jen pokud jsme bus sami spustili (nechceme vypnout sdílený bus Masteru)
  if (g_busOwnedByEeprom)
  {
    hal_i2c_release(g_bus);
  }
  g_busOwnedByEeprom = false;
  g_ownedSDA = g_ownedSCL = -1;
  g_ownedFreq = 0;
  return true;
}

// ----------------------------------------------------------
// Util
// ----------------------------------------------------------

// Parsovani dvoumistného hex bajtu (napr. "0A", "FF")
bool parseHexByte(const String &hexStr, uint8_t &value)
{
  if (hexStr.length() != 2)
    return false;
  char c1 = hexStr.charAt(0);
  char c2 = hexStr.charAt(1);
  if (!isHexadecimalDigit(c1) || !isHexadecimalDigit(c2))
    return false;

  value = static_cast<uint8_t>(strtol(hexStr.c_str(), nullptr, 16));
  return true;
}

// 7bit I2C adresa z řetězce (bere 0xNN i NN)
static bool parseI2C7bitAddr(const String &s, uint8_t &out)
{
  String t = s;
  t.trim();
  int base = (t.startsWith("0x") || t.startsWith("0X")) ? 16 : 10;
  char *end = nullptr;
  long v = strtol(t.c_str(), &end, base);
  if (end == t.c_str() || *end != '\0')
    return false;
  if (v < 0 || v > 0x7F)
    return false;
  out = static_cast<uint8_t>(v);
  return true;
}

// Parsuje byte adresu (0xNN nebo NN; hex i dec)
static bool parseByteAddr(const String &s, uint8_t &out)
{
  String t = s;
  t.trim();
  if (t.length() == 0)
    return false;

  int base = 10;
  if (t.startsWith("0x") || t.startsWith("0X"))
    base = 16;
  else
  {
    // Pokud obsahuje A-F, ber jako hex i bez 0x (např. "20" chceme brát spíš jako hex?)
    //  -> kvůli kompatibilitě je bezpečnější:
    //     - když uživatel chce hex, ať dá 0x
    //     - bez 0x bereme jako DEC
    base = 10;
  }

  char *end = nullptr;
  long v = strtol(t.c_str(), &end, base);
  if (end == t.c_str() || *end != '\0')
    return false;
  if (v < 0 || v > 255)
    return false;

  out = (uint8_t)v;
  return true;
}

// Pomocna funkce pro kontrolu zapsanych dat do Eeprom
static bool bytesEqual(const uint8_t *a, const uint8_t *b, uint8_t len)
{
  for (uint8_t i = 0; i < len; ++i)
    if (a[i] != b[i])
      return false;
  return true;
}

// ==============================================================
// Eeprom Data
// ==============================================================

// Parsuje příkaz "eeprom-write <adresa> <hex data>" a provede zápis
bool applyWriteEepromData(const String &cmd, String &err)
{
  err = "";

  String remainder = cmd.substring(String("eeprom-write ").length());
  remainder.trim();

  int spaceIndex = remainder.indexOf(' ');
  if (spaceIndex == -1)
  {
    err = F("ESP32 Error: Ocekavan format 'eeprom-write <adresa> <hex data>'");
    return false;
  }

  String addrStr = remainder.substring(0, spaceIndex);
  String dataStr = remainder.substring(spaceIndex + 1);
  dataStr.replace(" ", ""); // odstranění mezer

  uint8_t address;
  if (!parseHexByte(addrStr, address))
  {
    err = F("ESP32 Error: Chybna adresa ocekava se hex bajt (napr. 00, 10, 60).");
    return false;
  }

  if ((address % 8) != 0)
  {
    err = F("ESP32 Error: Chyba: adresa musi byt zarovnana na 8 bajtu.");
    return false;
  }

  if (dataStr.length() == 0 || (dataStr.length() % 2) != 0)
  {
    err = F("Chyba: hex data musi mit sudy pocet znaku.");
    return false;
  }

  int byteCount = dataStr.length() / 2;
  if ((byteCount % 8) != 0)
  {
    err = F("Chyba: pocet bajtu musi byt nasobek 8.");
    return false;
  }

  if ((int)address + byteCount > 256)
  {
    err = F("Chyba: prekroceni pameti EEPROM.");
    return false;
  }

  // HEX -> bytes
  uint8_t buffer[256];
  for (int i = 0; i < byteCount; ++i)
  {
    String byteStr = dataStr.substring(i * 2, i * 2 + 2);
    if (!parseHexByte(byteStr, buffer[i]))
    {
      err = F("Chyba pri prevodu HEX na byte na pozici ");
      err += String(i);
      return false;
    }
  }

  // Zapis
  return writeEepromData(address, buffer, byteCount, err);
}

// Parsuje příkaz "eepromExt-write <adresa> <hex data>" a provede ověřený zápis
// Ext verze - s overovanim zapisu dat do eeprom
bool applyWriteEepromDataExt(const String &cmd, String &err)
{
  err = "";

  String remainder = cmd.substring(String("eepromExt-write ").length());
  remainder.trim();

  int spaceIndex = remainder.indexOf(' ');
  if (spaceIndex == -1)
  {
    err = F("ESP32 Error: Ocekavan format 'eepromExt-write <adresa> <hex data>'");
    return false;
  }

  String addrStr = remainder.substring(0, spaceIndex);
  String dataStr = remainder.substring(spaceIndex + 1);
  dataStr.replace(" ", "");

  uint8_t address;
  if (!parseHexByte(addrStr, address))
  {
    err = F("ESP32 Error: Chybna adresa ocekava se hex bajt (napr. 00, 10, 60).");
    return false;
  }

  // EXT: chceme 16B bloky (zarovnání na 16)
  if ((address % EEPROM_EXT_BLOCK_SIZE) != 0)
  {
    err = F("ESP32 Error: Chyba: adresa musi byt zarovnana na 16 bajtu (00,10,20,30..).");
    return false;
  }

  if (dataStr.length() == 0 || (dataStr.length() % 2) != 0)
  {
    err = F("Chyba: hex data musi mit sudy pocet znaku.");
    return false;
  }

  int byteCount = dataStr.length() / 2;

  // EXT: vyžadujeme násobek 16
  if ((byteCount % EEPROM_EXT_BLOCK_SIZE) != 0)
  {
    err = F("Chyba: pocet bajtu musi byt nasobek 16.");
    return false;
  }

  if ((int)address + byteCount > 256)
  {
    err = F("Chyba: prekroceni pameti EEPROM.");
    return false;
  }

  uint8_t buffer[256];
  for (int i = 0; i < byteCount; ++i)
  {
    String byteStr = dataStr.substring(i * 2, i * 2 + 2);
    if (!parseHexByte(byteStr, buffer[i]))
    {
      err = F("Chyba pri prevodu HEX na byte na pozici ");
      err += String(i);
      return false;
    }
  }

  // Ověřený zápis
  return writeEepromDataExt(address, buffer, (uint8_t)byteCount, err);
}

// Zápis bloků (po 8B) do EEPROM
bool writeEepromData(uint8_t startAddress, const uint8_t *data, uint8_t length, String &errorMessage)
{
  const uint8_t pageSize = 8;

  if (length == 0 || (length % pageSize) != 0)
  {
    errorMessage = F("Delka dat musi byt nasobkem 8 bytu.");
    return false;
  }
  if ((startAddress % pageSize) != 0)
  {
    errorMessage = F("Adresa musi byt zarovnana na 8 bytu (napr. 0x00, 0x08, 0x10).");
    return false;
  }

  for (uint8_t offset = 0; offset < length; offset += pageSize)
  {
    uint8_t currentAddress = startAddress + offset;

    eepromI2C().beginTransmission(g_addrEE);
    eepromI2C().write(currentAddress);

    for (uint8_t i = 0; i < pageSize; ++i)
    {
      eepromI2C().write(data[offset + i]);
    }

    int result = eepromI2C().endTransmission();
    if (result != 0)
    {
      errorMessage = "I2C chyba pri zapisu na adresu 0x" + String(currentAddress, HEX) + " (kod: " + String(result) + ")";
      return false;
    }

    delay(10); // write cycle time
  }

  return true;
}

// EXT: Ověřený zápis po 16B blocích: write -> read -> compare -> retry
// Ext verze - s overovanim zapisu dat do eeprom
bool writeEepromDataExt(uint8_t startAddress, const uint8_t *data, uint8_t length, String &errorMessage)
{
  errorMessage = "";

  if (length == 0 || (length % EEPROM_EXT_BLOCK_SIZE) != 0)
  {
    errorMessage = F("EXT: Delka dat musi byt nasobkem 16 bytu.");
    return false;
  }
  if ((startAddress % EEPROM_EXT_BLOCK_SIZE) != 0)
  {
    errorMessage = F("EXT: Adresa musi byt zarovnana na 16 bytu (0x00,0x10,0x20..).");
    return false;
  }

  uint8_t verifyBuf[EEPROM_EXT_BLOCK_SIZE];

  for (uint8_t offset = 0; offset < length; offset += EEPROM_EXT_BLOCK_SIZE)
  {
    uint8_t addr = startAddress + offset;

    bool okBlock = false;
    for (uint8_t attempt = 0; attempt <= EEPROM_EXT_RETRY_COUNT; ++attempt)
    {
      // 1) zapsat 16B jako 2x 8B (použijeme stávající writeEepromData, aby bylo vše konzistentní)
      String e;
      if (!writeEepromData(addr, &data[offset], EEPROM_EXT_BLOCK_SIZE, e))
      {
        errorMessage = "EXT: write failed at 0x" + String(addr, HEX) + " (" + e + ")";
        // write selhalo I2C chybou -> zkusíme retry (pokud ještě zbývá)
      }
      else
      {
        // 2) přečíst 16B zpět
        String rerr;
        if (!readEepromDataAt(addr, rerr))
        {
          errorMessage = "EXT: readback failed at 0x" + String(addr, HEX) + " (" + rerr + ")";
        }
        else
        {
          // zkopírovat z globálního eepromData do lokálního bufferu
          for (uint8_t i = 0; i < EEPROM_EXT_BLOCK_SIZE; ++i)
            verifyBuf[i] = eepromData[addr + i];

          // 3) porovnat
          if (bytesEqual(&data[offset], verifyBuf, EEPROM_EXT_BLOCK_SIZE))
          {
            okBlock = true;
            break;
          }
          else
          {
            // mismatch -> připrav chybu a retry
            errorMessage = "EXT: verify mismatch at 0x" + String(addr, HEX) + " attempt " + String(attempt + 1);
          }
        }
      }

      // malá pauza před dalším pokusem (nepřetěžovat bus/kontakty)
      delay(2);
    }

    if (!okBlock)
      return false;
  }

  return true;
}

// Cteni cele EEPROM (po 16B)
bool readEepromData(String &errorMessage)
{
  for (int address = 0; address < 256; address += 16)
  {
    // nastavit adresu
    eepromI2C().beginTransmission(g_addrEE);
    eepromI2C().write((uint8_t)address);
    int result = eepromI2C().endTransmission();
    if (result != 0)
    {
      errorMessage = "I2C chyba pri nastaveni adresy 0x" + String(address, HEX) + " (kod: " + String(result) + ")";
      return false;
    }

    // načíst 16 B
    eepromI2C().requestFrom((int)g_addrEE, (int)16);
    for (int i = 0; i < 16 && eepromI2C().available(); ++i)
    {
      eepromData[address + i] = (uint8_t)eepromI2C().read();
    }

    delay(5);
  }
  return true;
}

// Vyrobi textove radky dumpu EEPROM (hlavicka + 16 řádků)
std::vector<String> getEepromData()
{
  const int lineCount = 16; // 256 / 16
  std::vector<String> out;
  out.reserve(1 + lineCount);

  out.emplace_back("OK-16"); // hlavička: počet řádků

  for (int address = 0; address < 256; address += 16)
  {
    String line;
    line.reserve(3 + 2 + 2 + (3 * 16));

    // Doplni na kazdy radek pocatecni cislo adresy: "0x00: "
    // line += "0x";
    // if (address < 16) line += "0";
    // line += String(address, HEX);
    // line += ": ";

    for (int i = 0; i < 16; ++i)
    {
      uint8_t b = eepromData[address + i];
      if (b < 16)
        line += "0";
      line += String(b, HEX);
      line += " ";
    }

    out.emplace_back(std::move(line));
  }
  return out;
}

// Přečte 16B od konkrétní adresy (musí být zarovnaná na 16)
bool readEepromDataAt(uint8_t address, String &errorMessage)
{
  if ((address % 16) != 0)
  {
    errorMessage = F("Chyba: adresa musi byt zarovnana na 16 (0x00,0x10,0x20..).");
    return false;
  }

  // nastavit adresu
  eepromI2C().beginTransmission(g_addrEE);
  eepromI2C().write(address);
  int result = eepromI2C().endTransmission();
  if (result != 0)
  {
    errorMessage = "I2C chyba pri nastaveni adresy 0x" + String(address, HEX) + " (kod: " + String(result) + ")";
    return false;
  }

  // načíst 16 B
  eepromI2C().requestFrom((int)g_addrEE, (int)16);
  for (int i = 0; i < 16 && eepromI2C().available(); ++i)
  {
    eepromData[address + i] = (uint8_t)eepromI2C().read();
  }

  delay(2);
  return true;
}

String formatEepromLineAt(uint8_t address)
{
  String line;
  line.reserve(3 * 16);

  for (int i = 0; i < 16; ++i)
  {
    uint8_t b = eepromData[address + i];
    if (b < 16)
      line += "0";
    line += String(b, HEX);
    line += " ";
  }
  return line;
}

// Precteni USN (16B) z druhe adresy
bool readEepromSerialNumber(String &errorMessage)
{
  eepromI2C().beginTransmission(g_addrUSN);
  eepromI2C().write(0x80); // zacatek USN oblasti
  int result = eepromI2C().endTransmission();
  if (result != 0)
  {
    errorMessage = "Chyba I2C pri nastaveni adresy (kod: " + String(result) + ")";
    return false;
  }

  eepromI2C().requestFrom((int)g_addrUSN, (int)16);
  for (int i = 0; i < 16 && eepromI2C().available(); ++i)
  {
    usnData[i] = (uint8_t)eepromI2C().read();
  }

  delay(2);
  return true;
}

// Vytisteni USN jako 32 hex znaku (bez mezer)
String printEepromSerialNumber()
{
  String line;
  line.reserve(32);
  for (int i = 0; i < 16; ++i)
  {
    if (usnData[i] < 16)
      line += "0";
    line += String(usnData[i], HEX);
  }
  return line;
}

// ----------------------------------------------------------
// NOVÉ: příkaz „eeprom-set-addr <mem> <idn>“
// ----------------------------------------------------------
bool applySetEepromAddrCmd(const String &cmd, String &err)
{
  err = "";
  const char *P1 = "eeprom-set-addr ";
  const char *P2 = "eeprom-set-adr ";

  int start = -1;
  if (cmd.startsWith(P1))
    start = strlen(P1);
  else if (cmd.startsWith(P2))
    start = strlen(P2);
  else
  {
    err = F("Chyba: spatny prikaz.");
    return false;
  }

  String rest = cmd.substring(start);
  rest.trim();
  int sp = rest.indexOf(' ');
  if (sp < 0)
  {
    err = F("Pouzij: eeprom-set-addr <addr_memory> <addr_idn>");
    return false;
  }

  String memStr = rest.substring(0, sp);
  String idnStr = rest.substring(sp + 1);
  idnStr.trim();

  uint8_t aMem = 0, aIdn = 0;
  if (!parseI2C7bitAddr(memStr, aMem))
  {
    err = F("Neplatna adresa pameti.");
    return false;
  }
  if (!parseI2C7bitAddr(idnStr, aIdn))
  {
    err = F("Neplatna adresa identity.");
    return false;
  }
  TwoWire &I2C = eepromI2C();
  // volitelně: rychlé ověření, že zařízení odpovídají
  /*

  I2C.beginTransmission(aMem);
  if (I2C.endTransmission() != 0)
  {
    err = F("Na adrese pameti nic neodpovida.");
    return false;
  }
  I2C.beginTransmission(aIdn);
  if (I2C.endTransmission() != 0)
  {
    err = F("Na adrese identity nic neodpovida.");
    return false;
  }
  */

  // přepnutí konfigurace
  eepromConfigure(I2C, aMem, aIdn);
  return true;
}

// ----------------------------------------------------------
// Dispatcher pro příkazy z UARTu
//  - i2c-eeprom-init
//  - i2c-eeprom-deinit
//  - eeprom-read-data
//  - eeprom-read-sn
//  - eeprom-write ...
//  - eeprom-set-addr ...
// ----------------------------------------------------------
bool Eeprom_ApplyCommand(const String &cmd, std::vector<String> &outLines, String &err)
{
  outLines.clear();
  err = "";

  String s = cmd;
  s.trim();

  // ------------------------------------------------------
  // i2c-eeprom-init <bus> <sda> <scl> <freqHz> <addrEE> <addrUSN>
  //   bus: 0 = Wire, 1 = Wire1
  // ------------------------------------------------------
  if (s.startsWith("i2c-eeprom-init "))
  {
    int bus = 0, sda = 0, scl = 0;
    uint32_t freq = 100000;
    char aEE[16] = {0}, aUSN[16] = {0};

    int n = sscanf(s.c_str(), "i2c-eeprom-init %d %d %d %u %15s %15s",
                   &bus, &sda, &scl, &freq, aEE, aUSN);

    if (n == 6)
    {
      auto parse7 = [](const char *s) -> uint8_t
      {
        int base = (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) ? 16 : 16; // očekáváme hex (např. 0x54/54)
        long v = strtol(s, nullptr, base);
        return (uint8_t)(v & 0x7F);
      };

      uint8_t addrEE = parse7(aEE);
      uint8_t addrUSN = parse7(aUSN);

      TwoWire *W = (bus == 1 ? &Wire1 : &Wire);
      String e;
      if (eepromI2CInitOn(*W, sda, scl, freq, addrEE, addrUSN, e))
      {
        char buf[96];
        snprintf(buf, sizeof(buf),
                 "EEPROM BUS READY (bus=%d SDA=%d SCL=%d %uHz EE=0x%02X USN=0x%02X)",
                 bus, sda, scl, (unsigned)freq, addrEE, addrUSN);

        outLines.push_back("OK-1");
        outLines.push_back(String(buf));
        return true;
      }
      else
      {
        err = e;
        return false;
      }
    }
    else
    {
      err = "Usage: i2c-eeprom-init <bus 0|1> <sda> <scl> <freqHz> <addrEE> <addrUSN>";
      return false;
    }
  }

  // ------------------------------------------------------
  // i2c-eeprom-deinit
  // ------------------------------------------------------
  if (s.equalsIgnoreCase("i2c-eeprom-deinit"))
  {
    String e;
    if (eepromI2CDeinit(e))
    {
      outLines.push_back("OK-1");
      outLines.push_back("EEPROM BUS DEINIT");
      return true;
    }
    else
    {
      err = e;
      return false;
    }
  }

  // ------------------------------------------------------
  // eeprom-read-data
  //   - bez parametru: OK-16 + 16 řádků (celý dump)  [kompatibilní]
  //   - s parametrem:  OK-1  + 1 řádek (16B od adresy)
  //     příklad: eeprom-read-data 0x20
  // ------------------------------------------------------
  if (s.equalsIgnoreCase("eeprom-read-data") || s.startsWith("eeprom-read-data "))
  {
    // varianta bez parametru
    if (s.equalsIgnoreCase("eeprom-read-data"))
    {
      String e;
      if (readEepromData(e))
      {
        outLines = getEepromData();
        return true;
      }
      err = e;
      return false;
    }

    // varianta s adresou
    String rest = s.substring(String("eeprom-read-data ").length());
    rest.trim();

    uint8_t addr = 0;
    if (!parseByteAddr(rest, addr))
    {
      err = F("Pouzij: eeprom-read-data <addr>  (napr. 0x20).");
      return false;
    }

    String e;
    if (readEepromDataAt(addr, e))
    {
      outLines.push_back("OK-1");
      outLines.push_back(formatEepromLineAt(addr));
      return true;
    }
    err = e;
    return false;
  }

  // ------------------------------------------------------
  // eeprom-read-sn
  //  - OK-1 + řetězec se sériovým číslem (32 hex znaků)
  // ------------------------------------------------------
  if (s.equalsIgnoreCase("eeprom-read-sn"))
  {
    String e;
    if (readEepromSerialNumber(e))
    {
      String sn = printEepromSerialNumber();
      outLines.push_back("OK-1");
      outLines.push_back(sn);
      return true;
    }
    else
    {
      err = e;
      return false;
    }
  }

  // ------------------------------------------------------
  // eeprom-write <adresa> <hex data>
  //  - zachovává logiku: na úspěch vrací "OK-0"
  // ------------------------------------------------------
  if (s.startsWith("eeprom-write "))
  {
    String e;
    if (applyWriteEepromData(s, e))
    {
      outLines.push_back("OK-0");
      return true;
    }
    else
    {
      err = e;
      return false;
    }
  }

  // ------------------------------------------------------
  // eepromExt-write <adresa> <hex data>
  //  - ověřený zápis (16B bloky + retry)
  //  - na úspěch vrací "OK-0" (kompatibilní styl)
  // ------------------------------------------------------
  if (s.startsWith("eepromExt-write "))
  {
    String e;
    if (applyWriteEepromDataExt(s, e))
    {
      outLines.push_back("OK-0");
      return true;
    }
    err = e;
    return false;
  }
  
  // ------------------------------------------------------
  // eeprom-set-addr <addr_memory> <addr_idn>
  // ------------------------------------------------------
  if (s.startsWith("eeprom-set-addr ") || s.startsWith("eeprom-set-adr "))
  {
    String e;
    if (applySetEepromAddrCmd(s, e))
    {
      outLines.push_back("OK-1");
      // volitelně můžeš doplnit info řádek, nechal jsem jen OK-1
      return true;
    }
    else
    {
      err = e;
      return false;
    }
  }

  // Nic z výše uvedeného nesedlo
  err = "Unknown EEPROM command";
  return false;
}
