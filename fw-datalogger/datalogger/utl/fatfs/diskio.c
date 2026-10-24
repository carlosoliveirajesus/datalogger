/**
 * @file diskio.c
 * @brief Camada de disco do FatFs sobre o hal_sd (cartão SD em blocos de 512 B).
 *
 * Única ligação entre o FatFs (genérico, do ChaN) e o hardware: o FatFs chama
 * estas funções e elas chamam o hal_sd, que por sua vez usa o port do SDMMC.
 * Uma unidade só (pdrv 0).
 */
#include "hal.h"
#include "ff.h"
#include "diskio.h"

static DSTATUS disk_stat = STA_NOINIT;

DSTATUS disk_status(BYTE pdrv)
{
    if(pdrv != 0U)
        return STA_NOINIT;

    if(!hal_sd_is_ready())
        disk_stat |= STA_NOINIT;

    return disk_stat;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if(pdrv != 0U)
        return STA_NOINIT;

    /* Normalmente a app já iniciou o cartão; só tenta de novo se não estiver pronto. */
    if(hal_sd_is_ready() || hal_sd_init() == HAL_SD_OK)
        disk_stat &= (DSTATUS) ~STA_NOINIT;
    else
        disk_stat |= STA_NOINIT;

    return disk_stat;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
    if(pdrv != 0U || count == 0U)
        return RES_PARERR;

    if(disk_stat & STA_NOINIT)
        return RES_NOTRDY;

    return hal_sd_read(buff, (uint32_t) sector, (uint32_t) count) == HAL_SD_OK ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
    if(pdrv != 0U || count == 0U)
        return RES_PARERR;

    if(disk_stat & STA_NOINIT)
        return RES_NOTRDY;

    return hal_sd_write(buff, (uint32_t) sector, (uint32_t) count) == HAL_SD_OK ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
    hal_sd_info_t info;

    if(pdrv != 0U)
        return RES_PARERR;

    if(disk_stat & STA_NOINIT)
        return RES_NOTRDY;

    switch(cmd)
    {
        case CTRL_SYNC:
            /* hal_sd_write() só retorna depois de o cartão voltar ao estado
               TRANSFER, então não há escrita pendente. */
            return RES_OK;

        case GET_SECTOR_COUNT:
            if(hal_sd_info_get(&info) != HAL_SD_OK)
                return RES_ERROR;
            *(LBA_t*) buff = (LBA_t) info.block_count;
            return RES_OK;

        case GET_SECTOR_SIZE:
            *(WORD*) buff = (WORD) HAL_SD_BLOCK_SIZE;
            return RES_OK;

        case GET_BLOCK_SIZE:
            *(DWORD*) buff = 1U; /* tamanho do bloco de apagamento desconhecido */
            return RES_OK;

        default:
            return RES_PARERR;
    }
}
