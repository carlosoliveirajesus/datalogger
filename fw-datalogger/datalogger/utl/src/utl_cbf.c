#include "utl_cbf.h"

static inline uint16_t cbf_next(const utl_cbf_t* cb, uint16_t idx)
{
    return (uint16_t) ((idx + 1U) >= cb->size ? 0U : idx + 1U);
}

void utl_cbf_init(utl_cbf_t* cb, uint8_t* area, uint16_t area_size)
{
    cb->buf = area;
    cb->size = area_size;
    cb->head = 0;
    cb->tail = 0;
}

void utl_cbf_flush(utl_cbf_t* cb)
{
    cb->tail = cb->head;
}

uint16_t utl_cbf_bytes_available(const utl_cbf_t* cb)
{
    uint16_t head = cb->head;
    uint16_t tail = cb->tail;

    return (uint16_t) (head >= tail ? head - tail : cb->size - tail + head);
}

uint16_t utl_cbf_free_space(const utl_cbf_t* cb)
{
    return (uint16_t) (cb->size - 1U - utl_cbf_bytes_available(cb));
}

utl_cbf_status_t utl_cbf_put(utl_cbf_t* cb, uint8_t c)
{
    uint16_t next = cbf_next(cb, cb->head);

    if(next == cb->tail)
        return UTL_CBF_FULL;

    cb->buf[cb->head] = c;
    cb->head = next;

    return UTL_CBF_OK;
}

utl_cbf_status_t utl_cbf_get(utl_cbf_t* cb, uint8_t* c)
{
    if(cb->tail == cb->head)
        return UTL_CBF_EMPTY;

    *c = cb->buf[cb->tail];
    cb->tail = cbf_next(cb, cb->tail);

    return UTL_CBF_OK;
}
