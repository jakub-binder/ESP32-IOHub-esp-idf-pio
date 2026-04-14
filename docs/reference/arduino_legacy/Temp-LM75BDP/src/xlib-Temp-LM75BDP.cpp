// xlib-Temp-LM75BDP.cpp

#include <Arduino.h>
#include <math.h>
#include <stdlib.h> // strtol, strtoul, strtof
#include <Temp-LM75BDP/xlib-Temp-LM75BDP.h>
#include <hal/hal_i2c.h>

// ----------------------------------------------------------
// HW / LM75B definice
// ----------------------------------------------------------

// Výchozí konfigurace pro IOHub
static const uint8_t TEMP_SDA_PIN_DEFAULT = 21;
static const uint8_t TEMP_SCL_PIN_DEFAULT = 22;
static const uint32_t TEMP_I2C_FREQ_DEFAULT = 100000; // 100 kHz

// výchozí adresa LM75B (7-bit)
static const uint8_t LM75B_ADDR_DEFAULT = 0x48;

// LM75B registry
static const uint8_t LM75B_REG_TEMP = 0x00;
static const uint8_t LM75B_REG_CONF = 0x01;
static const uint8_t LM75B_REG_THYST = 0x02;
static const uint8_t LM75B_REG_TOS = 0x03;

namespace
{
struct TempLm75State
{
    // aktuálně zvolený I2C bus (0=Wire, 1=Wire1)
    // výchozí: bus 1 – kvůli zpětné kompatibilitě
    uint8_t bus = 1;

    // aktuální konfigurace I2C / adresy (lze změnit přes "temp-init ..." s parametry)
    uint8_t sdaPin = TEMP_SDA_PIN_DEFAULT;
    uint8_t sclPin = TEMP_SCL_PIN_DEFAULT;
    uint32_t i2cFreq = TEMP_I2C_FREQ_DEFAULT;
    uint8_t addr = LM75B_ADDR_DEFAULT;

    // vnitřní stav – zda je I2C pro teploměr inicializovaný
    bool initialized = false;
    bool busOwned = false;
};

static TempLm75State g_temp;

static TwoWire &tempI2C()
{
    return hal_i2c(g_temp.bus);
}
} // namespace

// ----------------------------------------------------------
// Nízká vrstva I2C
// ----------------------------------------------------------

static bool lm75b_write16(uint8_t reg, uint16_t value)
{
    tempI2C().beginTransmission(g_temp.addr);
    tempI2C().write(reg);
    tempI2C().write((uint8_t)(value >> 8));   // MSB
    tempI2C().write((uint8_t)(value & 0xFF)); // LSB
    return (tempI2C().endTransmission() == 0);
}

static bool lm75b_read16(uint8_t reg, uint16_t &value)
{
    tempI2C().beginTransmission(g_temp.addr);
    tempI2C().write(reg);
    if (tempI2C().endTransmission(false) != 0) // repeated start
    {
        return false;
    }

    const uint8_t bytesToRead = 2;
    uint8_t n = tempI2C().requestFrom((int)g_temp.addr, (int)bytesToRead);
    if (n != bytesToRead)
    {
        return false;
    }

    uint8_t msb = tempI2C().read();
    uint8_t lsb = tempI2C().read();
    value = ((uint16_t)msb << 8) | lsb;
    return true;
}

static bool lm75b_read8(uint8_t reg, uint8_t &value)
{
    tempI2C().beginTransmission(g_temp.addr);
    tempI2C().write(reg);
    if (tempI2C().endTransmission(false) != 0)
    {
        return false;
    }

    uint8_t n = tempI2C().requestFrom((int)g_temp.addr, 1);
    if (n != 1)
    {
        return false;
    }

    value = tempI2C().read();
    return true;
}

// ----------------------------------------------------------
// High-level LM75B API
// ----------------------------------------------------------

// čtení teploty (°C), 11bit, 0.125 °C / LSB
static bool lm75b_readTemperatureC(float &outTemp)
{
    uint16_t raw16;
    if (!lm75b_read16(LM75B_REG_TEMP, raw16))
    {
        return false;
    }

    int16_t tempRaw = (int16_t)raw16;
    tempRaw >>= 5; // zachová sign bit

    outTemp = (float)tempRaw * 0.125f;
    return true;
}

// Převod °C na 16bit hodnotu pro registry Tos/Thyst (0.5 °C / krok)
static uint16_t lm75b_encodeThreshold(float tempC)
{
    float steps = tempC * 2.0f;           // 0.5 °C -> 1 step
    int16_t raw = (int16_t)roundf(steps); // signed 9bit nám stačí

    // D8..D0 do bitů 15..7
    uint16_t regVal = ((uint16_t)raw) << 7;
    return regVal;
}

static bool lm75b_setTos(float tempC)
{
    uint16_t regVal = lm75b_encodeThreshold(tempC);
    return lm75b_write16(LM75B_REG_TOS, regVal);
}

static bool lm75b_setThyst(float tempC)
{
    uint16_t regVal = lm75b_encodeThreshold(tempC);
    return lm75b_write16(LM75B_REG_THYST, regVal);
}

static bool lm75b_readConfig(uint8_t &conf)
{
    return lm75b_read8(LM75B_REG_CONF, conf);
}

// ----------------------------------------------------------
// Veřejné funkce (API)
// ----------------------------------------------------------

bool TempLM75_Init(String &err)
{
    err = "";

    if (g_temp.initialized)
    {
        // už inicializováno – bereme jako OK
        return true;
    }

    // Pokud byl modul dříve inicializovaný a byl vynucen re-init změnou konfigurace,
    // nejdřív uvolníme předchozí držení sběrnice.
    if (g_temp.busOwned)
    {
        hal_i2c_release(g_temp.bus);
        g_temp.busOwned = false;
    }

    if (!hal_i2c_acquire(g_temp.bus, g_temp.sdaPin, g_temp.sclPin, g_temp.i2cFreq))
    {
        err = "Wire.begin() failed";
        return false;
    }
    g_temp.busOwned = true;

    // ověř komunikaci se senzorem (přečtením configu)
    uint8_t conf = 0;
    if (!lm75b_readConfig(conf))
    {
        err = "LM75B not responding at 0x";
        err += String(g_temp.addr, HEX);

        hal_i2c_release(g_temp.bus);
        g_temp.busOwned = false;
        pinMode(g_temp.sdaPin, INPUT);
        pinMode(g_temp.sclPin, INPUT);
        return false;
    }

    // zahodíme první měření (po datasheetu může být neplatné)
    float dummy;
    (void)lm75b_readTemperatureC(dummy);

    g_temp.initialized = true;
    return true;
}

bool TempLM75_Deinit(String &err)
{
    err = "";

    if (g_temp.busOwned)
    {
        hal_i2c_release(g_temp.bus);
        g_temp.busOwned = false;
    }

    // piny do Hi-Z (podle aktuální konfigurace)
    pinMode(g_temp.sdaPin, INPUT);
    pinMode(g_temp.sclPin, INPUT);

    g_temp.initialized = false;
    return true;
}

bool TempLM75_ReadData(String &value, String &err)
{
    err = "";
    value = "";

    if (!g_temp.initialized)
    {
        err = "Temp sensor not initialized (use temp-init)";
        return false;
    }

    float t;
    if (!lm75b_readTemperatureC(t))
    {
        err = "I2C read error (TEMP)";
        return false;
    }

    // 3 desetinná místa, např. "25.375"
    value = String(t, 3);
    return true;
}

bool TempLM75_SetThresholds(float thyst, float tos, String &err)
{
    err = "";

    if (!g_temp.initialized)
    {
        err = "Temp sensor not initialized (use temp-init)";
        return false;
    }

    if (!(thyst < tos))
    {
        err = "Invalid thresholds: Thyst must be < Tos";
        return false;
    }

    // nejdriv Thyst (dole), pak Tos (nahore)
    if (!lm75b_setThyst(thyst))
    {
        err = "Failed to write Thyst";
        return false;
    }

    if (!lm75b_setTos(tos))
    {
        err = "Failed to write Tos";
        return false;
    }

    return true;
}

// ----------------------------------------------------------
// Pomocné parsování příkazů
// ----------------------------------------------------------

// parsování dvou floatů z textu "19.0 20.0"
static bool parseTwoFloats(const String &text, float &a, float &b, String &err)
{
    err = "";

    // uděláme si kopii do C stringu kvůli strtof
    char buf[64];
    text.substring(0, sizeof(buf) - 1).toCharArray(buf, sizeof(buf));

    char *p = buf;
    // přeskočit leading spaces
    while (*p == ' ' || *p == '\t')
        ++p;

    if (*p == '\0')
    {
        err = "Missing Thyst and Tos values";
        return false;
    }

    char *endptr = nullptr;
    a = strtof(p, &endptr);
    if (endptr == p)
    {
        err = "Invalid Thyst value";
        return false;
    }

    p = endptr;
    while (*p == ' ' || *p == '\t')
        ++p;

    if (*p == '\0')
    {
        err = "Missing Tos value";
        return false;
    }

    b = strtof(p, &endptr);
    if (endptr == p)
    {
        err = "Invalid Tos value";
        return false;
    }

    return true;
}

// parsování argumentů pro "temp-init <bus> <sda> <scl> <freqHz> <addr>"
static bool parseTempInitArgs(
    const String &text,
    int &bus, // 0 nebo 1
    int &sda,
    int &scl,
    uint32_t &freq,
    uint8_t &addr,
    String &err)
{
    err = "";

    char buf[96];
    text.substring(0, sizeof(buf) - 1).toCharArray(buf, sizeof(buf));

    char *p = buf;

    auto skipSpaces = [&]()
    {
        while (*p == ' ' || *p == '\t')
            ++p;
    };

    char *end = nullptr;

    // bus
    skipSpaces();
    if (*p == '\0')
    {
        err = "Missing bus index";
        return false;
    }
    long busL = strtol(p, &end, 10);
    if (end == p)
    {
        err = "Invalid bus index";
        return false;
    }
    bus = (int)busL;
    p = end;

    // sda
    skipSpaces();
    if (*p == '\0')
    {
        err = "Missing SDA pin";
        return false;
    }
    long sdaL = strtol(p, &end, 10);
    if (end == p)
    {
        err = "Invalid SDA pin";
        return false;
    }
    sda = (int)sdaL;
    p = end;

    // scl
    skipSpaces();
    if (*p == '\0')
    {
        err = "Missing SCL pin";
        return false;
    }
    long sclL = strtol(p, &end, 10);
    if (end == p)
    {
        err = "Invalid SCL pin";
        return false;
    }
    scl = (int)sclL;
    p = end;

    // freq
    skipSpaces();
    if (*p == '\0')
    {
        err = "Missing frequency";
        return false;
    }
    unsigned long freqL = strtoul(p, &end, 10);
    if (end == p)
    {
        err = "Invalid frequency";
        return false;
    }
    freq = (uint32_t)freqL;
    p = end;

    // addr (umožní 0x48 i decimální 72)
    skipSpaces();
    if (*p == '\0')
    {
        err = "Missing I2C address";
        return false;
    }
    unsigned long addrL = strtoul(p, &end, 0); // base 0 = auto (0x.., 0.., nebo dec)
    if (end == p)
    {
        err = "Invalid I2C address";
        return false;
    }
    if (addrL > 0x7F)
    {
        err = "I2C address must be 7-bit (0..0x7F)";
        return false;
    }
    addr = (uint8_t)addrL;

    return true;
}

// ----------------------------------------------------------
// Hlavní handler pro textové příkazy "temp-*"
// ----------------------------------------------------------

bool TempLM75_ApplyCommand(const String &cmd, String &out, String &err)
{
    out = "";
    err = "";

    String s = cmd;
    s.trim();

    if (s.startsWith("temp-init"))
    {
        const char *prefix = "temp-init";
        String rest = s.substring(strlen(prefix));
        rest.trim();

        if (rest.length() == 0)
        {
            // starý způsob: bez parametrů, použij výchozí nastavení
            // zachovává zpětnou kompatibilitu
            g_temp.bus = 1;
            g_temp.sdaPin = TEMP_SDA_PIN_DEFAULT;
            g_temp.sclPin = TEMP_SCL_PIN_DEFAULT;
            g_temp.i2cFreq = TEMP_I2C_FREQ_DEFAULT;
            g_temp.addr = LM75B_ADDR_DEFAULT;

            // při opakovaném initu se může konfigurace změnit – vynutíme re-init
            g_temp.initialized = false;

            if (TempLM75_Init(err))
            {
                out = "TEMP INIT OK";
                return true;
            }
            return false;
        }
        else
        {
            // nový způsob: temp-init <bus> <sda> <scl> <freqHz> <addr>
            int bus = 0;
            int sda = 0;
            int scl = 0;
            uint32_t freq = 0;
            uint8_t addr = 0;

            if (!parseTempInitArgs(rest, bus, sda, scl, freq, addr, err))
            {
                return false;
            }

            if (bus == 0)
                g_temp.bus = 0;
            else if (bus == 1)
                g_temp.bus = 1;
            else
            {
                err = "Bus must be 0 (Wire) or 1 (Wire1)";
                return false;
            }

            g_temp.sdaPin = (uint8_t)sda;
            g_temp.sclPin = (uint8_t)scl;
            g_temp.i2cFreq = freq;
            g_temp.addr = addr;

            // při změně konfigurace vynutíme re-init
            g_temp.initialized = false;

            if (TempLM75_Init(err))
            {
                out = "TEMP INIT OK";
                return true;
            }
            return false;
        }
    }
    else if (s.equalsIgnoreCase("temp-deinit"))
    {
        if (TempLM75_Deinit(err))
        {
            out = "TEMP DE-INIT OK";
            return true;
        }
        return false;
    }
    else if (s.equalsIgnoreCase("temp-read-data"))
    {
        return TempLM75_ReadData(out, err);
    }
    else if (s.startsWith("temp-set-treshold"))
    {
        // odřízneme "temp-set-treshold"
        const char *prefix = "temp-set-treshold";
        String rest = s.substring(strlen(prefix));
        rest.trim(); // v rest by mělo být "19.0 20.0"

        float thyst = 0.0f, tos = 0.0f;
        if (!parseTwoFloats(rest, thyst, tos, err))
        {
            return false;
        }

        if (TempLM75_SetThresholds(thyst, tos, err))
        {
            out = "SET TRESHOLD OK";
            return true;
        }
        return false;
    }
    else
    {
        err = "Unknown TEMP command";
        return false;
    }
}
