#include <ADC-ADS7961-SPI/adc_ads7961_spi.h>

#include <SPI.h>
#include <stdlib.h>

#if __has_include(<board_pins.h>)
#include <board_pins.h>
#endif

namespace
{
static String Hex16(uint16_t v)
{
    char b[8];
    snprintf(b, sizeof(b), "0x%04X", (unsigned)v);
    return String(b);
}

static String Hex8(uint8_t v)
{
    char b[6];
    snprintf(b, sizeof(b), "0x%02X", (unsigned)v);
    return String(b);
}

static bool parseIntToken(const String &token, int &out)
{
    char *end = nullptr;
    long v = strtol(token.c_str(), &end, 10);
    if (end == token.c_str() || *end != '\0')
    {
        return false;
    }
    out = (int)v;
    return true;
}

static std::vector<String> splitTokens(const String &input)
{
    std::vector<String> tokens;
    int start = -1;
    for (int i = 0; i < input.length(); ++i)
    {
        if (input[i] != ' ' && input[i] != '\t')
        {
            if (start < 0)
                start = i;
        }
        else if (start >= 0)
        {
            tokens.push_back(input.substring(start, i));
            start = -1;
        }
    }
    if (start >= 0)
        tokens.push_back(input.substring(start));
    return tokens;
}
} // namespace

uint16_t AdcAds7961Spi::State::BuildManualCmd(uint8_t channel) const
{
    return BuildManualCmdEx(channel, false, 0, false);
}

uint16_t AdcAds7961Spi::State::BuildManualCmdEx(uint8_t channel, bool gpioRead, uint8_t gpioOutNibble, bool powerDown) const
{
    channel &= 0x0F;
    gpioOutNibble &= 0x0F;

    uint16_t cmd = 0;
    cmd |= (0b0001u << 12);
    cmd |= (1u << 11);
    cmd |= (uint16_t(channel) << 7);
    cmd |= (range2xRef ? (1u << 6) : 0u);
    cmd |= (powerDown ? (1u << 5) : 0u);
    cmd |= (gpioRead ? (1u << 4) : 0u);
    cmd |= (uint16_t(gpioOutNibble) & 0x0F);
    return cmd;
}

uint8_t AdcAds7961Spi::State::ExtractHeader4(uint16_t rx) const
{
    return (uint8_t)((rx >> 12) & 0x0F);
}

uint8_t AdcAds7961Spi::State::ExtractData8(uint16_t rx) const
{
    return (uint8_t)((rx >> 4) & 0xFF);
}

void AdcAds7961Spi::State::GuardCsHigh() const
{
    if (csGuard >= 0)
    {
        digitalWrite(csGuard, HIGH);
    }
}

void AdcAds7961Spi::State::UpdateSpiSettings()
{
    spiSettings = SPISettings(spiHz, MSBFIRST, SPI_MODE0);
}

uint16_t AdcAds7961Spi::State::Transfer16(uint16_t tx)
{
    GuardCsHigh();
    SPI.beginTransaction(spiSettings);
    digitalWrite(csAdc, LOW);
    uint16_t rx = SPI.transfer16(tx);
    digitalWrite(csAdc, HIGH);
    SPI.endTransaction();
    return rx;
}

bool AdcAds7961Spi::State::Init(String &err)
{
    err = "";
    if (ready)
        return true;

    pinMode(csAdc, OUTPUT);
    digitalWrite(csAdc, HIGH);

    if (csGuard >= 0)
    {
        pinMode(csGuard, OUTPUT);
        digitalWrite(csGuard, HIGH);
    }

    SPI.begin(sck, miso, mosi, -1);

    Transfer16(BuildManualCmd(0));
    Transfer16(BuildManualCmd(0));
    Transfer16(BuildManualCmd(0));

    ready = true;
    return true;
}

void AdcAds7961Spi::State::Deinit()
{
    if (!ready)
        return;

    SPI.end();
    ready = false;

    pinMode(sck, INPUT);
    pinMode(mosi, INPUT);
    pinMode(miso, INPUT);

    pinMode(csAdc, OUTPUT);
    digitalWrite(csAdc, HIGH);

    if (csGuard >= 0)
    {
        pinMode(csGuard, OUTPUT);
        digitalWrite(csGuard, HIGH);
    }
}

bool AdcAds7961Spi::State::ReadChannel(uint8_t ch, uint8_t &code8, uint16_t &rx, String &err)
{
    err = "";
    code8 = 0;
    rx = 0;

    if (!ready)
    {
        err = "adc not initialized";
        return false;
    }

    Transfer16(BuildManualCmd(ch));
    Transfer16(BuildManualCmd(ch));
    rx = Transfer16(BuildManualCmd(ch));

    code8 = ExtractData8(rx);
    return true;
}

float AdcAds7961Spi::State::Code8ToVolts(uint8_t code8) const
{
    const float vfs = range2xRef ? (2.0f * refpVolts) : refpVolts;
    return (float)code8 * (vfs / 255.0f);
}

void AdcAds7961Spi::State::HandleRead(uint8_t ch, std::vector<String> &lines)
{
    if (!ready)
    {
        lines.push_back("ERR-1");
        lines.push_back("Error: adc not initialized");
        return;
    }

    uint8_t code8 = 0;
    uint16_t rx = 0;
    String readErr;
    if (!ReadChannel(ch, code8, rx, readErr))
    {
        lines.push_back("ERR-1");
        lines.push_back("Error: " + readErr);
        return;
    }

    float v = Code8ToVolts(code8);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.4f", (double)v);
    lines.push_back("OK-1");
    lines.push_back(String(buf));
}

void AdcAds7961Spi::State::ScanRange(uint8_t chFrom, uint8_t chTo, std::vector<String> &lines)
{
    if (!ready)
    {
        lines.push_back("ERR-1");
        lines.push_back("Error: adc not initialized");
        return;
    }

    lines.push_back("OK-1");
    for (uint8_t ch = chFrom; ch <= chTo; ch++)
    {
        uint8_t code8 = 0;
        uint16_t rx = 0;
        String readErr;
        if (!ReadChannel(ch, code8, rx, readErr))
        {
            lines.push_back("CH" + String(ch) + ": ERR " + readErr);
            continue;
        }

        float v = Code8ToVolts(code8);
        char buf[96];
        snprintf(buf, sizeof(buf), "CH%u: rx=0x%04X code8=%u (0x%02X) Vadc=%.4f",
                 (unsigned)ch, (unsigned)rx, (unsigned)code8, (unsigned)code8, (double)v);
        lines.push_back(String(buf));
    }
}

void AdcAds7961Spi::State::ScanExplicit(const uint8_t *channels, size_t count, std::vector<String> &lines)
{
    if (!ready)
    {
        lines.push_back("ERR-1");
        lines.push_back("Error: adc not initialized");
        return;
    }

    lines.push_back("OK-1");
    for (size_t i = 0; i < count; ++i)
    {
        const uint8_t ch = channels[i];
        uint8_t code8 = 0;
        uint16_t rx = 0;
        String readErr;
        if (!ReadChannel(ch, code8, rx, readErr))
        {
            lines.push_back("CH" + String(ch) + ": ERR " + readErr);
            continue;
        }

        float v = Code8ToVolts(code8);
        char buf[96];
        snprintf(buf, sizeof(buf), "CH%u: rx=0x%04X code8=%u (0x%02X) Vadc=%.4f",
                 (unsigned)ch, (unsigned)rx, (unsigned)code8, (unsigned)code8, (double)v);
        lines.push_back(String(buf));
    }
}

void AdcAds7961Spi::State::PrintHelp(std::vector<String> &lines) const
{
    lines.push_back("OK-1");
    lines.push_back("ADC Commands:");
    lines.push_back("adc-init");
    lines.push_back("adc-deinit");
    lines.push_back("adc-help");
    lines.push_back("adc-read-ch0  (MTR1-TEMP)");
    lines.push_back("adc-read-ch1  (MTR2-TEMP)");
    lines.push_back("adc-read-ch2  (MTR3-TEMP)");
    lines.push_back("adc-read-ch3  (MTR4-TEMP)");
    lines.push_back("adc-read-ch4  (PHD-DOT-CAL-EN)");
    lines.push_back("adc-read-ch5  (NetJ11_10)");
    lines.push_back("adc-read-ch6  (PHD-TEMP)");
    lines.push_back("adc-read-ch8  (PH-VOLTS)");
    lines.push_back("adc-read-ch7   (GND)");
    lines.push_back("adc-read-ch9  (+3V3F)");
    lines.push_back("adc-read-ch10  (+5VF)");
    lines.push_back("adc-read-ch11  ()");
    lines.push_back("adc-read-ch12  ()");
    lines.push_back("adc-read-ch13  ()");
    lines.push_back("adc-read-ch15  (GND)");
    lines.push_back("====================");
    lines.push_back("adc-scan-0-3");
    lines.push_back("adc-scan-5-6-8");
    lines.push_back("adc-scan-9-13");
    lines.push_back("adc-diag-format-ch0-200   (checks header + stuck RX + stats)");
    lines.push_back("adc-diag-gpio-200         (checks GPIO header + stuck RX + stats)");
}


AdcAds7961Spi::AdcAds7961Spi()
{
    _state.UpdateSpiSettings();
}

bool AdcAds7961Spi::ApplyCommand(const String &cmd, std::vector<String> &lines, String &err)
{
    err = "";
    lines.clear();

    if (!(cmd.startsWith("adc-") || cmd.startsWith("ADC-")))
        return false;

    if (cmd.equalsIgnoreCase("adc-help"))
    {
        _state.PrintHelp(lines);
        return true;
    }

    if (cmd.startsWith("adc-init") || cmd.startsWith("ADC-INIT"))
    {
        std::vector<String> tokens = splitTokens(cmd);
        if (tokens.size() != 1 && tokens.size() != 5 && tokens.size() != 6 && tokens.size() != 7)
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: use adc-init or adc-init <sck> <miso> <mosi> <csAdc> [spiHz] [csGuard]");
            return true;
        }

        if (tokens.size() == 1)
        {
            _state.sck = State::DEFAULT_SCK;
            _state.miso = State::DEFAULT_MISO;
            _state.mosi = State::DEFAULT_MOSI;
            _state.csAdc = State::DEFAULT_CS_ADC;
            _state.spiHz = State::DEFAULT_SPI_HZ;
            _state.csGuard = State::DEFAULT_CS_GUARD;
            _state.UpdateSpiSettings();
        }
        else
        {
            int sck = 0, miso = 0, mosi = 0, csAdc = 0;
            int spiHz = (int)State::DEFAULT_SPI_HZ;
            int csGuard = State::DEFAULT_CS_GUARD;

            if (!parseIntToken(tokens[1], sck) || !parseIntToken(tokens[2], miso) ||
                !parseIntToken(tokens[3], mosi) || !parseIntToken(tokens[4], csAdc))
            {
                lines.push_back("ERR-1");
                lines.push_back("Error: invalid adc-init pins");
                return true;
            }

            if (tokens.size() >= 6 && !parseIntToken(tokens[5], spiHz))
            {
                lines.push_back("ERR-1");
                lines.push_back("Error: invalid spiHz");
                return true;
            }

            if (tokens.size() >= 7 && !parseIntToken(tokens[6], csGuard))
            {
                lines.push_back("ERR-1");
                lines.push_back("Error: invalid csGuard");
                return true;
            }

            _state.sck = sck;
            _state.miso = miso;
            _state.mosi = mosi;
            _state.csAdc = csAdc;
            _state.csGuard = csGuard;
            _state.spiHz = spiHz > 0 ? (uint32_t)spiHz : State::DEFAULT_SPI_HZ;
            _state.UpdateSpiSettings();
        }

        String initErr;
        if (_state.Init(initErr))
        {
            lines.push_back("OK-1");
            lines.push_back("adc-init done");
        }
        else
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: " + initErr);
        }
        return true;
    }

    if (cmd.equalsIgnoreCase("adc-deinit"))
    {
        _state.Deinit();
        lines.push_back("OK-1");
        lines.push_back("adc-deinit done");
        return true;
    }

    if (cmd.equalsIgnoreCase("adc-scan-0-3"))
    {
        _state.ScanRange(0, 3, lines);
        return true;
    }

    if (cmd.equalsIgnoreCase("adc-scan-5-6-8"))
    {
        static const uint8_t channels[] = {5, 6, 8};
        _state.ScanExplicit(channels, 3, lines);
        return true;
    }

    if (cmd.equalsIgnoreCase("adc-scan-9-13"))
    {
        _state.ScanRange(9, 13, lines);
        return true;
    }

    if (cmd.startsWith("adc-read-ch") || cmd.startsWith("ADC-READ-CH"))
    {
        int idx = cmd.lastIndexOf("ch");
        if (idx < 0)
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: invalid command format");
            return true;
        }

        int ch = cmd.substring(idx + 2).toInt();
        if (ch < 0 || ch > 15)
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: channel must be 0..15");
            return true;
        }

        _state.HandleRead((uint8_t)ch, lines);
        return true;
    }

    if (cmd.startsWith("adc-diag-format-ch") || cmd.startsWith("ADC-DIAG-FORMAT-CH"))
    {
        int pCh = cmd.indexOf("ch");
        int pDash = cmd.indexOf('-', pCh);
        if (pCh < 0 || pDash < 0)
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: use adc-diag-format-chX-N (e.g. adc-diag-format-ch0-200)");
            return true;
        }

        int ch = cmd.substring(pCh + 2, pDash).toInt();
        int n = cmd.substring(pDash + 1).toInt();
        if (ch < 0 || ch > 15 || n <= 0)
            n = 200;

        if (!_state.ready)
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: adc not initialized");
            return true;
        }

        uint16_t cmdWord = _state.BuildManualCmdEx((uint8_t)ch, false, 0, false);
        _state.Transfer16(cmdWord);
        _state.Transfer16(cmdWord);

        uint32_t headerOk = 0, headerBad = 0;
        uint32_t rxAll0 = 0, rxAll1 = 0, rxSameAsPrev = 0;
        uint16_t prevRx = 0xFFFF;
        uint8_t minCode = 0xFF, maxCode = 0x00;
        uint16_t hdrCount[16] = {0};
        uint16_t firstRx = 0, lastRx = 0, minRx = 0xFFFF, maxRx = 0x0000;

        for (int i = 0; i < n; i++)
        {
            uint16_t rx = _state.Transfer16(cmdWord);
            if (i == 0)
                firstRx = rx;
            lastRx = rx;
            if (rx < minRx)
                minRx = rx;
            if (rx > maxRx)
                maxRx = rx;

            uint8_t hdr = _state.ExtractHeader4(rx);
            hdrCount[hdr]++;
            if (hdr == (uint8_t)ch)
                headerOk++;
            else
                headerBad++;

            if (rx == 0x0000)
                rxAll0++;
            if (rx == 0xFFFF)
                rxAll1++;
            if (i > 0 && rx == prevRx)
                rxSameAsPrev++;
            prevRx = rx;

            uint8_t code8 = _state.ExtractData8(rx);
            if (code8 < minCode)
                minCode = code8;
            if (code8 > maxCode)
                maxCode = code8;
        }

        int bestHdr = 0;
        uint16_t bestCnt = hdrCount[0];
        for (int h = 1; h < 16; h++)
        {
            if (hdrCount[h] > bestCnt)
            {
                bestCnt = hdrCount[h];
                bestHdr = h;
            }
        }

        String verdict = "PASS";
        String meaning = "header OK and RX not stuck";
        if (rxAll0 == (uint32_t)n)
        {
            verdict = "FAIL";
            meaning = "RX stuck at 0x0000 -> MISO/SDO low OR ADC not driving (power/reset/CS issue)";
        }
        else if (rxAll1 == (uint32_t)n)
        {
            verdict = "FAIL";
            meaning = "RX stuck at 0xFFFF -> MISO floating/pulled-up OR ADC not selected";
        }
        else if (headerBad > 0)
        {
            verdict = "WARN";
            meaning = "header mismatches -> MOSI/CS timing/glitch/ringing or wrong SPI mode";
        }

        lines.push_back("OK-1");
        lines.push_back("diag-format (ADC 16-bit RX frames):");
        lines.push_back("  requested_ch=" + String(ch) + " samples=" + String(n));
        lines.push_back("  header(DO15..12): ok=" + String(headerOk) + " bad=" + String(headerBad) +
                        "  most_seen=0x" + String(bestHdr, HEX) + " (" + String(bestCnt) + "x)");
        lines.push_back("  rx_stuck: 0x0000=" + String(rxAll0) +
                        "  0xFFFF=" + String(rxAll1) +
                        "  same_as_prev=" + String(rxSameAsPrev));
        lines.push_back("  data8(DO11..4): min=" + String(minCode) + " (" + Hex8(minCode) + ")" +
                        "  max=" + String(maxCode) + " (" + Hex8(maxCode) + ")");
        lines.push_back("  example_rx: first=" + Hex16(firstRx) +
                        " last=" + Hex16(lastRx) +
                        " min=" + Hex16(minRx) +
                        " max=" + Hex16(maxRx));
        lines.push_back("  verdict=" + verdict);
        lines.push_back("  meaning: " + meaning);
        return true;
    }

    if (cmd.startsWith("adc-diag-gpio") || cmd.startsWith("ADC-DIAG-GPIO"))
    {
        int pDash = cmd.lastIndexOf('-');
        int n = 200;
        if (pDash > 0)
            n = cmd.substring(pDash + 1).toInt();
        if (n <= 0)
            n = 200;

        if (!_state.ready)
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: adc not initialized");
            return true;
        }

        uint16_t cmdWord = _state.BuildManualCmdEx(0, true, 0, false);
        _state.Transfer16(cmdWord);
        _state.Transfer16(cmdWord);

        uint32_t same = 0, changes = 0;
        uint32_t rxAll0 = 0, rxAll1 = 0;
        int lastNib = -1;
        uint16_t firstRx = 0, lastRx = 0, minRx = 0xFFFF, maxRx = 0x0000;

        for (int i = 0; i < n; i++)
        {
            uint16_t rx = _state.Transfer16(cmdWord);
            if (i == 0)
                firstRx = rx;
            lastRx = rx;
            if (rx < minRx)
                minRx = rx;
            if (rx > maxRx)
                maxRx = rx;

            int nib = (int)_state.ExtractHeader4(rx);
            if (rx == 0x0000)
                rxAll0++;
            if (rx == 0xFFFF)
                rxAll1++;

            if (i == 0)
                lastNib = nib;
            else if (nib == lastNib)
                same++;
            else
            {
                changes++;
                lastNib = nib;
            }
        }

        String verdict = "PASS";
        String meaning = "GPIO nibble stable";
        if (rxAll0 == (uint32_t)n)
        {
            verdict = "FAIL";
            meaning = "RX stuck at 0x0000 -> MISO/SDO low OR ADC not driving";
        }
        else if (rxAll1 == (uint32_t)n)
        {
            verdict = "FAIL";
            meaning = "RX stuck at 0xFFFF -> MISO floating/pulled-up OR ADC not selected";
        }
        else if (changes > 0)
        {
            verdict = "WARN";
            meaning = "GPIO nibble changes unexpectedly -> signal integrity / CS timing";
        }

        lines.push_back("OK-1");
        lines.push_back("diag-gpio (DO15..12 shows GPIO nibble):");
        lines.push_back("  samples=" + String(n));
        lines.push_back("  gpio_nibble: stable=" + String(same) +
                        " changes=" + String(changes) +
                        " last=0x" + String(lastNib, HEX));
        lines.push_back("  rx_stuck: 0x0000=" + String(rxAll0) +
                        "  0xFFFF=" + String(rxAll1));
        lines.push_back("  example_rx: first=" + Hex16(firstRx) +
                        " last=" + Hex16(lastRx) +
                        " min=" + Hex16(minRx) +
                        " max=" + Hex16(maxRx));
        lines.push_back("  verdict=" + verdict);
        lines.push_back("  meaning: " + meaning);
        return true;
    }

    lines.push_back("ERR-1");
    lines.push_back("Error: unknown adc command (use adc-help)");
    return true;
}
