/**
 * @file utl_dbg.h
 * @brief Mensagens de debug por módulo, com timestamp, arquivo e linha.
 *
 * A lista de módulos (XMACRO_DBG_MODULES) fica em hal_cfg.h. Cada módulo
 * pode ser ligado e desligado em tempo de execução. Definir UTL_DBG_DISABLED
 * remove todas as mensagens na compilação.
 *
 * Exemplo de saída:  [1234][IMU][lsm6dsox.c:42] WHO_AM_I = 0x6C
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "hal_cfg.h"

typedef enum utl_dbg_mod_e
{
#define X(MOD, INDEX) MOD = INDEX,
    XMACRO_DBG_MODULES
#undef X
    UTL_DBG_NUM_MODS
} utl_dbg_mod_t;

void utl_dbg_init(void);
void utl_dbg_mod_enable(utl_dbg_mod_t mod);
void utl_dbg_mod_disable(utl_dbg_mod_t mod);
bool utl_dbg_mod_enabled(utl_dbg_mod_t mod);
const char* utl_dbg_mod_name_get(utl_dbg_mod_t mod);
const char* utl_dbg_base_name_get(const char* full_path);
void utl_dbg_dump(const uint8_t* data, size_t size);

/* hal_time_get_ms() é usado no cabeçalho das mensagens. */
uint32_t hal_time_get_ms(void);

#ifdef UTL_DBG_DISABLED

#define UTL_DBG_PRINTF(mod, fmt, ...) do { } while(0)
#define UTL_DBG_PRINTF_NH(mod, fmt, ...) do { } while(0)
#define UTL_DBG_DUMP(mod, data, size) do { } while(0)

#else

#define UTL_DBG_HEADER(mod, fmt) \
    "[%lu][%s][%s:%d] " fmt, (unsigned long) hal_time_get_ms(), utl_dbg_mod_name_get(mod), \
        utl_dbg_base_name_get(__FILE__), __LINE__

/** Mensagem com cabeçalho [ms][módulo][arquivo:linha]. */
#define UTL_DBG_PRINTF(mod, fmt, ...)                                 \
    do                                                                \
    {                                                                 \
        if(utl_dbg_mod_enabled(mod))                                  \
            utl_printf(UTL_DBG_HEADER(mod, fmt), ##__VA_ARGS__);      \
    } while(0)

/** Mensagem sem cabeçalho (continuação de linha, tabelas). */
#define UTL_DBG_PRINTF_NH(mod, fmt, ...)                              \
    do                                                                \
    {                                                                 \
        if(utl_dbg_mod_enabled(mod))                                  \
            utl_printf(fmt, ##__VA_ARGS__);                           \
    } while(0)

/** Dump hexadecimal de um bloco de memória. */
#define UTL_DBG_DUMP(mod, data, size)                                 \
    do                                                                \
    {                                                                 \
        if(utl_dbg_mod_enabled(mod))                                  \
            utl_dbg_dump((const uint8_t*) (data), (size));            \
    } while(0)

#endif
