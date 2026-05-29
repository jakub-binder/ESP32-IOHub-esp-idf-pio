#include "spi_bus.h"

#include "esp_log.h"
#include "soc/soc_caps.h"

static const char *const SPI_BUS_TAG = "spi_bus";
static const uint32_t SPI_DEVICE_MIN_CLOCK_HZ = 1U;

static bool spi_bus_host_is_valid(spi_host_device_t host_id)
{
    return (host_id >= 0) && (host_id < SOC_SPI_PERIPH_NUM);
}

static bool spi_bus_gpio_is_valid(gpio_num_t pin)
{
    return GPIO_IS_VALID_GPIO(pin);
}

static bool spi_bus_pins_are_unique(gpio_num_t sclk_pin,
                                    gpio_num_t mosi_pin,
                                    gpio_num_t miso_pin)
{
    return (sclk_pin != mosi_pin) &&
           (sclk_pin != miso_pin) &&
           (mosi_pin != miso_pin);
}

static bool spi_device_mode_is_valid(uint8_t mode)
{
    return mode <= 3U;
}

static esp_err_t spi_bus_configure_pins_hiz(gpio_num_t sclk_pin,
                                            gpio_num_t mosi_pin,
                                            gpio_num_t miso_pin)
{
    const gpio_config_t gpio_cfg = {
        .pin_bit_mask = (1ULL << sclk_pin) | (1ULL << mosi_pin) | (1ULL << miso_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&gpio_cfg);
}

bool spi_bus_is_initialized(const spi_bus_t *bus)
{
    return (bus != NULL) && bus->initialized;
}

esp_err_t spi_bus_init(spi_bus_t *bus, const spi_bus_cfg_t *cfg)
{
    esp_err_t err;

    if (bus == NULL || cfg == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (bus->initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!spi_bus_host_is_valid(cfg->host_id) ||
        !spi_bus_gpio_is_valid(cfg->sclk_pin) ||
        !spi_bus_gpio_is_valid(cfg->mosi_pin) ||
        !spi_bus_gpio_is_valid(cfg->miso_pin) ||
        !spi_bus_pins_are_unique(cfg->sclk_pin, cfg->mosi_pin, cfg->miso_pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    const spi_bus_config_t idf_cfg = {
        .mosi_io_num = cfg->mosi_pin,
        .miso_io_num = cfg->miso_pin,
        .sclk_io_num = cfg->sclk_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
    };

    err = spi_bus_initialize(cfg->host_id, &idf_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK)
    {
        ESP_LOGE(SPI_BUS_TAG, "spi_bus_initialize failed: %d", (int)err);
        return err;
    }

    bus->host_id = cfg->host_id;
    bus->sclk_pin = cfg->sclk_pin;
    bus->mosi_pin = cfg->mosi_pin;
    bus->miso_pin = cfg->miso_pin;
    bus->initialized = true;

    ESP_LOGI(SPI_BUS_TAG, "initialized: host=%d sclk=%d mosi=%d miso=%d",
             (int)bus->host_id,
             (int)bus->sclk_pin,
             (int)bus->mosi_pin,
             (int)bus->miso_pin);

    return ESP_OK;
}

esp_err_t spi_bus_deinit(spi_bus_t *bus)
{
    esp_err_t err;

    if (bus == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!bus->initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    err = spi_bus_free(bus->host_id);
    if (err != ESP_OK)
    {
        ESP_LOGE(SPI_BUS_TAG, "spi_bus_free failed: %d", (int)err);
        return err;
    }

    bus->initialized = false;
    ESP_LOGI(SPI_BUS_TAG, "deinitialized: host=%d", (int)bus->host_id);
    return ESP_OK;
}

esp_err_t spi_bus_set_pins_state(const spi_bus_t *bus,
                                 spi_bus_pins_state_t state)
{
    esp_err_t err;

    if (bus == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    switch (state)
    {
        case SPI_BUS_PINS_STATE_KEEP:
            return ESP_OK;

        case SPI_BUS_PINS_STATE_HIZ:
            if (!spi_bus_gpio_is_valid(bus->sclk_pin) ||
                !spi_bus_gpio_is_valid(bus->mosi_pin) ||
                !spi_bus_gpio_is_valid(bus->miso_pin))
            {
                ESP_LOGE(SPI_BUS_TAG, "invalid SPI pins for HIZ: sclk=%d mosi=%d miso=%d",
                         (int)bus->sclk_pin, (int)bus->mosi_pin, (int)bus->miso_pin);
                return ESP_ERR_INVALID_ARG;
            }

            err = spi_bus_configure_pins_hiz(bus->sclk_pin, bus->mosi_pin, bus->miso_pin);
            if (err != ESP_OK)
            {
                ESP_LOGE(SPI_BUS_TAG, "failed to set SPI pins to HIZ: sclk=%d mosi=%d miso=%d err=%d",
                         (int)bus->sclk_pin, (int)bus->mosi_pin, (int)bus->miso_pin, (int)err);
                return err;
            }

            ESP_LOGI(SPI_BUS_TAG, "pins set to HIZ: sclk=%d mosi=%d miso=%d",
                     (int)bus->sclk_pin, (int)bus->mosi_pin, (int)bus->miso_pin);
            return ESP_OK;

        default:
            return ESP_ERR_INVALID_ARG;
    }
}

bool spi_device_is_initialized(const spi_device_t *dev)
{
    return (dev != NULL) && dev->initialized;
}

esp_err_t spi_device_init(spi_device_t *dev, const spi_device_cfg_t *cfg)
{
    spi_device_handle_t handle = NULL;
    esp_err_t err;

    if (dev == NULL || cfg == NULL || cfg->bus == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (dev->initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!spi_bus_is_initialized(cfg->bus))
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!spi_bus_gpio_is_valid(cfg->cs_pin) ||
        !spi_device_mode_is_valid(cfg->mode) ||
        (cfg->clock_hz < SPI_DEVICE_MIN_CLOCK_HZ))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if ((cfg->cs_pin == cfg->bus->sclk_pin) ||
        (cfg->cs_pin == cfg->bus->mosi_pin) ||
        (cfg->cs_pin == cfg->bus->miso_pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    const spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = (int)cfg->clock_hz,
        .mode = cfg->mode,
        .spics_io_num = cfg->cs_pin,
        .queue_size = 1,
    };

    err = spi_bus_add_device(cfg->bus->host_id, &dev_cfg, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(SPI_BUS_TAG, "spi_bus_add_device failed: %d", (int)err);
        return err;
    }

    dev->bus = cfg->bus;
    dev->cs_pin = cfg->cs_pin;
    dev->mode = cfg->mode;
    dev->clock_hz = cfg->clock_hz;
    dev->platform_handle = handle;
    dev->initialized = true;

    ESP_LOGI(SPI_BUS_TAG, "device initialized: host=%d cs=%d mode=%u hz=%lu",
             (int)dev->bus->host_id,
             (int)dev->cs_pin,
             (unsigned)dev->mode,
             (unsigned long)dev->clock_hz);

    return ESP_OK;
}

esp_err_t spi_device_deinit(spi_device_t *dev)
{
    esp_err_t err;

    if (dev == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!dev->initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    err = spi_bus_remove_device(dev->platform_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(SPI_BUS_TAG, "spi_bus_remove_device failed: %d", (int)err);
        return err;
    }

    ESP_LOGI(SPI_BUS_TAG, "device deinitialized: host=%d cs=%d",
             dev->bus ? (int)dev->bus->host_id : -1,
             (int)dev->cs_pin);

    dev->initialized = false;
    dev->platform_handle = NULL;
    dev->bus = NULL;
    return ESP_OK;
}

esp_err_t spi_device_transfer(spi_device_t *dev,
                              const void *tx_data,
                              void *rx_data,
                              size_t length)
{
    spi_transaction_t trans = {0};
    esp_err_t err;

    if (dev == NULL || length == 0U || (tx_data == NULL && rx_data == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!spi_device_is_initialized(dev) ||
        dev->bus == NULL ||
        !spi_bus_is_initialized(dev->bus))
    {
        return ESP_ERR_INVALID_STATE;
    }

    trans.length = length * 8U;
    trans.tx_buffer = tx_data;
    trans.rx_buffer = rx_data;

    err = spi_device_transmit(dev->platform_handle, &trans);
    if (err != ESP_OK)
    {
        ESP_LOGE(SPI_BUS_TAG, "spi_device_transmit failed: %d", (int)err);
    }

    return err;
}
