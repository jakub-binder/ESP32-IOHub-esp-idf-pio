// --------------------------------------------------------------------
// TLA2024
// TLA202x Cost-Optimized, Ultra-Small, 12-Bit, System-Monitoring ADCs
// --------------------------------------------------------------------
// basic-scanfil-safety-adc-tla2024_Step1.h
// Step1_Command_Prototype
//
// Sketch with commands:
/*
====================================
TLA2024:READ 0
TLA2024:READ 1
TLA2024:READALL
TLA2024:REG 00
TLA2024:REG 01
====================================
*/
// Answ:  OK-N
// Fails: ERR:...
// 
// -------------------------------------------------------------------
// MUX = 100 measures AIN0 against GND and MUX = 101 measures AIN1 against GND.
// Conversion register contains 12-bit result left-aligned in 16-bit register.
// Firmware shifts it right by 4 and returns raw 12-bit value.
//
// -------------------------------------------------------------------
/*
====================================
pio run -t clean -e esp32wroom32
pio run -e esp32wroom32
pio run -t upload -e esp32wroom32
pio device monitor
pio device monitor -p COM15
====================================
*/
// -------------------------------------------------------------------

#include <Arduino.h>
#include <Wire.h>

// =========================
// TLA2024 / ESP32 settings
// =========================
#define TLA2024_ADDR 0x48 // ADDR = GND -> 1001000b

#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define I2C_FREQ 100000

// TLA2024 registers
#define REG_CONVERSION 0x00
#define REG_CONFIG 0x01

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

bool parseHex8(const String &s, uint8_t &value)
{
  String t = trimCopy(s);
  if (t.length() != 2)
    return false;

  int hi = hexNibble(t[0]);
  int lo = hexNibble(t[1]);
  if (hi < 0 || lo < 0)
    return false;

  value = (uint8_t)((hi << 4) | lo);
  return true;
}

bool parseChannel(const String &s, uint8_t &channel)
{
  String t = trimCopy(s);

  if (t == "0")
  {
    channel = 0;
    return true;
  }

  if (t == "1")
  {
    channel = 1;
    return true;
  }

  return false;
}

// =========================
// I2C register access
// =========================
bool tlaWriteRegister(uint8_t reg, uint16_t value)
{
  Wire.beginTransmission(TLA2024_ADDR);
  Wire.write(reg);
  Wire.write((uint8_t)(value >> 8));   // MSB
  Wire.write((uint8_t)(value & 0xFF)); // LSB

  return (Wire.endTransmission() == 0);
}

bool tlaReadRegister(uint8_t reg, uint16_t &value)
{
  Wire.beginTransmission(TLA2024_ADDR);
  Wire.write(reg);

  if (Wire.endTransmission(false) != 0)
    return false;

  if (Wire.requestFrom((int)TLA2024_ADDR, 2) != 2)
    return false;

  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();

  value = ((uint16_t)msb << 8) | lsb;
  return true;
}

// =========================
// ADC conversion
// =========================
uint16_t buildConfigForChannel(uint8_t channel)
{
  uint16_t config = 0;

  config |= (1 << 15); // OS = 1, start single conversion

  // MUX single-ended
  if (channel == 0)
  {
    config |= (0b100 << 12); // AIN0 vs GND
  }
  else
  {
    config |= (0b101 << 12); // AIN1 vs GND
  }

  config |= (0b001 << 9); // PGA = ±4.096 V
  config |= (1 << 8);     // MODE = single-shot
  config |= (0b100 << 5); // DR = 1600 SPS
  config |= 0x0003;       // reserved bits = 03h

  return config;
}

bool tlaReadChannelRaw(uint8_t channel, int16_t &raw12)
{
  if (channel > 1)
    return false;

  uint16_t config = buildConfigForChannel(channel);

  if (!tlaWriteRegister(REG_CONFIG, config))
    return false;

  delay(2); // 1600 SPS -> conversion time cca 0.625 ms

  uint16_t raw16 = 0;
  if (!tlaReadRegister(REG_CONVERSION, raw16))
    return false;

  // TLA2024 result is 12-bit left-aligned in 16-bit register
  raw12 = (int16_t)raw16 >> 4;

  return true;
}

// =========================
// Protocol handlers
// =========================
void printHelp()
{
  Serial.println("OK-6");
  Serial.println("Commands:");
  Serial.println("TLA2024:READ 0");
  Serial.println("TLA2024:READ 1");
  Serial.println("TLA2024:READALL");
  Serial.println("TLA2024:REG 00");
  Serial.println("TLA2024:REG 01");
}

void handleRead(const String &args)
{
  uint8_t channel = 0;

  if (!parseChannel(args, channel))
  {
    Serial.println("ERR:READ:CHANNEL");
    return;
  }

  int16_t raw = 0;

  if (!tlaReadChannelRaw(channel, raw))
  {
    Serial.println("ERR:READ:I2C");
    return;
  }

  Serial.println("OK-1");
  Serial.println(raw);
}

void handleReadAll()
{
  int16_t ch0 = 0;
  int16_t ch1 = 0;

  if (!tlaReadChannelRaw(0, ch0))
  {
    Serial.println("ERR:READALL:CH0");
    return;
  }

  if (!tlaReadChannelRaw(1, ch1))
  {
    Serial.println("ERR:READALL:CH1");
    return;
  }

  Serial.println("OK-2");
  Serial.println(ch0);
  Serial.println(ch1);
}

void handleReg(const String &args)
{
  uint8_t reg = 0;

  if (!parseHex8(args, reg))
  {
    Serial.println("ERR:REG:ADDR");
    return;
  }

  if (reg != REG_CONVERSION && reg != REG_CONFIG)
  {
    Serial.println("ERR:REG:UNSUPPORTED");
    return;
  }

  uint16_t value = 0;

  if (!tlaReadRegister(reg, value))
  {
    Serial.println("ERR:REG:I2C");
    return;
  }

  Serial.println("OK-1");
  Serial.printf("%04X\n", value);
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

  if (line.startsWith("TLA2024:READ "))
  {
    handleRead(line.substring(String("TLA2024:READ ").length()));
    return;
  }

  if (line.equalsIgnoreCase("TLA2024:READALL"))
  {
    handleReadAll();
    return;
  }

  if (line.startsWith("TLA2024:REG "))
  {
    handleReg(line.substring(String("TLA2024:REG ").length()));
    return;
  }

  Serial.println("ERR:UNKNOWN");
}

// =========================
// Arduino setup / loop
// =========================
void setup()
{
  Serial.begin(115200);
  delay(500);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);

  uint16_t config = 0;
  if (!tlaReadRegister(REG_CONFIG, config))
  {
    Serial.println("ERR:INIT:I2C");
    return;
  }

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

    if (g_line.length() < 128)
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