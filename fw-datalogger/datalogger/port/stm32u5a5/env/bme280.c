#include "bme280.h"

#define BME280_RESET_TIMEOUT_MS 10U

static bool bme280_probe(bme280_t* dev, uint8_t addr)
{
    uint8_t id = 0;

    if(!dev->bus->read(addr, BME280_REG_CHIP_ID, &id, 1))
        return false;

    dev->addr = addr;
    dev->chip_id = id;

    return true;
}

bool bme280_init(bme280_t* dev, const bme280_bus_t* bus)
{
    uint8_t cmd = BME280_RESET_CMD;
    uint8_t status = 0;

    dev->bus = bus;
    dev->addr = 0;
    dev->chip_id = 0;
    dev->initialized = false;

    if(!bme280_probe(dev, BME280_I2C_ADDR_PRIMARY) && !bme280_probe(dev, BME280_I2C_ADDR_SECONDARY))
        return false;

    if(dev->chip_id != BME280_CHIP_ID_VALUE)
        return false; /* 0x58 = BMP280 (sem umidade) */

    if(!bus->write(dev->addr, BME280_REG_RESET, &cmd, 1))
        return false;

    /* Após o reset o chip copia a calibração da NVM; espera im_update = 0. */
    for(uint32_t t = 0; t < BME280_RESET_TIMEOUT_MS; t++)
    {
        bus->delay_ms(1);
        if(bus->read(dev->addr, BME280_REG_STATUS, &status, 1) && (status & BME280_STATUS_IM_UPDATE) == 0U)
        {
            dev->initialized = true;
            break;
        }
    }

    /* TODO (passo 2): ler a calibração (0x88..0xA1 e 0xE1..0xE7). */

    return dev->initialized;
}

void bme280_deinit(bme280_t* dev)
{
    /* Após o reset o chip já fica em sleep mode. */
    dev->initialized = false;
}

/* TODO (passo 2): bme280_setup() (ctrl_hum, ctrl_meas, config) e
   bme280_read() (burst 0xF7..0xFE + compensação inteira do datasheet, 4.2.3). */
