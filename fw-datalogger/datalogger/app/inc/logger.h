/**
 * @file logger.h
 * @brief Gravação do CSV no cartão SD (FatFs): um arquivo novo por boot.
 *
 * Fluxo:
 *   ISR do INT1  -> logger_imu_push()   amostra da IMU no buffer circular
 *   loop         -> logger_env_set()    última leitura do BME280
 *   loop         -> logger_process()    tira amostras do buffer, formata o CSV,
 *                                       grava em blocos de 512 B e faz f_sync
 *                                       periódico
 *
 * O buffer circular desacopla a aquisição da escrita: o cartão pode demorar
 * dezenas de ms numa escrita sem que amostras se percam.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "hal.h"

#define LOGGER_NAME_MAX   13    /* "LOG_0001.CSV" + '\0' */
#define LOGGER_RING_SIZE  1024U /* amostras; potência de 2 (~2,4 s a 416 Hz) */
#define LOGGER_BLOCK_SIZE 512U  /* tamanho de cada f_write */
#define LOGGER_SYNC_MS    1000U /* f_sync no máximo a cada 1 s */

typedef enum logger_status_e
{
    LOGGER_OK = 0,
    LOGGER_ERR_MOUNT, /**< cartão sem FAT válido (formatar em FAT32) */
    LOGGER_ERR_FULL,  /**< sem espaço, ou já existem os 9999 arquivos */
    LOGGER_ERR_IO,    /**< falha de leitura/escrita no cartão */
} logger_status_t;

/** Uma amostra da IMU, como sai da ISR. */
typedef struct logger_imu_sample_s
{
    uint64_t t_us;
    hal_imu_raw_t acc;
    hal_imu_raw_t gyr;
} logger_imu_sample_t;

typedef struct logger_stats_s
{
    uint32_t lines;      /**< linhas gravadas no arquivo atual */
    uint32_t dropped;    /**< amostras perdidas por buffer cheio */
    uint32_t ring_max;   /**< maior ocupação do buffer (amostras) */
    uint32_t bytes;      /**< bytes gravados (sem o cabeçalho) */
} logger_stats_t;

/**
 * Monta o volume, cria o próximo LOG_nnnn.CSV livre, grava o cabeçalho e
 * passa a aceitar amostras. As escalas convertem o valor bruto da IMU em g e
 * dps.
 */
logger_status_t logger_start(float acc_g_per_lsb, float gyr_dps_per_lsb);
/** Grava o que estiver no buffer, fecha o arquivo e desmonta o volume. */
void logger_stop(void);
bool logger_is_open(void);
const char* logger_file_name(void);

/** Chamado na ISR: guarda uma amostra (descarta e conta se o buffer estiver cheio). */
void logger_imu_push(const logger_imu_sample_t* sample);
/** Última leitura do BME280, repetida em cada linha até a próxima. */
void logger_env_set(const hal_env_data_t* env);
/** BME280 sem resposta: as colunas dele ficam vazias até o próximo logger_env_set(). */
void logger_env_invalidate(void);
/** Trabalho do loop: esvazia o buffer no arquivo. Retorna erro se a escrita falhar. */
logger_status_t logger_process(void);
void logger_stats_get(logger_stats_t* stats);
