#include "ads7961.h"

#include "esp_log.h"

#include <math.h>

#define ADS7961_CMD_BASE               0x1800U
#define ADS7961_CMD_CHANNEL_SHIFT      7U
#define ADS7961_CMD_CHANNEL_MASK       0x0FU
#define ADS7961_CMD_RANGE2X_REF        (1U << 6U)

#define ADS7961_TRANSFER_BYTES         2U
#define ADS7961_AVG_TOTAL_SAMPLES      40
#define ADS7961_AVG_TRIM_SAMPLES       4

static const char *const ADS7961_TAG = "ads7961";

static bool ads7961_is_ready(const ads7961_t *ctx)
{
    return (ctx != NULL) &&
           ctx->initialized &&
           (ctx->spi_dev != NULL) &&
           spi_device_is_initialized(ctx->spi_dev);
}

static bool ads7961_channel_is_valid(uint8_t channel)
{
    return channel <= 15U;
}

static bool ads7961_refp_is_valid(float refp_volts)
{
    return isfinite(refp_volts) && (refp_volts > 0.0f);
}

static float ads7961_vfs(const ads7961_t *ctx)
{
    if (ctx == NULL)
    {
        return 0.0f;
    }

    return ctx->range2x_ref ? (2.0f * ctx->refp_volts) : ctx->refp_volts;
}

static esp_err_t ads7961_transfer16(ads7961_t *ctx, uint16_t tx, uint16_t *out_rx)
{
    uint8_t tx_buf[ADS7961_TRANSFER_BYTES];
    uint8_t rx_buf[ADS7961_TRANSFER_BYTES] = {0U, 0U};
    esp_err_t err;

    if (!ads7961_is_ready(ctx))
    {
        return ESP_ERR_INVALID_STATE;
    }

    tx_buf[0] = (uint8_t)((tx >> 8U) & 0xFFU);
    tx_buf[1] = (uint8_t)(tx & 0xFFU);

    err = spi_device_transfer(ctx->spi_dev, tx_buf, rx_buf, sizeof(tx_buf));
    if (err != ESP_OK)
    {
        ESP_LOGE(ADS7961_TAG, "spi_device_transfer failed: %d", (int)err);
        return err;
    }

    if (out_rx != NULL)
    {
        *out_rx = ((uint16_t)rx_buf[0] << 8U) | (uint16_t)rx_buf[1];
    }

    return ESP_OK;
}

esp_err_t ads7961_init(ads7961_t *ctx, const ads7961_cfg_t *cfg)
{
    if (ctx == NULL || cfg == NULL || cfg->spi_dev == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!spi_device_is_initialized(cfg->spi_dev))
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!ads7961_refp_is_valid(cfg->refp_volts))
    {
        return ESP_ERR_INVALID_ARG;
    }

    ctx->spi_dev = cfg->spi_dev;
    ctx->range2x_ref = cfg->range2x_ref;
    ctx->refp_volts = cfg->refp_volts;
    ctx->initialized = true;

    return ESP_OK;
}

bool ads7961_is_initialized(const ads7961_t *ctx)
{
    return ads7961_is_ready(ctx);
}

uint16_t ads7961_build_manual_cmd(const ads7961_t *ctx, uint8_t channel)
{
    const bool range2x_ref = (ctx != NULL) && ctx->range2x_ref;
    const uint16_t ch_bits = (uint16_t)(channel & ADS7961_CMD_CHANNEL_MASK);
    uint16_t cmd = ADS7961_CMD_BASE;

    cmd |= (uint16_t)(ch_bits << ADS7961_CMD_CHANNEL_SHIFT);
    if (range2x_ref)
    {
        cmd |= ADS7961_CMD_RANGE2X_REF;
    }

    return cmd;
}

uint8_t ads7961_extract_header4(uint16_t rx)
{
    return (uint8_t)((rx >> 12U) & 0x0FU);
}

uint8_t ads7961_extract_data8(uint16_t rx)
{
    return (uint8_t)((rx >> 4U) & 0xFFU);
}

float ads7961_code8_to_volts(const ads7961_t *ctx, uint8_t code8)
{
    const float vfs = ads7961_vfs(ctx);

    if (vfs <= 0.0f)
    {
        return 0.0f;
    }

    return (float)code8 * (vfs / 255.0f);
}

esp_err_t ads7961_read_channel(ads7961_t *ctx,
                               uint8_t channel,
                               uint8_t *out_code8,
                               uint16_t *out_rx)
{
    uint16_t rx = 0U;
    uint16_t cmd;
    esp_err_t err;

    if (out_code8 == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!ads7961_is_ready(ctx))
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!ads7961_channel_is_valid(channel))
    {
        return ESP_ERR_INVALID_ARG;
    }

    cmd = ads7961_build_manual_cmd(ctx, channel);

    err = ads7961_transfer16(ctx, cmd, NULL);
    if (err != ESP_OK)
    {
        return err;
    }

    err = ads7961_transfer16(ctx, cmd, NULL);
    if (err != ESP_OK)
    {
        return err;
    }

    err = ads7961_transfer16(ctx, cmd, &rx);
    if (err != ESP_OK)
    {
        return err;
    }

    *out_code8 = ads7961_extract_data8(rx);
    if (out_rx != NULL)
    {
        *out_rx = rx;
    }

    return ESP_OK;
}

esp_err_t ads7961_read_channel_avg(ads7961_t *ctx,
                                   uint8_t channel,
                                   float *out_avg_volts,
                                   float *out_avg_code8)
{
    const int total = ADS7961_AVG_TOTAL_SAMPLES;
    const int trim = ADS7961_AVG_TRIM_SAMPLES;
    const int used = total - (2 * trim);
    uint32_t sum = 0U;
    uint16_t cmd;
    esp_err_t err;
    int i;

    if (out_avg_volts == NULL || out_avg_code8 == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!ads7961_is_ready(ctx))
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!ads7961_channel_is_valid(channel))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (used <= 0)
    {
        ESP_LOGE(ADS7961_TAG, "invalid averaging parameters: total=%d trim=%d", total, trim);
        return ESP_ERR_INVALID_STATE;
    }

    cmd = ads7961_build_manual_cmd(ctx, channel);

    err = ads7961_transfer16(ctx, cmd, NULL);
    if (err != ESP_OK)
    {
        return err;
    }

    err = ads7961_transfer16(ctx, cmd, NULL);
    if (err != ESP_OK)
    {
        return err;
    }

    for (i = 0; i < total; i++)
    {
        uint16_t rx = 0U;
        uint8_t code8;

        err = ads7961_transfer16(ctx, cmd, &rx);
        if (err != ESP_OK)
        {
            return err;
        }

        code8 = ads7961_extract_data8(rx);
        if (i >= trim && i < (total - trim))
        {
            sum += (uint32_t)code8;
        }
    }

    *out_avg_code8 = (float)sum / (float)used;
    *out_avg_volts = (*out_avg_code8) * (ads7961_vfs(ctx) / 255.0f);

    return ESP_OK;
}
