#include <IO-ADS7961-DAC7564-SPI/io_ads7961_dac7564_spi.h>

#include <ctype.h>
#include <math.h>
#include <stdlib.h>

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

static bool ParseDacChannelToken(const String &tokUpper, char &channelLetter)
{
    if (tokUpper == "DAC-VOUTA" || tokUpper == "DAC-VOUTA-V")
    {
        channelLetter = 'A';
        return true;
    }
    if (tokUpper == "DAC-VOUTB" || tokUpper == "DAC-VOUTB-V")
    {
        channelLetter = 'B';
        return true;
    }
    if (tokUpper == "DAC-VOUTC" || tokUpper == "DAC-VOUTC-V")
    {
        channelLetter = 'C';
        return true;
    }
    if (tokUpper == "DAC-VOUTD" || tokUpper == "DAC-VOUTD-V")
    {
        channelLetter = 'D';
        return true;
    }
    return false;
}

static bool IsDacVoltsToken(const String &tokUpper)
{
    return tokUpper.endsWith("-V");
}

static uint8_t ChannelToSel(char channelLetter)
{
    switch (toupper((unsigned char)channelLetter))
    {
    case 'A':
        return 0;
    case 'B':
        return 1;
    case 'C':
        return 2;
    case 'D':
        return 3;
    default:
        return 0;
    }
}

static uint16_t DacFracToCode(float frac)
{
    if (frac < 0.0f)
        frac = 0.0f;
    if (frac > 1.0f)
        frac = 1.0f;

    long code = lroundf(frac * 4095.0f);
    if (code < 0)
        code = 0;
    if (code > 4095)
        code = 4095;
    return (uint16_t)code;
}

static bool DacVoltsToCode(float volts, float vref, uint16_t &outCode, String &err)
{
    if (!isfinite(volts) || !isfinite(vref))
    {
        err = "DAC: volts/vref not finite";
        return false;
    }

    if (vref <= 0.0f)
    {
        err = "DAC: vref must be > 0";
        return false;
    }

    if (volts < 0.0f || volts > vref)
    {
        err = "DAC: volts out of range 0..Vref";
        return false;
    }

    long code = lroundf((volts / vref) * 4095.0f);
    if (code < 0)
        code = 0;
    if (code > 4095)
        code = 4095;

    outCode = (uint16_t)code;
    return true;
}

static uint32_t DacBuildSingleChannelUpdate(uint8_t channelSel, uint16_t code12)
{
    // DAC7564: write-and-update single channel command, address pins A1/A0 assumed 0.
    code12 &= 0x0FFF;

    const uint8_t a1 = 0;
    const uint8_t a0 = 0;
    const uint8_t ld1 = 0;
    const uint8_t ld0 = 1;
    const uint8_t fixed0 = 0;
    const uint8_t sel1 = (channelSel >> 1) & 0x1;
    const uint8_t sel0 = (channelSel >> 0) & 0x1;
    const uint8_t pd0 = 0;

    const uint8_t ctrl =
        (a1 << 7) |
        (a0 << 6) |
        (ld1 << 5) |
        (ld0 << 4) |
        (fixed0 << 3) |
        (sel1 << 2) |
        (sel0 << 1) |
        (pd0 << 0);

    const uint16_t payload16 = (uint16_t)(code12 << 4);
    return ((uint32_t)ctrl << 16) | payload16;
}

static bool ParseStrictFloat(const String &value, float &out)
{
    String t = value;
    t.trim();
    if (t.length() == 0)
        return false;

    const char *c = t.c_str();
    char *endp = nullptr;
    out = strtof(c, &endp);
    if (endp == c)
        return false;

    while (*endp == ' ' || *endp == '\t' || *endp == '\r' || *endp == '\n')
        ++endp;

    return (*endp == '\0');
}

static bool ParseStrictInt(const String &value, int &out)
{
    String t = value;
    t.trim();
    if (t.length() == 0)
        return false;

    const char *c = t.c_str();
    char *endp = nullptr;
    long v = strtol(c, &endp, 10);
    if (endp == c)
        return false;

    while (*endp == ' ' || *endp == '\t' || *endp == '\r' || *endp == '\n')
        ++endp;

    if (*endp != '\0')
        return false;

    out = (int)v;
    return true;
}

static uint16_t AdcBuildManualCmd(const IoAds7961Dac7564Spi::AdcConfig &cfg, uint8_t channel)
{
    channel &= 0x0F;
    uint16_t cmd = 0;
    cmd |= (0b0001u << 12);
    cmd |= (1u << 11);
    cmd |= (uint16_t(channel) << 7);
    cmd |= (cfg.range2xRef ? (1u << 6) : 0u);
    return cmd;
}

static uint16_t AdcBuildManualCmdEx(const IoAds7961Dac7564Spi::AdcConfig &cfg, uint8_t channel, bool gpioRead, uint8_t gpioOutNibble, bool powerDown)
{
    uint16_t cmd = AdcBuildManualCmd(cfg, channel);
    gpioOutNibble &= 0x0F;
    cmd |= (powerDown ? (1u << 5) : 0u);
    cmd |= (gpioRead ? (1u << 4) : 0u);
    cmd |= (uint16_t(gpioOutNibble) & 0x0F);
    return cmd;
}

static uint8_t AdcExtractHeader4(uint16_t rx)
{
    return (uint8_t)((rx >> 12) & 0x0F);
}

static uint8_t AdcExtractData8(uint16_t rx)
{
    return (uint8_t)((rx >> 4) & 0xFF);
}
} // namespace

IoAds7961Dac7564Spi::IoAds7961Dac7564Spi(const Pins &pins, SPIClass &spiBus)
    : _pins(pins), _spi(spiBus),
      _spiAdc(_adcCfg.spiHz, MSBFIRST, SPI_MODE0),
      _spiDac(_dacCfg.spiHz, MSBFIRST, SPI_MODE1)
{
}

bool IoAds7961Dac7564Spi::InitDac(String &err)
{
    err = "";
    if (_dacInited)
        return true;

    if (!AcquireDac_(err))
        return false;

    // DAC7564 init sequence from reference implementation:
    // disable internal reference (0x012000).
    DacWrite24_(0x012000);

    _dacInited = true;
    return true;
}

void IoAds7961Dac7564Spi::DeinitDac()
{
    if (!_dacInited)
        return;

    _dacInited = false;
    ReleaseDac_();
}

bool IoAds7961Dac7564Spi::InitAdc(String &err)
{
    err = "";
    if (_adcInited)
        return true;

    if (!AcquireAdc_(err))
        return false;

    const uint16_t cmd = AdcBuildManualCmd(_adcCfg, 0);
    AdcTransfer16_(cmd);
    AdcTransfer16_(cmd);
    AdcTransfer16_(cmd);

    _adcInited = true;
    return true;
}

void IoAds7961Dac7564Spi::DeinitAdc()
{
    if (!_adcInited)
        return;

    _adcInited = false;
    ReleaseAdc_();
}

bool IoAds7961Dac7564Spi::InitAll(String &err)
{
    err = "not implemented";
    return false;
}

void IoAds7961Dac7564Spi::DeinitAll()
{
}

bool IoAds7961Dac7564Spi::IsDacReady() const
{
    return _dacInited;
}

bool IoAds7961Dac7564Spi::IsAdcReady() const
{
    return _adcInited;
}

void IoAds7961Dac7564Spi::SetAdcConfig(const AdcConfig &cfg)
{
    _adcCfg = cfg;
    _spiAdc = SPISettings(_adcCfg.spiHz, MSBFIRST, SPI_MODE0);
}

void IoAds7961Dac7564Spi::SetDacConfig(const DacConfig &cfg)
{
    _dacCfg = cfg;
    _spiDac = SPISettings(_dacCfg.spiHz, MSBFIRST, SPI_MODE1);
}

bool IoAds7961Dac7564Spi::ApplyCommand(const String &cmdIn, std::vector<String> &lines, String &err)
{
    err = "";
    lines.clear();

    String cmd = cmdIn;
    cmd.trim();
    if (cmd.length() == 0)
    {
        err = "empty command";
        return false;
    }

    if (!(cmd.startsWith("dac-") || cmd.startsWith("DAC-") || cmd.startsWith("adc-") || cmd.startsWith("ADC-")))
        return false;

    int sp = cmd.indexOf(' ');
    String tok = (sp < 0) ? cmd : cmd.substring(0, sp);
    String arg = (sp < 0) ? "" : cmd.substring(sp + 1);
    tok.trim();
    arg.trim();

    String tokUpper = tok;
    tokUpper.toUpperCase();

    if (tokUpper == "ADC-HELP" || tokUpper == "ADC-?")
    {
        lines.push_back("OK-1");
        lines.push_back("ADC Commands:");
        lines.push_back("adc-init");
        lines.push_back("adc-deinit");
        lines.push_back("adc-help");
        lines.push_back("adc-read-chX");
        lines.push_back("adc-readavg-chX");
        lines.push_back("adc-scan-0-3");
        lines.push_back("adc-scan-9-13");
        lines.push_back("adc-diag-format-chX-N");
        lines.push_back("adc-diag-gpio-N");
        return true;
    }

    if (tokUpper == "ADC-INIT")
    {
        if (arg.length() > 0)
        {
            if (_adcInited)
            {
                lines.push_back("ERR-1");
                lines.push_back("Error: adc already initialized, deinit first");
                return true;
            }

            String p1, p2, p3, p4, p5, p6;
            int sp1 = arg.indexOf(' ');
            if (sp1 < 0)
            {
                lines.push_back("ERR-1");
                lines.push_back("Error: use adc-init <sck> <miso> <mosi> <csAdc> [spiHz] [csGuard]");
                return true;
            }
            p1 = arg.substring(0, sp1);

            String rem = arg.substring(sp1 + 1);
            rem.trim();
            int sp2 = rem.indexOf(' ');
            if (sp2 < 0)
            {
                lines.push_back("ERR-1");
                lines.push_back("Error: use adc-init <sck> <miso> <mosi> <csAdc> [spiHz] [csGuard]");
                return true;
            }
            p2 = rem.substring(0, sp2);

            rem = rem.substring(sp2 + 1);
            rem.trim();
            int sp3 = rem.indexOf(' ');
            if (sp3 < 0)
            {
                lines.push_back("ERR-1");
                lines.push_back("Error: use adc-init <sck> <miso> <mosi> <csAdc> [spiHz] [csGuard]");
                return true;
            }
            p3 = rem.substring(0, sp3);

            rem = rem.substring(sp3 + 1);
            rem.trim();
            int sp4 = rem.indexOf(' ');
            if (sp4 < 0)
            {
                p4 = rem;
            }
            else
            {
                p4 = rem.substring(0, sp4);
                rem = rem.substring(sp4 + 1);
                rem.trim();

                int sp5 = rem.indexOf(' ');
                if (sp5 < 0)
                {
                    p5 = rem;
                }
                else
                {
                    p5 = rem.substring(0, sp5);
                    p6 = rem.substring(sp5 + 1);
                    p6.trim();
                }
            }

            int sck = 0, miso = 0, mosi = 0, csAdc = 0;
            if (!ParseStrictInt(p1, sck) || !ParseStrictInt(p2, miso) ||
                !ParseStrictInt(p3, mosi) || !ParseStrictInt(p4, csAdc))
            {
                lines.push_back("ERR-1");
                lines.push_back("Error: invalid adc-init pins");
                return true;
            }

            _pins.sclk = sck;
            _pins.miso = miso;
            _pins.mosi = mosi;
            _pins.csAdc = csAdc;

            if (p5.length() > 0)
            {
                int spiHz = 0;
                if (!ParseStrictInt(p5, spiHz))
                {
                    lines.push_back("ERR-1");
                    lines.push_back("Error: invalid spiHz");
                    return true;
                }
                if (spiHz > 0)
                {
                    _adcCfg.spiHz = (uint32_t)spiHz;
                    _spiAdc = SPISettings(_adcCfg.spiHz, MSBFIRST, SPI_MODE0);
                }
            }

            if (p6.length() > 0)
            {
                int csGuard = 0;
                if (!ParseStrictInt(p6, csGuard))
                {
                    lines.push_back("ERR-1");
                    lines.push_back("Error: invalid csGuard");
                    return true;
                }
                _pins.csDac = csGuard;
            }
        }

        String initErr;
        if (InitAdc(initErr))
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

    if (tokUpper == "ADC-DEINIT")
    {
        DeinitAdc();
        lines.push_back("OK-1");
        lines.push_back("adc-deinit done");
        return true;
    }

    if (tokUpper.startsWith("ADC-READAVG-CH"))
    {
        int idx = tokUpper.lastIndexOf("CH");
        if (idx < 0)
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: invalid command format");
            return true;
        }

        int ch = tokUpper.substring(idx + 2).toInt();
        if (ch < 0 || ch > 15)
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: channel must be 0..15");
            return true;
        }
        if (!_adcInited)
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: adc not initialized");
            return true;
        }

        float avgV = 0.0f;
        float avgCode8 = 0.0f;
        String readErr;
        if (!AdcReadChannelAvg40Trim4((uint8_t)ch, avgV, avgCode8, readErr))
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: " + readErr);
            return true;
        }

        lines.push_back("OK-1");
        lines.push_back(String(avgV, 4));
        return true;
    }

    if (tokUpper == "ADC-SCAN-0-3" || tokUpper == "ADC-SCAN-9-13")
    {
        if (!_adcInited)
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: adc not initialized");
            return true;
        }

        const uint8_t from = (tokUpper == "ADC-SCAN-0-3") ? 0 : 9;
        const uint8_t to = (tokUpper == "ADC-SCAN-0-3") ? 3 : 13;

        lines.push_back("OK-1");
        for (uint8_t ch = from; ch <= to; ch++)
        {
            uint8_t code8 = 0;
            uint16_t rx = 0;
            String readErr;
            if (!AdcReadChannel(ch, code8, rx, readErr))
            {
                lines.push_back("CH" + String(ch) + ": ERR " + readErr);
                continue;
            }

            float v = AdcCode8ToVolts(code8);
            char buf[96];
            snprintf(buf, sizeof(buf), "CH%u: rx=0x%04X code8=%u (0x%02X) Vadc=%.4f",
                     (unsigned)ch, (unsigned)rx, (unsigned)code8, (unsigned)code8, (double)v);
            lines.push_back(String(buf));
        }
        return true;
    }

    if (tokUpper.startsWith("ADC-READ-CH"))
    {
        int idx = tokUpper.lastIndexOf("CH");
        if (idx < 0)
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: invalid command format");
            return true;
        }

        int ch = tokUpper.substring(idx + 2).toInt();
        if (ch < 0 || ch > 15)
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: channel must be 0..15");
            return true;
        }
        if (!_adcInited)
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: adc not initialized");
            return true;
        }

        uint8_t code8 = 0;
        uint16_t rx = 0;
        String readErr;
        if (!AdcReadChannel((uint8_t)ch, code8, rx, readErr))
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: " + readErr);
            return true;
        }

        lines.push_back("OK-1");
        lines.push_back(String(AdcCode8ToVolts(code8), 4));
        return true;
    }

    if (tokUpper.startsWith("ADC-DIAG-FORMAT-CH"))
    {
        int pCh = tokUpper.indexOf("CH");
        int pDash = tokUpper.indexOf('-', pCh);
        if (pCh < 0 || pDash < 0)
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: use adc-diag-format-chX-N (e.g. adc-diag-format-ch0-200)");
            return true;
        }

        int ch = tokUpper.substring(pCh + 2, pDash).toInt();
        int n = tokUpper.substring(pDash + 1).toInt();
        if (ch < 0 || ch > 15 || n <= 0)
            n = 200;

        if (!_adcInited)
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: adc not initialized");
            return true;
        }

        const uint16_t cmdWord = AdcBuildManualCmdEx(_adcCfg, (uint8_t)ch, false, 0, false);
        AdcTransfer16_(cmdWord);
        AdcTransfer16_(cmdWord);

        uint32_t headerOk = 0, headerBad = 0;
        uint32_t rxAll0 = 0, rxAll1 = 0, rxSameAsPrev = 0;
        uint16_t prevRx = 0xFFFF;
        uint8_t minCode = 0xFF, maxCode = 0x00;
        uint16_t hdrCount[16] = {0};
        uint16_t firstRx = 0, lastRx = 0, minRx = 0xFFFF, maxRx = 0x0000;

        for (int i = 0; i < n; i++)
        {
            uint16_t rx = AdcTransfer16_(cmdWord);
            if (i == 0)
                firstRx = rx;
            lastRx = rx;
            if (rx < minRx)
                minRx = rx;
            if (rx > maxRx)
                maxRx = rx;

            uint8_t hdr = AdcExtractHeader4(rx);
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

            uint8_t code8 = AdcExtractData8(rx);
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
            meaning = "RX stuck at 0x0000 -> MISO/SDO low OR ADC not driving";
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

    if (tokUpper.startsWith("ADC-DIAG-GPIO-"))
    {
        int pDash = tokUpper.lastIndexOf('-');
        int n = 200;
        if (pDash > 0)
            n = tokUpper.substring(pDash + 1).toInt();
        if (n <= 0)
            n = 200;

        if (!_adcInited)
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: adc not initialized");
            return true;
        }

        const uint16_t cmdWord = AdcBuildManualCmdEx(_adcCfg, 0, true, 0, false);
        AdcTransfer16_(cmdWord);
        AdcTransfer16_(cmdWord);

        uint32_t same = 0, changes = 0;
        uint32_t rxAll0 = 0, rxAll1 = 0;
        int lastNib = -1;
        uint16_t firstRx = 0, lastRx = 0, minRx = 0xFFFF, maxRx = 0x0000;

        for (int i = 0; i < n; i++)
        {
            uint16_t rx = AdcTransfer16_(cmdWord);
            if (i == 0)
                firstRx = rx;
            lastRx = rx;
            if (rx < minRx)
                minRx = rx;
            if (rx > maxRx)
                maxRx = rx;

            int nib = (int)AdcExtractHeader4(rx);
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

    if (tokUpper.startsWith("ADC-"))
    {
        lines.push_back("ERR-1");
        lines.push_back("Error: unknown adc command (use adc-help)");
        return true;
    }

    if (tokUpper == "DAC-HELP" || tokUpper == "DAC-?")
    {
        lines.push_back("OK-17");
        lines.push_back("DAC7564 commands:");
        lines.push_back("dac-init");
        lines.push_back("dac-deinit");
        lines.push_back("dac-help");
        lines.push_back("dac-vouta 0.25");
        lines.push_back("dac-voutb 0.25");
        lines.push_back("dac-voutc 0.25");
        lines.push_back("dac-voutd 0.25");
        lines.push_back("Direct volts:");
        lines.push_back("dac-vouta-v 0.9");
        lines.push_back("dac-voutb-v 0.9");
        lines.push_back("dac-voutc-v 0.9");
        lines.push_back("dac-voutd-v 0.9");
        lines.push_back(String(" - Vref for '-v' = ") + String(_dacCfg.vrefVolts, 4) + " V");
        lines.push_back("Response contract:");
        lines.push_back(" - success: OK-* + details");
        return true;
    }

    if (tokUpper == "DAC-INIT")
    {
        String initErr;
        if (InitDac(initErr))
        {
            lines.push_back("OK-1");
            lines.push_back("DAC: init done, internal Vref disabled (0x012000)");
        }
        else
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: " + initErr);
        }
        return true;
    }

    if (tokUpper == "DAC-DEINIT")
    {
        DeinitDac();
        lines.push_back("OK-1");
        lines.push_back("DAC: deinit done");
        return true;
    }

    char channelLetter = 0;
    if (!ParseDacChannelToken(tokUpper, channelLetter))
    {
        lines.push_back("ERR-1");
        lines.push_back("Error: unknown dac command (use dac-help)");
        return true;
    }

    if (arg.length() == 0)
    {
        lines.push_back("ERR-1");
        lines.push_back("Error: DAC: missing value");
        return true;
    }

    if (IsDacVoltsToken(tokUpper))
    {
        float volts = NAN;
        if (!ParseStrictFloat(arg, volts))
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: DAC: invalid volts value");
            return true;
        }

        String opErr;
        if (!DacSetVolts(channelLetter, volts, opErr))
        {
            lines.push_back("ERR-1");
            lines.push_back("Error: " + opErr);
            return true;
        }

        lines.push_back("OK-1");
        String out = "DAC: VOUT";
        out += (char)toupper((unsigned char)channelLetter);
        out += " volts=";
        out += String(volts, 4);
        out += " V (Vref=";
        out += String(_dacCfg.vrefVolts, 4);
        out += " V)";
        lines.push_back(out);
        return true;
    }

    float fraction = NAN;
    if (!ParseStrictFloat(arg, fraction))
    {
        lines.push_back("ERR-1");
        lines.push_back("Error: DAC: invalid fraction value");
        return true;
    }

    String opErr;
    if (!DacSetFraction(channelLetter, fraction, opErr))
    {
        lines.push_back("ERR-1");
        lines.push_back("Error: " + opErr);
        return true;
    }

    lines.push_back("OK-1");
    String out = "DAC: VOUT";
    out += (char)toupper((unsigned char)channelLetter);
    out += " frac=";
    out += String(fraction, 4);
    lines.push_back(out);
    return true;
}

bool IoAds7961Dac7564Spi::AdcReadChannel(uint8_t ch, uint8_t &code8, uint16_t &rx, String &err)
{
    err = "";
    code8 = 0;
    rx = 0;

    if (!_adcInited)
    {
        err = "adc not initialized";
        return false;
    }

    const uint16_t cmd = AdcBuildManualCmd(_adcCfg, ch);
    AdcTransfer16_(cmd);
    AdcTransfer16_(cmd);
    rx = AdcTransfer16_(cmd);

    code8 = AdcExtractData8(rx);
    return true;
}

bool IoAds7961Dac7564Spi::AdcReadChannelAvg40Trim4(uint8_t ch, float &avgVolts, float &avgCode8, String &err)
{
    err = "";
    avgVolts = 0.0f;
    avgCode8 = 0.0f;

    if (!_adcInited)
    {
        err = "adc not initialized";
        return false;
    }

    const int total = 40;
    const int trim = 4;
    const int used = total - 2 * trim;
    if (used <= 0)
    {
        err = "invalid averaging parameters";
        return false;
    }

    const uint16_t cmd = AdcBuildManualCmd(_adcCfg, ch);
    AdcTransfer16_(cmd);
    AdcTransfer16_(cmd);

    uint32_t sum = 0;
    for (int i = 0; i < total; i++)
    {
        uint8_t code8 = AdcExtractData8(AdcTransfer16_(cmd));
        if (i >= trim && i < (total - trim))
            sum += (uint32_t)code8;
    }

    avgCode8 = (float)sum / (float)used;
    const float vfs = _adcCfg.range2xRef ? (2.0f * _adcCfg.refpVolts) : _adcCfg.refpVolts;
    avgVolts = avgCode8 * (vfs / 255.0f);
    return true;
}

float IoAds7961Dac7564Spi::AdcCode8ToVolts(uint8_t code8) const
{
    const float vfs = _adcCfg.range2xRef ? (2.0f * _adcCfg.refpVolts) : _adcCfg.refpVolts;
    return (float)code8 * (vfs / 255.0f);
}

bool IoAds7961Dac7564Spi::DacSetFraction(char channelLetter, float fraction, String &err)
{
    err = "";
    if (!_dacInited)
    {
        err = "DAC: not inited. Send: dac-init";
        return false;
    }

    const uint8_t chSel = ChannelToSel(channelLetter);
    uint16_t code = DacFracToCode(fraction);
    uint32_t word = DacBuildSingleChannelUpdate(chSel, code);
    DacWrite24_(word);
    return true;
}

bool IoAds7961Dac7564Spi::DacSetVolts(char channelLetter, float volts, String &err)
{
    err = "";
    if (!_dacInited)
    {
        err = "DAC: not inited. Send: dac-init";
        return false;
    }

    uint16_t code = 0;
    if (!DacVoltsToCode(volts, _dacCfg.vrefVolts, code, err))
        return false;

    const uint8_t chSel = ChannelToSel(channelLetter);
    uint32_t word = DacBuildSingleChannelUpdate(chSel, code);
    DacWrite24_(word);
    return true;
}

bool IoAds7961Dac7564Spi::BusAcquire_(String &err)
{
    err = "";
    if (_busUsers == 0)
    {
        BusEnsurePinsSafe_();
        _spi.begin(_pins.sclk, _pins.miso, _pins.mosi, -1);
        _busStarted = true;
    }

    _busUsers++;
    return true;
}

void IoAds7961Dac7564Spi::BusRelease_()
{
    if (_busUsers <= 0)
        return;

    _busUsers--;
    if (_busUsers == 0)
    {
        if (_busStarted)
        {
            _spi.end();
            _busStarted = false;
        }
        BusEndAndHiZ_();
    }
}

void IoAds7961Dac7564Spi::BusEnsurePinsSafe_()
{
    pinMode(_pins.csDac, OUTPUT);
    pinMode(_pins.csAdc, OUTPUT);
    digitalWrite(_pins.csDac, HIGH);
    digitalWrite(_pins.csAdc, HIGH);
}

void IoAds7961Dac7564Spi::BusEndAndHiZ_()
{
    pinMode(_pins.sclk, INPUT);
    pinMode(_pins.mosi, INPUT);
    pinMode(_pins.miso, INPUT);

    // Keep CS pins in deterministic idle state.
    pinMode(_pins.csDac, OUTPUT);
    pinMode(_pins.csAdc, OUTPUT);
    digitalWrite(_pins.csDac, HIGH);
    digitalWrite(_pins.csAdc, HIGH);
}

bool IoAds7961Dac7564Spi::AcquireDac_(String &err)
{
    err = "";
    if (_dacUsers == 0)
    {
        if (!BusAcquire_(err))
            return false;
        BusEnsurePinsSafe_();
    }

    _dacUsers++;
    return true;
}

void IoAds7961Dac7564Spi::ReleaseDac_()
{
    if (_dacUsers <= 0)
        return;

    _dacUsers--;
    if (_dacUsers == 0)
        BusRelease_();
}

bool IoAds7961Dac7564Spi::AcquireAdc_(String &err)
{
    err = "";
    if (_adcUsers == 0)
    {
        if (!BusAcquire_(err))
            return false;
        BusEnsurePinsSafe_();
    }
    _adcUsers++;
    return true;
}

void IoAds7961Dac7564Spi::ReleaseAdc_()
{
    if (_adcUsers <= 0)
        return;

    _adcUsers--;
    if (_adcUsers == 0)
        BusRelease_();
}

uint16_t IoAds7961Dac7564Spi::AdcTransfer16_(uint16_t tx)
{
    digitalWrite(_pins.csDac, HIGH);

    _spi.beginTransaction(_spiAdc);
    digitalWrite(_pins.csAdc, LOW);
    delayMicroseconds(1);
    const uint16_t rx = _spi.transfer16(tx);
    digitalWrite(_pins.csAdc, HIGH);
    _spi.endTransaction();
    return rx;
}

void IoAds7961Dac7564Spi::DacWrite24_(uint32_t word24)
{
    const uint8_t b0 = (word24 >> 16) & 0xFF;
    const uint8_t b1 = (word24 >> 8) & 0xFF;
    const uint8_t b2 = (word24 >> 0) & 0xFF;

    // Ensure ADC is not selected.
    digitalWrite(_pins.csAdc, HIGH);

    _spi.beginTransaction(_spiDac);
    digitalWrite(_pins.csDac, LOW);
    delayMicroseconds(1);
    _spi.transfer(b0);
    _spi.transfer(b1);
    _spi.transfer(b2);
    digitalWrite(_pins.csDac, HIGH);
    _spi.endTransaction();
}
