// ===============================================||
// ESP32_Commands:
// -----------------------------------------------||
// init                                           ||
// firmware                                       ||
// restart                                        ||
// -----------------------------------------------||
//
// ------------------ Gpio-Debug ---- ------------||
// set-gpio 4 in                                  || 2,4,5,12,13,14,15,16,17,18,19,21,22,23,25,26,27,32,33 (16,17 => Uart RS232)
// set-gpio 4 in pulldown                         ||
// set-gpio 4 in pullup                           ||
//                                                ||
// set-gpio 21 out high                           ||
// set-gpio 21 out low                            ||
// set-gpio 22 out high                           ||
// set-gpio 22 out low                            ||
//                                                ||
// set-gpio all hi-z                              ||
//                                                ||
// get-gpio 4                                     ||
//                                                ||
// ------------------ xlib-Eeprom.h --------------||
//    esp32-3s:                                   ||
//                                                ||
// i2c-eeprom-init 0 16 15 100000 0x56 0x5E       || Wire (bus=0), I2C2 (26,25), i2c-eeprom-init <bus> <sda> <scl> <freqHz> <addrEE> <addrUSN>
// i2c-eeprom-init 0 16 15 100000 0x51 0x59       || Printer
//    esp32-wroom32                               ||
// i2c-eeprom-init 0 21 22 100000 0x56 0x5E       || Printhead
// i2c-eeprom-init 0 21 22 100000 0x51 0x59       || Printer
// i2c-eeprom-deinit                              ||
// eeprom-read-data                               ||
// eeprom-read-sn                                 ||
// eeprom-read-data <address>                     || eeprom-read-data 0x20
// eeprom-write <address> <hex data>              ||
// eepromExt-write <address> <hex data>           || eeprom-write 10 AABBCCDDEEFFGGHH
// -----------------------------------------------||
//
// ----------------- xlib-Temp-LM75BDP.h  --------||
//    esp32-3s:                                   ||
// temp-init 0 16 15 100000 0x48                  || temp-init <0-Wire,1-Wire1> <SDA> <SCL> <FRQ> <ADD 7-bit>
//    esp32-wroom32                               ||
// temp-init 0 21 22 100000 0x48                  ||
// temp-deinit                                    ||
// temp-read-data                                 || return: 25.375
// temp-set-treshold <Thyst> <Tos>                || temp-set-treshold 19.0 20.0  |  temp-set-treshold 29.0 30.0
// -----------------------------------------------||
//
// ----------------- IO-ADS7961-DAC7564-SPI ------||
// dac-help                                       ||
// dac-init                                       ||
// dac-deinit                                     ||
// dac-vouta 0.25 dac-vouta 0.5 dac-vouta 0.75    ||
// dac-vouta 0.25 dac-voutb 0.5 dac-voutb 0.75    ||
// dac-vouta 0.25 dac-voutc 0.5 dac-voutc 0.75    ||
// dac-vouta 0.25 dac-voutd 0.5 dac-voutd 0.75    ||
// dac-vouta-v 0.9                                ||
// dac-voutb-v 0.9                                ||
// dac-voutc-v 0.9                                ||
// dac-voutd-v 0.9                                ||
// -----------------------------------------------||
// adc-help                                       ||
// adc-init                                       ||
// adc-init 18 19 23 5                            || adc-init <sck> <miso> <mosi> <csAdc> [spiHz] [csGuard]
// adc-init 18 19 23 5 100000                     ||
// adc-init 18 19 23 5 100000 15                  ||
// adc-deinit                                     ||
//                                                ||
// adc-read-ch0  (MTR1-TEMP)                      ||
// adc-read-ch1  (MTR2-TEMP)                      ||
// adc-read-ch2  (MTR3-TEMP)                      ||
// adc-read-ch3  (MTR4-TEMP)                      ||
// adc-read-ch3  (PHD-DOT-CAL-EN)                 ||
// adc-read-ch5  (NetJ11_10)                      ||
// adc-read-ch6  (PHD-TEMP)                       ||
// adc-read-ch7   (GND)                           ||
// adc-read-ch8  (PH-VOLTS)                       ||
// adc-read-ch9  (+3V3F)                          ||
// adc-read-ch10  (+5VF)                          ||
// adc-read-ch11  ()                              ||
// adc-read-ch12  ()                              ||
// adc-read-ch13  ()                              ||
// adc-read-ch14  ()                              ||
// adc-read-ch15  (GND)                           ||
//                                                ||
// adc-read-ch0                                   || adc-read-chX
// adc-readavg-ch0                                || adc-readavg-chX
// adc-scan-0-3                                   ||
// adc-scan-9-13                                  ||
// adc-diag-format-ch0-200                        || adc-diag-format-chX-N
// adc-diag-gpio-200                              || adc-diag-gpio-N
// -----------------------------------------------||




#include <Arduino.h>
#include "fixture_default.h"
#include <Wire.h>
#include <hal/hal_log.h>
#include <hal/hal_i2c.h>
#include <Esp32-sys/xlib-esp32-sys.h>
#include <Eeprom-24CS02/xlib-Eeprom-24CS02.h>
#include <Temp-LM75BDP/xlib-Temp-LM75BDP.h>
#include <IO-ADS7961-DAC7564-SPI/io_ads7961_dac7564_spi.h>
#include <Gpio-Debug/gpio_debug.h>
#include <board_pins.h>
#include <serial_routing.h>

// --- Nastavení chování ---
static constexpr uint32_t USB_BAUD = 115200;
static constexpr uint32_t UART1_BAUD = 115200;



static void blink(int pin, uint8_t times, uint16_t onMs = 80, uint16_t offMs = 120)
{
  for (uint8_t i = 0; i < times; i++)
  {
    digitalWrite(pin, HIGH);
    delay(onMs);
    digitalWrite(pin, LOW);
    delay(offMs);
  }
}

// ===========================================================
// Serial Setup
// ===========================================================
static Stream &cmdPort = serialCommands();
static Stream &flashPort = serialCommandsFlash();

// Info
const char *cmdPortName =
#if defined(APP_MODE_DEBUG)
  "Serial(USB)";
#else
  "Serial1 (16,17)";
#endif
const char *firmwareVersion = "ESP32 - " FW_BOARD_NAME " - Build:2025-12-10";
// const char* cmdPortName = "Serial1 (16,17)";
// const char* cmdPortName = "Serial2 (33,27)";

String inputCommand = "";
String inputCommandFlash = "";

static IoAds7961Dac7564Spi::Pins g_ioSpiPins(VSPI_SCK, VSPI_MISO, VSPI_MOSI, HSPI_SS, VSPI_SS);
static IoAds7961Dac7564Spi g_ioSpi(g_ioSpiPins, SPI);

void handleCommand(const String &cmd);
void handleCommandFlash(const String &cmd);

//==========================================================
// Init, Error, SETUP, LOOP
//==========================================================
static const SysCommandConfig kSysConfig = {firmwareVersion, "ESP32-INIT"};
static const SysCommandConfig kSysFlashConfig = {firmwareVersion, "ESP32-INIT (USB-CDC)"};

void printError(const String &errorMessage)
{
  cmdPort.println("ERR-1");
  cmdPort.println(errorMessage.c_str());
}

static void printFail(const String &failMessage)
{
  String out = "Error: " + failMessage;
  cmdPort.println("OK-1");
  cmdPort.println(out.c_str());
}

static void printFailFlash(const String &failMessage)
{
  String out = "Error: " + failMessage;
  flashPort.println("OK-1");
  flashPort.println(out.c_str());
}

void FixtureDefault::setup()
{
  if (DIAG1 >= 0)
  {
    pinMode(DIAG1, OUTPUT);
    digitalWrite(DIAG1, LOW);
  }
  if (DIAG2 >= 0)
  {
    pinMode(DIAG2, OUTPUT);
    digitalWrite(DIAG2, LOW);
  }

  Serial.begin(USB_BAUD);
  delay(200); // krátká stabilizace USB CDC
  Serial1.begin(UART1_BAUD, SERIAL_8N1, UART1_RX, UART1_TX);

  // Info
  Serial.println(cmdPortName);
  Serial1.println(cmdPortName);

  // Safe default for debug/ICT/FCT: put debug GPIOs to HI-Z.
  // UART1 remains untouched by Gpio-Debug protected-pin policy.
  GpioDebug_SetAllHiZ();

  // ----------- I2Cs
  // ukonci pripadné stare instance
  Wire.end();
  Wire1.end();
  (void)hal_i2c(); // shared HAL accessor (no behavior change)

}

void FixtureDefault::loop()
{
  while (flashPort.available() > 0)
  {
    char c = flashPort.read();

    if (c == '\n' || c == '\r')
    {
      if (inputCommandFlash.length() > 0)
      {
        handleCommandFlash(inputCommandFlash);
        inputCommandFlash = "";
      }
    }
    else
    {
      inputCommandFlash += c;
    }
  }

  while (cmdPort.available() > 0)
  {
    char c = cmdPort.read();

    if (c == '\n' || c == '\r')
    {
      if (inputCommand.length() > 0)
      {
        handleCommand(inputCommand);
        inputCommand = "";
      }
    }
    else
    {
      inputCommand += c;
    }
  }

  // Heartbeat bez dlouhých blokujících delay
  static uint32_t lastBeat = 0;
  if (millis() - lastBeat >= 1000)
  {
    lastBeat = millis();
    if (DIAG1 >= 0)
    {
      digitalWrite(DIAG1, !digitalRead(DIAG1));
    }

  }
}

//==========================================================
// HELP FUNCTIONS
//==========================================================
void handleCommand(const String &cmd)
{
  // ---------------------------------------------------------
  // INIT
  // --------------------------------------------------------
  String err;
  if (Sys_ApplyCommand(cmd, cmdPort, kSysConfig, err))
  {
    return;
  }
  // ---------------------------------------------------------
  // EEPROM
  // --------------------------------------------------------
  // i2c-eeprom-init <bus> <sda> <scl> <freqHz> <addrEE> <addrUSN>
  //   bus: 0 = Wire, 1 = Wire1
  else if (cmd.startsWith("i2c-eeprom-") || cmd.startsWith("eeprom-"))
  {
    std::vector<String> lines;
    if (Eeprom_ApplyCommand(cmd, lines, err))
    {
      // EEPROM knihovna si do lines dává rovnou "OK-1"/"OK-0"/"OK-16" atd.
      for (size_t i = 0; i < lines.size(); ++i)
      {
        cmdPort.println(lines[i].c_str());
      }
    }
    else
    {
      // zachováme chování: ERR-1 + "Error: ..."
      printFail(err);
    }
  }
  // ---------------------------------------------------------
  // TEMP SENSOR LM75BDP
  // ---------------------------------------------------------
  else if (cmd.startsWith("temp-"))
  {
    String out, err;
    if (TempLM75_ApplyCommand(cmd, out, err))
    {
      cmdPort.println("OK-1");
      if (out.length() > 0)
        cmdPort.println(out.c_str());
    }
    else
    {
      printFail(err); // už máš hotovou funkci
    }
  }
  // ---------------------------------------------------------
  // GPIO DEBUG (SET-GPIO / GET-GPIO)
  // ---------------------------------------------------------
  else if (cmd.startsWith("set-gpio") || cmd.startsWith("SET-GPIO") ||
           cmd.startsWith("get-gpio") || cmd.startsWith("GET-GPIO"))
  {
    std::vector<String> lines;
    if (GpioDebug_ApplyCommand(cmd, lines, err))
    {
      for (size_t i = 0; i < lines.size(); ++i)
      {
        cmdPort.println(lines[i].c_str());
      }
    }
    else
    {
      printFail(err);
    }
  }
  // ---------------------------------------------------------
  // IO-ADS7961-DAC7564-SPI (shared SPI module)
  // ---------------------------------------------------------
  else if (cmd.startsWith("dac-") || cmd.startsWith("DAC-") || cmd.startsWith("adc-") || cmd.startsWith("ADC-"))
  {
    std::vector<String> lines;
    if (g_ioSpi.ApplyCommand(cmd, lines, err))
    {
      for (size_t i = 0; i < lines.size(); ++i)
      {
        cmdPort.println(lines[i].c_str());
      }
    }
    else
    {
      printFail(err);
    }
  }
  // --------------------------------------------------------
  else
  {
    String err = "Neznamy prikaz: " + cmd;
    printError(err);
  }
}

void handleCommandFlash(const String &cmd)
{
  // ---------------------------------------------------------
  // INIT
  // --------------------------------------------------------
  String err;
  if (Sys_ApplyCommand(cmd, flashPort, kSysFlashConfig, err))
  {
    return;
  }
  // ---------------------------------------------------------
  // EEPROM
  // --------------------------------------------------------
  // i2c-eeprom-init <bus> <sda> <scl> <freqHz> <addrEE> <addrUSN>
  //   bus: 0 = Wire, 1 = Wire1
  else if (cmd.startsWith("i2c-eeprom-") || cmd.startsWith("eeprom-"))
  {
    std::vector<String> lines;
    if (Eeprom_ApplyCommand(cmd, lines, err))
    {
      // EEPROM knihovna si do lines dává rovnou "OK-1"/"OK-0"/"OK-16" atd.
      for (size_t i = 0; i < lines.size(); ++i)
      {
        flashPort.println(lines[i].c_str());
      }
    }
    else
    {
      // zachováme chování: ERR-1 + "Error: ..."
      printFailFlash(err);
    }
  }
  // ---------------------------------------------------------
  // GPIO DEBUG (SET-GPIO / GET-GPIO)
  // ---------------------------------------------------------
  else if (cmd.startsWith("set-gpio") || cmd.startsWith("SET-GPIO") ||
           cmd.startsWith("get-gpio") || cmd.startsWith("GET-GPIO"))
  {
    std::vector<String> lines;
    if (GpioDebug_ApplyCommand(cmd, lines, err))
    {
      for (size_t i = 0; i < lines.size(); ++i)
      {
        flashPort.println(lines[i].c_str());
      }
    }
    else
    {
      printFailFlash(err);
    }
  }
  // ---------------------------------------------------------
  // IO-ADS7961-DAC7564-SPI (shared SPI module)
  // ---------------------------------------------------------
  else if (cmd.startsWith("dac-") || cmd.startsWith("DAC-") || cmd.startsWith("adc-") || cmd.startsWith("ADC-"))
  {
    std::vector<String> lines;
    if (g_ioSpi.ApplyCommand(cmd, lines, err))
    {
      for (size_t i = 0; i < lines.size(); ++i)
      {
        flashPort.println(lines[i].c_str());
      }
    }
    else
    {
      printFailFlash(err);
    }
  }
  // --------------------------------------------------------
  // Error
  // --------------------------------------------------------
  else
  {
    String err = "Neznamy prikaz: " + cmd;
    printFailFlash(err);
  }
}
