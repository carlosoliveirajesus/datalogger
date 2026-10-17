/**
 * @file utl.h
 * @brief Utilitários independentes de hardware (inclui todos de uma vez).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "hal_cfg.h"
#include "utl_printf.h"
#include "utl_cbf.h"
#include "utl_dbg.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#define UTL_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
