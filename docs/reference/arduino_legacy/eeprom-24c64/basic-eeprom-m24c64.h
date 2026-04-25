// basic-scanfil-safety-eeprom-m24c64_Step3.h
#include <Arduino.h>
#include <Wire.h>

// =========================
// M24C64 / ESP32 settings
// =========================
#define EEPROM_I2C_ADDR 0x53 // 7-bit I2C address
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define I2C_FREQ 100000

#define EEPROM_PAGE_SIZE 32
#define EEPROM_SIZE 8192 // 64 kbit = 8 kB

// =========================
// Serial command settings
// =========================
static String g_line;

// =========================
// Utility
// =========================
String trimCopy(const String &s)
{
  String t = s;
  t.trim();
  return t;
}

bool isHexChar(char c)
{
  return ((c >= '0' && c <= '9') ||
          (c >= 'A' && c <= 'F') ||
          (c >= 'a' && c <= 'f'));
}

int hexNibble(char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return -1;
}

bool parseHex16(const String &s, uint16_t &value)
{
  String t = trimCopy(s);
  if (t.length() != 4)
    return false;

  uint16_t v = 0;
  for (size_t i = 0; i < 4; i++)
  {
    int n = hexNibble(t[i]);
    if (n < 0)
      return false;
    v = (uint16_t)((v << 4) | (uint16_t)n);
  }

  value = v;
  return true;
}

bool parseDecSize(const String &s, size_t &value)
{
  String t = trimCopy(s);
  if (t.length() == 0)
    return false;

  for (size_t i = 0; i < t.length(); i++)
  {
    if (t[i] < '0' || t[i] > '9')
      return false;
  }

  value = (size_t)t.toInt();
  return true;
}

bool hexStringToBytes(const String &hex, uint8_t *out, size_t outLen)
{
  if (hex.length() != outLen * 2)
    return false;

  for (size_t i = 0; i < outLen; i++)
  {
    int hi = hexNibble(hex[2 * i]);
    int lo = hexNibble(hex[2 * i + 1]);
    if (hi < 0 || lo < 0)
      return false;
    out[i] = (uint8_t)((hi << 4) | lo);
  }

  return true;
}

String bytesToHexString(const uint8_t *data, size_t len)
{
  const char *hex = "0123456789ABCDEF";
  String out;
  out.reserve(len * 2);

  for (size_t i = 0; i < len; i++)
  {
    out += hex[(data[i] >> 4) & 0x0F];
    out += hex[data[i] & 0x0F];
  }

  return out;
}

// =========================
// EEPROM low-level
// =========================
bool eepromWaitReady(uint32_t timeoutMs = 20)
{
  uint32_t start = millis();

  while ((millis() - start) < timeoutMs)
  {
    Wire.beginTransmission(EEPROM_I2C_ADDR);
    uint8_t err = Wire.endTransmission();
    if (err == 0)
    {
      return true;
    }
    delay(1);
  }

  return false;
}

bool eepromReadBytes(uint16_t memAddr, uint8_t *buffer, size_t length)
{
  if (buffer == nullptr)
    return false;
  if ((uint32_t)memAddr + length > EEPROM_SIZE)
    return false;

  size_t offset = 0;

  while (offset < length)
  {
    size_t chunk = length - offset;
    if (chunk > 28)
      chunk = 28;

    Wire.beginTransmission(EEPROM_I2C_ADDR);
    Wire.write((uint8_t)((memAddr + offset) >> 8));
    Wire.write((uint8_t)((memAddr + offset) & 0xFF));

    if (Wire.endTransmission(false) != 0)
    {
      return false;
    }

    size_t received = Wire.requestFrom((int)EEPROM_I2C_ADDR, (int)chunk);
    if (received != chunk)
    {
      return false;
    }

    for (size_t i = 0; i < chunk; i++)
    {
      buffer[offset + i] = Wire.read();
    }

    offset += chunk;
  }

  return true;
}

bool eepromWriteChunk(uint16_t memAddr, const uint8_t *data, size_t length)
{
  if (data == nullptr)
    return false;
  if (length == 0)
    return true;
  if ((uint32_t)memAddr + length > EEPROM_SIZE)
    return false;

  Wire.beginTransmission(EEPROM_I2C_ADDR);
  Wire.write((uint8_t)(memAddr >> 8));
  Wire.write((uint8_t)(memAddr & 0xFF));

  for (size_t i = 0; i < length; i++)
  {
    Wire.write(data[i]);
  }

  if (Wire.endTransmission() != 0)
  {
    return false;
  }

  return eepromWaitReady();
}

// Write arbitrary-length data, split across page boundaries automatically
bool eepromWriteBytes(uint16_t memAddr, const uint8_t *data, size_t length)
{
  if (data == nullptr)
    return false;
  if ((uint32_t)memAddr + length > EEPROM_SIZE)
    return false;

  size_t offset = 0;

  while (offset < length)
  {
    uint16_t curAddr = (uint16_t)(memAddr + offset);
    size_t pageOffset = curAddr % EEPROM_PAGE_SIZE;
    size_t pageRemain = EEPROM_PAGE_SIZE - pageOffset;
    size_t chunk = length - offset;

    if (chunk > pageRemain)
    {
      chunk = pageRemain;
    }

    if (!eepromWriteChunk(curAddr, &data[offset], chunk))
    {
      return false;
    }

    offset += chunk;
  }

  return true;
}

// =========================
// Dump helper
// =========================
void printHexDump256(const uint8_t *data)
{
  for (size_t i = 0; i < 256; i += 16)
  {
    //Serial.printf("%04X: ", (unsigned int)i);
    for (size_t j = 0; j < 16; j++)
    {
      Serial.printf("%02X", data[i + j]);
      if (j < 15)
        Serial.print(" ");
    }

    Serial.println();
  }
}

// =========================
// Protocol handlers
// =========================
void printHelp()
{
  Serial.println("OK-6");
  Serial.println("Commands:");
  Serial.println("M24C64:READ <addr>,<len>");
  Serial.println("M24C64:WRITE <addr>,<len>,<data>");
  Serial.println("M24C64:DUMP");
  Serial.println("addr=4 hex digits, len=decimal");
  Serial.println("data=hex string without spaces");
}

void handleRead(const String &args)
{
  int comma = args.indexOf(',');
  if (comma < 0)
  {
    Serial.println("ERR:READ:SYNTAX");
    return;
  }

  String addrStr = trimCopy(args.substring(0, comma));
  String lenStr = trimCopy(args.substring(comma + 1));

  uint16_t addr = 0;
  size_t len = 0;

  if (!parseHex16(addrStr, addr))
  {
    Serial.println("ERR:READ:ADDR");
    return;
  }

  if (!parseDecSize(lenStr, len) || len == 0)
  {
    Serial.println("ERR:READ:LEN");
    return;
  }

  if ((uint32_t)addr + len > EEPROM_SIZE)
  {
    Serial.println("ERR:READ:RANGE");
    return;
  }

  uint8_t *buffer = new uint8_t[len];
  if (!buffer)
  {
    Serial.println("ERR:READ:NOMEM");
    return;
  }

  bool ok = eepromReadBytes(addr, buffer, len);
  if (!ok)
  {
    delete[] buffer;
    Serial.println("ERR:READ:I2C");
    return;
  }

  String hex = bytesToHexString(buffer, len);
  delete[] buffer;

  Serial.println("OK-1");
  Serial.println(hex);
}

void handleWrite(const String &args)
{
  int c1 = args.indexOf(',');
  if (c1 < 0)
  {
    Serial.println("ERR:WRITE:SYNTAX");
    return;
  }

  int c2 = args.indexOf(',', c1 + 1);
  if (c2 < 0)
  {
    Serial.println("ERR:WRITE:SYNTAX");
    return;
  }

  String addrStr = trimCopy(args.substring(0, c1));
  String lenStr = trimCopy(args.substring(c1 + 1, c2));
  String dataStr = trimCopy(args.substring(c2 + 1));

  uint16_t addr = 0;
  size_t len = 0;

  if (!parseHex16(addrStr, addr))
  {
    Serial.println("ERR:WRITE:ADDR");
    return;
  }

  if (!parseDecSize(lenStr, len) || len == 0)
  {
    Serial.println("ERR:WRITE:LEN");
    return;
  }

  if ((uint32_t)addr + len > EEPROM_SIZE)
  {
    Serial.println("ERR:WRITE:RANGE");
    return;
  }

  if (dataStr.length() != len * 2)
  {
    Serial.println("ERR:WRITE:DATALEN");
    return;
  }

  for (size_t i = 0; i < dataStr.length(); i++)
  {
    if (!isHexChar(dataStr[i]))
    {
      Serial.println("ERR:WRITE:DATAHEX");
      return;
    }
  }

  uint8_t *buffer = new uint8_t[len];
  if (!buffer)
  {
    Serial.println("ERR:WRITE:NOMEM");
    return;
  }

  if (!hexStringToBytes(dataStr, buffer, len))
  {
    delete[] buffer;
    Serial.println("ERR:WRITE:DATA");
    return;
  }

  bool ok = eepromWriteBytes(addr, buffer, len);
  delete[] buffer;

  if (!ok)
  {
    Serial.println("ERR:WRITE:I2C");
    return;
  }

  Serial.println("OK-0");
}

void handleDump()
{
  uint8_t buffer[256];

  if (!eepromReadBytes(0x0000, buffer, sizeof(buffer)))
  {
    Serial.println("ERR:DUMP:I2C");
    return;
  }

  Serial.println("OK-16");
  printHexDump256(buffer);
}

void handleCommand(String line)
{
  line.trim();
  if (line.length() == 0)
    return;

  if (line.equalsIgnoreCase("HELP") || line.equalsIgnoreCase("?"))
  {
    printHelp();
    return;
  }

  if (line.startsWith("M24C64:READ "))
  {
    handleRead(line.substring(String("M24C64:READ ").length()));
    return;
  }

  if (line.startsWith("M24C64:WRITE "))
  {
    handleWrite(line.substring(String("M24C64:WRITE ").length()));
    return;
  }

  if (line.equalsIgnoreCase("M24C64:DUMP"))
  {
    handleDump();
    return;
  }

  Serial.println("ERR:UNKNOWN");
}

// =========================
// Arduino setup/loop
// =========================
void setup()
{
  Serial.begin(115200);
  delay(500);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);

  Serial.println("OK-0");
}

void loop()
{
  while (Serial.available() > 0)
  {
    char ch = (char)Serial.read();

    if (ch == '\r')
    {
      continue;
    }

    if (ch == '\n')
    {
      handleCommand(g_line);
      g_line = "";
      continue;
    }

    if (g_line.length() < 512)
    {
      g_line += ch;
    }
    else
    {
      g_line = "";
      Serial.println("ERR:LINE:TOOLONG");
    }
  }
}