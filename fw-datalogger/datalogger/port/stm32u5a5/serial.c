/**
 * @file serial.c
 * @brief Port de hal_ser para o STM32U5A5 (HAL da ST).
 *
 * TX bloqueante. RX por interrupção, um byte por vez, para um buffer
 * circular. A recepção só funciona com "USART1 global interrupt" ligado no
 * CubeMX (NVIC); sem isso o TX funciona normalmente.
 */
#include "main.h"
#include "usart.h"
#include "hal.h"

#define SER_TX_TIMEOUT_MS 100U

typedef struct port_ser_s
{
    UART_HandleTypeDef* huart;
    utl_cbf_t* rx_cbf;
    uint8_t rx_byte;
} port_ser_t;

UTL_CBF_DECLARE(ser_dbg_rx, HAL_SER_DBG_RX_BUF_SIZE);

static port_ser_t ports[HAL_SER_NUM_DEVS] = {
    [HAL_SER_DEV_DBG] = { .huart = &huart1, .rx_cbf = &ser_dbg_rx },
};

static void port_ser_rx_start(port_ser_t* p)
{
    (void) HAL_UART_Receive_IT(p->huart, &p->rx_byte, 1);
}

static void port_ser_init(void)
{
    /* As UARTs já foram inicializadas pelo CubeMX (MX_USARTx_UART_Init). */
    for(uint32_t n = 0; n < HAL_SER_NUM_DEVS; n++)
    {
        utl_cbf_flush(ports[n].rx_cbf);
        port_ser_rx_start(&ports[n]);
    }
}

static void port_ser_write(hal_ser_dev_t dev, const uint8_t* buffer, uint16_t len)
{
    (void) HAL_UART_Transmit(ports[dev].huart, buffer, len, SER_TX_TIMEOUT_MS);
}

static void port_ser_write_byte(hal_ser_dev_t dev, uint8_t c)
{
    port_ser_write(dev, &c, 1);
}

static uint16_t port_ser_bytes_available(hal_ser_dev_t dev)
{
    return utl_cbf_bytes_available(ports[dev].rx_cbf);
}

static int16_t port_ser_read_byte(hal_ser_dev_t dev, uint8_t* c)
{
    return utl_cbf_get(ports[dev].rx_cbf, c) == UTL_CBF_OK ? 1 : 0;
}

static void port_ser_flush(hal_ser_dev_t dev)
{
    utl_cbf_flush(ports[dev].rx_cbf);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart)
{
    for(uint32_t n = 0; n < HAL_SER_NUM_DEVS; n++)
    {
        if(ports[n].huart == huart)
        {
            (void) utl_cbf_put(ports[n].rx_cbf, ports[n].rx_byte);
            port_ser_rx_start(&ports[n]);
            break;
        }
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart)
{
    /* Overrun/ruído: descarta e volta a receber. */
    for(uint32_t n = 0; n < HAL_SER_NUM_DEVS; n++)
    {
        if(ports[n].huart == huart)
        {
            port_ser_rx_start(&ports[n]);
            break;
        }
    }
}

hal_ser_driver_t HAL_SER_DRIVER = {
    .init = port_ser_init,
    .write = port_ser_write,
    .write_byte = port_ser_write_byte,
    .bytes_available = port_ser_bytes_available,
    .read_byte = port_ser_read_byte,
    .flush = port_ser_flush,
};
