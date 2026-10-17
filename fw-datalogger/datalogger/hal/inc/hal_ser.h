/**
 * @file hal_ser.h
 * @brief Portas seriais (HAL_SER_PORTS em hal_cfg.h).
 *
 * A transmissão é bloqueante. A recepção usa interrupção e um buffer
 * circular por porta (precisa da interrupção da USART ligada no CubeMX).
 */
#pragma once

typedef enum hal_ser_dev_e
{
    HAL_SER_PORTS,
    HAL_SER_NUM_DEVS,
} hal_ser_dev_t;

typedef struct hal_ser_driver_s
{
    void (*init)(void);
    void (*write)(hal_ser_dev_t dev, const uint8_t* buffer, uint16_t len);
    void (*write_byte)(hal_ser_dev_t dev, uint8_t c);
    uint16_t (*bytes_available)(hal_ser_dev_t dev);
    int16_t (*read_byte)(hal_ser_dev_t dev, uint8_t* c);
    void (*flush)(hal_ser_dev_t dev);
} hal_ser_driver_t;

void hal_ser_init(void);
void hal_ser_write(hal_ser_dev_t dev, const uint8_t* buffer, uint16_t len);
void hal_ser_write_byte(hal_ser_dev_t dev, uint8_t c);
uint16_t hal_ser_bytes_available(hal_ser_dev_t dev);
/** Retorna 1 se leu um byte, 0 se o buffer estava vazio. */
int16_t hal_ser_read_byte(hal_ser_dev_t dev, uint8_t* c);
void hal_ser_flush(hal_ser_dev_t dev);
