/**
 * @file utl_cbf.h
 * @brief Buffer circular de bytes, um produtor e um consumidor (ISR → loop).
 *
 * Seguro sem seção crítica quando só a ISR chama utl_cbf_put() e só o loop
 * principal chama utl_cbf_get() (cada índice tem um único escritor).
 * Uma posição fica sempre vazia para distinguir cheio de vazio.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum utl_cbf_status_e
{
    UTL_CBF_OK = 0,
    UTL_CBF_FULL,
    UTL_CBF_EMPTY,
} utl_cbf_status_t;

typedef struct utl_cbf_s
{
    volatile uint16_t head; /**< próxima posição a escrever (produtor) */
    volatile uint16_t tail; /**< próxima posição a ler (consumidor)    */
    uint16_t size;          /**< tamanho da área (capacidade + 1)      */
    uint8_t* buf;
} utl_cbf_t;

/** Declara um buffer estático com capacidade de @p capacity bytes. */
#define UTL_CBF_DECLARE(name, capacity)            \
    static uint8_t name##_area[(capacity) + 1];    \
    static utl_cbf_t name = {                      \
        .head = 0,                                 \
        .tail = 0,                                 \
        .size = (capacity) + 1,                    \
        .buf = name##_area,                        \
    }

void utl_cbf_init(utl_cbf_t* cb, uint8_t* area, uint16_t area_size);
void utl_cbf_flush(utl_cbf_t* cb);
uint16_t utl_cbf_bytes_available(const utl_cbf_t* cb);
uint16_t utl_cbf_free_space(const utl_cbf_t* cb);
utl_cbf_status_t utl_cbf_put(utl_cbf_t* cb, uint8_t c);
utl_cbf_status_t utl_cbf_get(utl_cbf_t* cb, uint8_t* c);
