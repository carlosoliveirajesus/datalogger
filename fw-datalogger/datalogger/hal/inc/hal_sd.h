/**
 * @file hal_sd.h
 * @brief Cartão SD como dispositivo de blocos de 512 B.
 *
 * É a interface que o diskio.c do FatFS vai usar; a app não fala com o
 * cartão diretamente, só pelo FatFS.
 */
#pragma once

#define HAL_SD_BLOCK_SIZE 512U

typedef enum hal_sd_status_e
{
    HAL_SD_OK = 0,
    HAL_SD_ERR_NO_CARD,
    HAL_SD_ERR_INIT,
    HAL_SD_ERR_IO,
    HAL_SD_ERR_TIMEOUT,
    HAL_SD_ERR_PARAM,
} hal_sd_status_t;

typedef struct hal_sd_info_s
{
    uint32_t block_count; /**< número de blocos de 512 B */
    uint32_t block_size;
} hal_sd_info_t;

typedef struct hal_sd_driver_s
{
    bool (*is_present)(void);
    hal_sd_status_t (*init)(void);
    void (*deinit)(void);
    bool (*is_ready)(void);
    hal_sd_status_t (*read)(uint8_t* buf, uint32_t block, uint32_t count);
    hal_sd_status_t (*write)(const uint8_t* buf, uint32_t block, uint32_t count);
    hal_sd_status_t (*info_get)(hal_sd_info_t* info);
} hal_sd_driver_t;

/** Lê o pino de card detect. */
bool hal_sd_is_present(void);
/** Inicializa o barramento e o cartão. Falha sem travar se não houver cartão. */
hal_sd_status_t hal_sd_init(void);
void hal_sd_deinit(void);
/** true quando o cartão está inicializado e pronto para transferência. */
bool hal_sd_is_ready(void);
hal_sd_status_t hal_sd_read(uint8_t* buf, uint32_t block, uint32_t count);
hal_sd_status_t hal_sd_write(const uint8_t* buf, uint32_t block, uint32_t count);
hal_sd_status_t hal_sd_info_get(hal_sd_info_t* info);
