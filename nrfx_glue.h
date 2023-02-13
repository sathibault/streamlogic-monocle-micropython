/*
 * This file is part of the MicroPython for Monocle project:
 *      https://github.com/brilliantlabsAR/monocle-micropython
 *
 * Authored by: Josuah Demangeon (me@josuah.net)
 *              Raj Nakarja / Brilliant Labs Inc (raj@itsbrilliant.co)
 *
 * ISC Licence
 *
 * Copyright © 2023 Brilliant Labs Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include "monocle.h"

#include "nrfx.h"
#include "nrf_nvic.h"
#include <soc/nrfx_coredep.h>
#include <soc/nrfx_atomic.h>

#define NRFX_ASSERT(expression)  \
    do                           \
    {                            \
        if ((expression) == 0)   \
        {                        \
            app_err(0xDEAD0A55); \
        }                        \
    } while (0)

#define NRFX_STATIC_ASSERT(expression) \
    _Static_assert(expression, "unspecified message")

#define NRFX_IRQ_PRIORITY_SET(irq_number, priority) \
    sd_nvic_SetPriority(irq_number, priority)

#define NRFX_IRQ_ENABLE(irq_number) \
    sd_nvic_EnableIRQ(irq_number)

#define NRFX_IRQ_IS_ENABLED(irq_number) \
    (0 != (NVIC->ISER[irq_number / 32] & (1UL << (irq_number % 32))))

#define NRFX_IRQ_DISABLE(irq_number) \
    sd_nvic_DisableIRQ(irq_number)

#define NRFX_IRQ_PENDING_SET(irq_number) \
    sd_nvic_SetPendingIRQ(irq_number)

#define NRFX_IRQ_PENDING_CLEAR(irq_number) \
    sd_nvic_ClearPendingIRQ(irq_number)

#define NRFX_IRQ_IS_PENDING(irq_number) \
    NVIC_GetPendingIRQ(irq_number)

#define NRFX_CRITICAL_SECTION_ENTER()       \
    {                                       \
        uint8_t _is_nested_critical_region; \
        sd_nvic_critical_region_enter(&_is_nested_critical_region);

#define NRFX_CRITICAL_SECTION_EXIT()                          \
    sd_nvic_critical_region_exit(_is_nested_critical_region); \
    }

#define NRFX_DELAY_DWT_BASED 0

#define NRFX_DELAY_US(us_time) \
    nrfx_coredep_delay_us(us_time)

#define nrfx_atomic_t nrfx_atomic_u32_t

#define NRFX_ATOMIC_FETCH_STORE(p_data, value) \
    nrfx_atomic_u32_fetch_store(p_data, value)

#define NRFX_ATOMIC_FETCH_OR(p_data, value) \
    nrfx_atomic_u32_fetch_or(p_data, value)

#define NRFX_ATOMIC_FETCH_AND(p_data, value) \
    nrfx_atomic_u32_fetch_and(p_data, value)

#define NRFX_ATOMIC_FETCH_XOR(p_data, value) \
    nrfx_atomic_u32_fetch_xor(p_data, value)

#define NRFX_ATOMIC_FETCH_ADD(p_data, value) \
    nrfx_atomic_u32_fetch_add(p_data, value)

#define NRFX_ATOMIC_FETCH_SUB(p_data, value) \
    nrfx_atomic_u32_fetch_sub(p_data, value)

#define NRFX_CUSTOM_ERROR_CODES 0

#define NRFX_EVENT_READBACK_ENABLED 1

#define NRFX_DPPI_CHANNELS_USED 0

#define NRFX_DPPI_GROUPS_USED 0

#define NRFX_PPI_CHANNELS_USED 0

#define NRFX_PPI_GROUPS_USED 0

#define NRFX_GPIOTE_CHANNELS_USED 0

#define NRFX_EGUS_USED 0

#define NRFX_TIMERS_USED 0
