/**
 * @file sd.c
 * @brief Port de hal_sd para o STM32U5A5: SDMMC1 em 4 bits, modo polling.
 *
 * IMPORTANTE: o MX_SDMMC1_SD_Init() gerado chama Error_Handler() (trava) se
 * não houver cartão. No CubeMX, em Project Manager > Advanced Settings,
 * marque "Do Not Generate Function Call" para MX_SDMMC1_SD_Init: quem
 * inicializa o cartão é este port, que só retorna erro.
 */
#include "main.h"
#include "sdmmc.h"
#include "hal.h"

#define SD_IO_TIMEOUT_MS    1000U
#define SD_READY_TIMEOUT_MS 1000U

static bool sd_ready = false;

#if HAL_SD_BUS_WIDTH == 1
#define SD_BUS_WIDE SDMMC_BUS_WIDE_1B
#elif HAL_SD_BUS_WIDTH == 4
#define SD_BUS_WIDE SDMMC_BUS_WIDE_4B
#else
#error "HAL_SD_BUS_WIDTH deve ser 1 ou 4 (hal_cfg.h)"
#endif

static bool port_sd_is_present(void)
{
#if HAL_SD_CD_ENABLED == 1
    return hal_gpio_get(HAL_GPIO_SD_CD) == HAL_SD_CD_PRESENT_LVL;
#else
    return true; /* sem card detect: quem descobre a ausência é o init */
#endif
}

static hal_sd_status_t port_sd_wait_transfer(void)
{
    uint32_t start = HAL_GetTick();

    while(HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
    {
        if((HAL_GetTick() - start) > SD_READY_TIMEOUT_MS)
            return HAL_SD_ERR_TIMEOUT;
    }

    return HAL_SD_OK;
}

#if HAL_SD_DIAG_ENABLED == 1
/* Diagnóstico de bancada: depois de uma falha no HAL_SD_Init(), refaz o
   início da sequência de power-up (DS SD Physical Layer, 4.2) passo a passo
   e imprime o resultado de cada comando. O SDMMC continua ligado no clock de
   init depois da falha. Resultados esperados com cartão SDHC saudável:
   CMD0 err=0, CMD8 err=0 resp=0x000001AA, CMD55 err=0, ACMD41 err=0. */
static void port_sd_diag(void)
{
    uint32_t err;
    uint32_t resp;

    /* Nível dos pinos parados (IDR vale também em função alternativa).
       Com os pull-ups, CMD e D0 devem ler 1; 0 = curto para GND ou fio errado. */
    UTL_DBG_PRINTF(UTL_DBG_MOD_SD, "diag pinos  CMD(PD2)=%d D0(PC8)=%d\r\n",
                   HAL_GPIO_ReadPin(SD_CMD_GPIO_Port, SD_CMD_Pin) == GPIO_PIN_SET,
                   HAL_GPIO_ReadPin(SD_D0_GPIO_Port, SD_D0_Pin) == GPIO_PIN_SET);

    err = SDMMC_CmdGoIdleState(SDMMC1);
    UTL_DBG_PRINTF(UTL_DBG_MOD_SD, "diag CMD0   err=0x%08lX\r\n", (unsigned long) err);

    err = SDMMC_CmdOperCond(SDMMC1);
    resp = SDMMC_GetResponse(SDMMC1, SDMMC_RESP1);
    UTL_DBG_PRINTF(UTL_DBG_MOD_SD, "diag CMD8   err=0x%08lX resp=0x%08lX\r\n", (unsigned long) err,
                   (unsigned long) resp);

    err = SDMMC_CmdAppCommand(SDMMC1, 0);
    resp = SDMMC_GetResponse(SDMMC1, SDMMC_RESP1);
    UTL_DBG_PRINTF(UTL_DBG_MOD_SD, "diag CMD55  err=0x%08lX resp=0x%08lX\r\n", (unsigned long) err,
                   (unsigned long) resp);

    if(err == 0U)
    {
        err = SDMMC_CmdAppOperCommand(SDMMC1, SDMMC_VOLTAGE_WINDOW_SD | SDMMC_HIGH_CAPACITY);
        resp = SDMMC_GetResponse(SDMMC1, SDMMC_RESP1);
        UTL_DBG_PRINTF(UTL_DBG_MOD_SD, "diag ACMD41 err=0x%08lX resp=0x%08lX\r\n", (unsigned long) err,
                       (unsigned long) resp);
    }
}
#endif

static hal_sd_status_t port_sd_init(void)
{
    sd_ready = false;

    /* Largura e clock vêm do hal_cfg.h; o init do cartão roda sempre a
       400 kHz em 1 bit e o HAL_SD_Init() muda depois para estes valores. */
    hsd1.Instance = SDMMC1;
    hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd1.Init.BusWide = SD_BUS_WIDE;
    hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd1.Init.ClockDiv = HAL_SD_CLOCK_DIV;

    if(HAL_SD_Init(&hsd1) != HAL_OK)
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_SD, "HAL_SD_Init falhou (erro 0x%08lX)\r\n", (unsigned long) hsd1.ErrorCode);
#if HAL_SD_DIAG_ENABLED == 1
        port_sd_diag();
#endif
        return HAL_SD_ERR_INIT;
    }

    if(port_sd_wait_transfer() != HAL_SD_OK)
        return HAL_SD_ERR_TIMEOUT;

    sd_ready = true;

    return HAL_SD_OK;
}

static void port_sd_deinit(void)
{
    (void) HAL_SD_DeInit(&hsd1);
    sd_ready = false;
}

static bool port_sd_is_ready(void)
{
    return sd_ready && port_sd_is_present();
}

static hal_sd_status_t port_sd_read(uint8_t* buf, uint32_t block, uint32_t count)
{
    if(!port_sd_is_ready())
        return HAL_SD_ERR_NO_CARD;

    if(HAL_SD_ReadBlocks(&hsd1, buf, block, count, SD_IO_TIMEOUT_MS) != HAL_OK)
        return HAL_SD_ERR_IO;

    return port_sd_wait_transfer();
}

static hal_sd_status_t port_sd_write(const uint8_t* buf, uint32_t block, uint32_t count)
{
    if(!port_sd_is_ready())
        return HAL_SD_ERR_NO_CARD;

    if(HAL_SD_WriteBlocks(&hsd1, (uint8_t*) buf, block, count, SD_IO_TIMEOUT_MS) != HAL_OK)
        return HAL_SD_ERR_IO;

    return port_sd_wait_transfer();
}

static hal_sd_status_t port_sd_info_get(hal_sd_info_t* info)
{
    HAL_SD_CardInfoTypeDef ci;

    if(!port_sd_is_ready())
        return HAL_SD_ERR_NO_CARD;

    if(HAL_SD_GetCardInfo(&hsd1, &ci) != HAL_OK)
        return HAL_SD_ERR_IO;

    info->block_count = ci.LogBlockNbr;
    info->block_size = ci.LogBlockSize;

    return HAL_SD_OK;
}

hal_sd_driver_t HAL_SD_DRIVER = {
    .is_present = port_sd_is_present,
    .init = port_sd_init,
    .deinit = port_sd_deinit,
    .is_ready = port_sd_is_ready,
    .read = port_sd_read,
    .write = port_sd_write,
    .info_get = port_sd_info_get,
};
