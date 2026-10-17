/**
 * @file stdout.c
 * @brief Saída de texto (utl_printf e printf da newlib) na serial de debug.
 */
#include "hal.h"

/* utl_printf() */
void _putchar(char character)
{
    hal_ser_write_byte(HAL_SER_DEV_DBG, (uint8_t) character);
}

/* printf() da newlib, via _write() do syscalls.c gerado. */
int __io_putchar(int ch)
{
    hal_ser_write_byte(HAL_SER_DEV_DBG, (uint8_t) ch);
    return ch;
}
