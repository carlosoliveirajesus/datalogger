/**
 * @file logger.c
 * @brief Gravação do CSV no cartão SD sobre o FatFs.
 */
#include "hal.h"
#include "ff.h"
#include "logger.h"

#define LOGGER_MAX_FILES 9999U
#define LOGGER_LINE_MAX  128U

_Static_assert((LOGGER_RING_SIZE & (LOGGER_RING_SIZE - 1U)) == 0U, "LOGGER_RING_SIZE deve ser potencia de 2");

/* Uma linha por amostra da IMU; o BME280 mede mais devagar, então cada linha
   repete a última leitura dele. Unidades no nome da coluna. */
#define LOGGER_CSV_HEADER \
    "t_ms,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,temp_c,press_hpa,hum_rh\r\n"

static FATFS logger_fs;
static FIL logger_file;
static bool logger_mounted = false;
static bool logger_open = false;
static char logger_name[LOGGER_NAME_MAX] = "";

/* Buffer circular ISR -> loop: só a ISR escreve head, só o loop escreve tail. */
static logger_imu_sample_t logger_ring[LOGGER_RING_SIZE];
static volatile uint32_t logger_head = 0;
static volatile uint32_t logger_tail = 0;
static volatile bool logger_accepting = false;

static uint8_t logger_block[LOGGER_BLOCK_SIZE];
static uint32_t logger_block_len = 0;
static uint32_t logger_sync_tmr = 0;

static float logger_acc_scale = 0.0f;
static float logger_gyr_scale = 0.0f;
static hal_env_data_t logger_env;
static bool logger_env_valid = false;

static logger_stats_t logger_stats;

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

/* ------------------------------------------------- formatação do CSV */

/* Escreve um valor com casas decimais fixas, sem printf (que usaria double
   em software e custaria caro a 416 linhas/s). */
static char* logger_put_fixed(char* p, float value, uint32_t decimals)
{
    static const float pow10[] = { 1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f };
    int32_t scaled = (int32_t) (value * pow10[decimals] + (value < 0.0f ? -0.5f : 0.5f));
    uint32_t mag = scaled < 0 ? (uint32_t) (-scaled) : (uint32_t) scaled;
    uint32_t div = (uint32_t) pow10[decimals];
    uint32_t ipart = mag / div;
    uint32_t fpart = mag % div;
    char tmp[12];
    int n = 0;

    if(scaled < 0)
        *p++ = '-';

    do
    {
        tmp[n++] = (char) ('0' + ipart % 10U);
        ipart /= 10U;
    } while(ipart != 0U);

    while(n > 0)
        *p++ = tmp[--n];

    if(decimals > 0U)
    {
        *p++ = '.';
        for(uint32_t d = decimals; d > 0U; d--)
        {
            p[d - 1U] = (char) ('0' + fpart % 10U);
            fpart /= 10U;
        }
        p += decimals;
    }

    return p;
}

/* t em ms com 3 casas (resolução de 1 µs), a partir dos µs desde o boot. */
static char* logger_put_time(char* p, uint64_t t_us)
{
    uint32_t ms = (uint32_t) (t_us / 1000U);
    uint32_t us = (uint32_t) (t_us % 1000U);
    char tmp[11];
    int n = 0;

    do
    {
        tmp[n++] = (char) ('0' + ms % 10U);
        ms /= 10U;
    } while(ms != 0U);

    while(n > 0)
        *p++ = tmp[--n];

    *p++ = '.';
    *p++ = (char) ('0' + us / 100U);
    *p++ = (char) ('0' + (us / 10U) % 10U);
    *p++ = (char) ('0' + us % 10U);

    return p;
}

static uint32_t logger_format_line(char* line, const logger_imu_sample_t* s)
{
    char* p = line;

    p = logger_put_time(p, s->t_us);
    *p++ = ',';
    p = logger_put_fixed(p, s->acc.x * logger_acc_scale, 4);
    *p++ = ',';
    p = logger_put_fixed(p, s->acc.y * logger_acc_scale, 4);
    *p++ = ',';
    p = logger_put_fixed(p, s->acc.z * logger_acc_scale, 4);
    *p++ = ',';
    p = logger_put_fixed(p, s->gyr.x * logger_gyr_scale, 2);
    *p++ = ',';
    p = logger_put_fixed(p, s->gyr.y * logger_gyr_scale, 2);
    *p++ = ',';
    p = logger_put_fixed(p, s->gyr.z * logger_gyr_scale, 2);
    *p++ = ',';

    /* Até a primeira leitura do BME280 as colunas dele ficam vazias. */
    if(logger_env_valid)
    {
        p = logger_put_fixed(p, logger_env.temp_c, 2);
        *p++ = ',';
        p = logger_put_fixed(p, logger_env.press_pa / 100.0f, 2);
        *p++ = ',';
        p = logger_put_fixed(p, logger_env.hum_rh, 1);
    }
    else
    {
        *p++ = ','; /* temp_c e press_hpa vazios; hum_rh vazio no fim */
        *p++ = ',';
    }

    *p++ = '\r';
    *p++ = '\n';

    return (uint32_t) (p - line);
}

/* --------------------------------------------------- escrita em blocos */

static FRESULT logger_write(const uint8_t* data, uint32_t len)
{
    UINT written = 0;
    FRESULT fr = f_write(&logger_file, data, (UINT) len, &written);

    if(fr == FR_OK && written != len)
        fr = FR_DENIED; /* f_write grava menos que o pedido quando o cartão enche */

    if(fr == FR_OK)
        logger_stats.bytes += len;

    return fr;
}

/* Copia a linha para o bloco; cada bloco cheio vira um f_write de 512 B. */
static FRESULT logger_append(const char* line, uint32_t len)
{
    while(len > 0U)
    {
        uint32_t chunk = MIN(len, LOGGER_BLOCK_SIZE - logger_block_len);

        memcpy(&logger_block[logger_block_len], line, chunk);
        logger_block_len += chunk;
        line += chunk;
        len -= chunk;

        if(logger_block_len == LOGGER_BLOCK_SIZE)
        {
            FRESULT fr = logger_write(logger_block, LOGGER_BLOCK_SIZE);

            logger_block_len = 0;
            if(fr != FR_OK)
                return fr;
        }
    }

    return FR_OK;
}

/* Grava o bloco parcial e atualiza o diretório/FAT no cartão. Depois disso
   um corte de energia perde no máximo LOGGER_SYNC_MS de dados. */
static FRESULT logger_flush_sync(void)
{
    FRESULT fr = FR_OK;

    if(logger_block_len > 0U)
    {
        fr = logger_write(logger_block, logger_block_len);
        logger_block_len = 0;
    }

    if(fr == FR_OK)
        fr = f_sync(&logger_file);

    logger_sync_tmr = hal_time_get_ms();

    return fr;
}

/* -------------------------------------------------- arquivo e volume */

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

logger_status_t logger_start(float acc_g_per_lsb, float gyr_dps_per_lsb)
{
    FRESULT fr;

    logger_stop();

    logger_acc_scale = acc_g_per_lsb;
    logger_gyr_scale = gyr_dps_per_lsb;
    memset(&logger_stats, 0, sizeof(logger_stats));
    logger_block_len = 0;

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

    fr = logger_write((const uint8_t*) LOGGER_CSV_HEADER, sizeof(LOGGER_CSV_HEADER) - 1U);
    logger_stats.bytes = 0;
    if(fr == FR_OK)
        fr = f_sync(&logger_file);

    if(fr != FR_OK)
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_LOGGER, "gravar cabecalho falhou (FRESULT %d)\r\n", (int) fr);
        return logger_fr_to_status(fr);
    }

    /* Descarta amostras antigas e começa a aceitar novas. */
    logger_tail = logger_head;
    logger_sync_tmr = hal_time_get_ms();
    logger_accepting = true;

    UTL_DBG_PRINTF(UTL_DBG_MOD_LOGGER, "Gravando em %s\r\n", logger_name);

    return LOGGER_OK;
}

void logger_stop(void)
{
    logger_accepting = false;

    if(logger_open)
    {
        /* Grava o que já está no buffer antes de fechar. */
        (void) logger_process();
        (void) logger_flush_sync();
        (void) f_close(&logger_file);
        UTL_DBG_PRINTF(UTL_DBG_MOD_LOGGER, "%s fechado: %lu linhas, %lu perdidas\r\n", logger_name,
                       (unsigned long) logger_stats.lines, (unsigned long) logger_stats.dropped);
    }

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

/* ------------------------------------------------------- amostras */

void logger_imu_push(const logger_imu_sample_t* sample)
{
    uint32_t head = logger_head;
    uint32_t used = head - logger_tail;

    if(!logger_accepting)
        return;

    if(used >= LOGGER_RING_SIZE)
    {
        logger_stats.dropped++;
        return;
    }

    logger_ring[head & (LOGGER_RING_SIZE - 1U)] = *sample;
    logger_head = head + 1U; /* publica depois de copiar */

    if(used + 1U > logger_stats.ring_max)
        logger_stats.ring_max = used + 1U;
}

void logger_env_set(const hal_env_data_t* env)
{
    logger_env = *env;
    logger_env_valid = true;
}

void logger_env_invalidate(void)
{
    logger_env_valid = false;
}

logger_status_t logger_process(void)
{
    char line[LOGGER_LINE_MAX];
    FRESULT fr = FR_OK;

    if(!logger_open)
        return LOGGER_OK;

    while(logger_tail != logger_head && fr == FR_OK)
    {
        uint32_t tail = logger_tail;
        uint32_t len = logger_format_line(line, &logger_ring[tail & (LOGGER_RING_SIZE - 1U)]);

        logger_tail = tail + 1U; /* libera a posição para a ISR */
        fr = logger_append(line, len);
        if(fr == FR_OK)
            logger_stats.lines++;
    }

    if(fr == FR_OK && hal_time_elapsed_since_ms(logger_sync_tmr) >= LOGGER_SYNC_MS)
        fr = logger_flush_sync();

    if(fr != FR_OK)
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_LOGGER, "escrita falhou (FRESULT %d)\r\n", (int) fr);
        logger_accepting = false;
    }

    return logger_fr_to_status(fr);
}

void logger_stats_get(logger_stats_t* stats)
{
    uint32_t cs = hal_cpu_critical_section_enter();
    *stats = logger_stats;
    hal_cpu_critical_section_leave(cs);
}
