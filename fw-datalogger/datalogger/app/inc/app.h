/**
 * @file app.h
 * @brief Aplicação do Datalogger: máquina de estados de alto nível.
 *
 * A app só usa a HAL (hal_*.h) e os utilitários (utl_*.h). Nada aqui inclui
 * main.h ou arquivos da ST.
 */
#pragma once

typedef enum app_state_e
{
    APP_STATE_INIT = 0, /**< sobe sensores e cartão                     */
    APP_STATE_IDLE,     /**< tudo OK, esperando para gravar             */
    APP_STATE_LOGGING,  /**< gravando CSV (passo 5)                     */
    APP_STATE_ERROR,    /**< falha; LED de erro indica a causa          */
} app_state_t;

/** Causas de erro, também usadas para o padrão do LED de erro. */
typedef enum app_error_e
{
    APP_ERROR_NONE = 0,
    APP_ERROR_IMU = (1 << 0),
    APP_ERROR_ENV = (1 << 1),
    APP_ERROR_SD_ABSENT = (1 << 2),
    APP_ERROR_SD_INIT = (1 << 3),
    APP_ERROR_SD_FS = (1 << 4),   /**< cartão sem FAT válido ou falha ao criar o arquivo */
    APP_ERROR_SD_FULL = (1 << 5), /**< cartão cheio */
} app_error_t;

void app_dbg_init(void);
void app_init(void);
void app_loop(void);
app_state_t app_state_get(void);
uint32_t app_errors_get(void);
