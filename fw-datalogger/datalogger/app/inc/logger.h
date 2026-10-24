/**
 * @file logger.h
 * @brief Arquivo CSV no cartão SD (FatFs): um arquivo novo por boot.
 *
 * Passo 4: montar o volume, criar LOG_nnnn.CSV e gravar o cabeçalho.
 * Passo 5: gravação das amostras (buffer de 512 B, f_sync periódico).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define LOGGER_NAME_MAX 13 /* "LOG_0001.CSV" + '\0' */

typedef enum logger_status_e
{
    LOGGER_OK = 0,
    LOGGER_ERR_MOUNT,  /**< cartão sem FAT válido (formatar em FAT32) */
    LOGGER_ERR_FULL,   /**< sem espaço, ou já existem os 9999 arquivos */
    LOGGER_ERR_IO,     /**< falha de leitura/escrita no cartão */
} logger_status_t;

/** Monta o volume, cria o próximo LOG_nnnn.CSV livre e grava o cabeçalho. */
logger_status_t logger_start(void);
/** Fecha o arquivo e desmonta o volume (ex.: antes de tentar o cartão de novo). */
void logger_stop(void);
bool logger_is_open(void);
/** Nome do arquivo aberto (vazio se não houver). */
const char* logger_file_name(void);
