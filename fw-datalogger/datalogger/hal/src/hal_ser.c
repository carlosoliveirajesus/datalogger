#include "hal.h"

#if HAL_SER_ENABLED == 1

static hal_ser_driver_t* drv = &HAL_SER_DRIVER;

void hal_ser_init(void)
{
    drv->init();
}

void hal_ser_write(hal_ser_dev_t dev, const uint8_t* buffer, uint16_t len)
{
    if(dev < HAL_SER_NUM_DEVS)
        drv->write(dev, buffer, len);
}

void hal_ser_write_byte(hal_ser_dev_t dev, uint8_t c)
{
    if(dev < HAL_SER_NUM_DEVS)
        drv->write_byte(dev, c);
}

uint16_t hal_ser_bytes_available(hal_ser_dev_t dev)
{
    return dev < HAL_SER_NUM_DEVS ? drv->bytes_available(dev) : 0;
}

int16_t hal_ser_read_byte(hal_ser_dev_t dev, uint8_t* c)
{
    return dev < HAL_SER_NUM_DEVS ? drv->read_byte(dev, c) : 0;
}

void hal_ser_flush(hal_ser_dev_t dev)
{
    if(dev < HAL_SER_NUM_DEVS)
        drv->flush(dev);
}

#endif
