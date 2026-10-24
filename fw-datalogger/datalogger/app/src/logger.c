/**
 * @file logger.c
 * @brief Arquivo CSV no cartão SD sobre o FatFs.
 */
#include "hal.h"
#include "ff.h"
#include "logger.h"

#define LOGGER_MAX_FILES 9999U

/* Uma linha por amostra da IMU; o BME280 mede mais devagar, então cada linha
   repete a última leitura dele. Unidades no nome da coluna. */
#define LOGGER_CSV_HEADER \
    "t_ms,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,temp_c,press_hpa,hum_rh\r\n"

static FATFS logger_fs;
static FIL logger_file;
static bool logger_mounted = false;
static bool logger_open = false;
static char logger_name[LOGGER_NAME_MAX] = "";

static logger_status_t logger_fr_to_status(FRESULT fr)
{
    switch(fr)
    {
        case FR_OK:
            return LOGGER_OK;
        case FR_NO_FILESYSTEM:
        case FR_INVALID_DRIVE:
        case FR_NOT_ENABLED:
            return LOGGER_ERR_MOUNT;
        case FR_DENIED:
            return LOGGER_ERR_FULL;
        default:
            return LOGGER_ERR_IO;
    }
}

/* Cria o primeiro LOG_nnnn.CSV que ainda não existe. FA_CREATE_NEW falha com
   FR_EXIST se o nome já estiver em uso, então não é preciso listar o diretório. */
static FRESULT logger_create_next(void)
{
    FRESULT fr = FR_EXIST;

    for(uint32_t n = 1; n <= LOGGER_MAX_FILES && fr == FR_EXIST; n++)
    {
        utl_snprintf(logger_name, sizeof(logger_name), "LOG_%04lu.CSV", (unsigned long) n);
        fr = f_open(&logger_file, logger_name, FA_CREATE_NEW | FA_WRITE);
    }

    if(fr == FR_EXIST)
        fr = FR_DENIED; /* os 9999 nomes já estão em uso */

    if(fr != FR_OK)
        logger_name[0] = '\0';

    return fr;
}

logger_status_t logger_start(void)
{
    FRESULT fr;
    UINT written = 0;

    logger_stop();

    fr = f_mount(&logger_fs, "", 1); /* 1 = monta agora e valida o FAT */
    if(fr != FR_OK)
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_LOGGER, "f_mount falhou (FRESULT %d)\r\n", (int) fr);
        return logger_fr_to_status(fr);
    }
    logger_mounted = true;

    fr = logger_create_next();
    if(fr != FR_OK)
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_LOGGER, "criar arquivo falhou (FRESULT %d)\r\n", (int) fr);
        return logger_fr_to_status(fr);
    }
    logger_open = true;

    fr = f_write(&logger_file, LOGGER_CSV_HEADER, sizeof(LOGGER_CSV_HEADER) - 1U, &written);
    if(fr == FR_OK && written != sizeof(LOGGER_CSV_HEADER) - 1U)
        fr = FR_DENIED; /* cartão cheio */
    if(fr == FR_OK)
        fr = f_sync(&logger_file);

    if(fr != FR_OK)
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_LOGGER, "gravar cabecalho falhou (FRESULT %d)\r\n", (int) fr);
        return logger_fr_to_status(fr);
    }

    UTL_DBG_PRINTF(UTL_DBG_MOD_LOGGER, "Arquivo %s criado\r\n", logger_name);

    return LOGGER_OK;
}

void logger_stop(void)
{
    if(logger_open)
        (void) f_close(&logger_file);

    if(logger_mounted)
        (void) f_mount(NULL, "", 0);

    logger_open = false;
    logger_mounted = false;
}

bool logger_is_open(void)
{
    return logger_open;
}

const char* logger_file_name(void)
{
    return logger_name;
}
