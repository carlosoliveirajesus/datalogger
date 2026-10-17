#include "utl.h"

#define DBG_BYTES_PER_LINE 16

/* Nome curto de cada módulo: "UTL_DBG_MOD_IMU" -> "IMU". */
static const char* const dbg_mod_names[] = {
#define X(MOD, INDEX) [INDEX] = #MOD,
    XMACRO_DBG_MODULES
#undef X
};

static uint32_t dbg_mods_enabled = 0;

_Static_assert(UTL_DBG_NUM_MODS <= 32, "no maximo 32 modulos de debug");

void utl_dbg_init(void)
{
    dbg_mods_enabled = 0;
}

void utl_dbg_mod_enable(utl_dbg_mod_t mod)
{
    if(mod < UTL_DBG_NUM_MODS)
        dbg_mods_enabled |= (1UL << mod);
}

void utl_dbg_mod_disable(utl_dbg_mod_t mod)
{
    if(mod < UTL_DBG_NUM_MODS)
        dbg_mods_enabled &= ~(1UL << mod);
}

bool utl_dbg_mod_enabled(utl_dbg_mod_t mod)
{
    return (mod < UTL_DBG_NUM_MODS) && (dbg_mods_enabled & (1UL << mod));
}

const char* utl_dbg_mod_name_get(utl_dbg_mod_t mod)
{
    static const char prefix[] = "UTL_DBG_MOD_";

    if(mod >= UTL_DBG_NUM_MODS || dbg_mod_names[mod] == NULL)
        return "?";

    return dbg_mod_names[mod] + sizeof(prefix) - 1;
}

const char* utl_dbg_base_name_get(const char* full_path)
{
    const char* base = full_path;

    for(const char* p = full_path; *p; p++)
    {
        if(*p == '/' || *p == '\\')
            base = p + 1;
    }

    return base;
}

void utl_dbg_dump(const uint8_t* data, size_t size)
{
    for(size_t pos = 0; pos < size; pos += DBG_BYTES_PER_LINE)
    {
        utl_printf("%08lX: ", (unsigned long) pos);

        for(size_t n = 0; n < DBG_BYTES_PER_LINE; n++)
        {
            if(pos + n < size)
                utl_printf("%02X ", data[pos + n]);
            else
                utl_printf("   ");
        }

        utl_printf(" ");

        for(size_t n = 0; n < DBG_BYTES_PER_LINE && pos + n < size; n++)
        {
            uint8_t c = data[pos + n];
            utl_printf("%c", (c >= 0x20 && c < 0x7F) ? c : '.');
        }

        utl_printf("\r\n");
    }
}
