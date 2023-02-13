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

#include <stddef.h>

#include "py/obj.h"
#include "py/objarray.h"
#include "py/runtime.h"

#include "nrfx_log.h"
#include "nrfx_spim.h"

#include "driver/config.h"

// TODO use a header
void spi_chip_select(uint8_t cs_pin);
void spi_chip_deselect(uint8_t cs_pin);
void spi_read(uint8_t *buf, size_t len);
void spi_write(uint8_t const *buf, size_t len);

void fpga_cmd_write(uint16_t cmd, const uint8_t *buf, size_t len)
{
    uint8_t cmd_buf[2] = {cmd >> 8, cmd >> 0};

    spi_chip_select(FPGA_CS_PIN);
    spi_write(cmd_buf, sizeof cmd_buf);
    spi_write(buf, len);
    spi_chip_deselect(FPGA_CS_PIN);
}

void fpga_cmd_read(uint16_t cmd, uint8_t *buf, size_t len)
{
    uint8_t cmd_buf[2] = {(cmd >> 8) & 0xFF, (cmd >> 0) & 0xFF};

    spi_chip_select(FPGA_CS_PIN);
    spi_write(cmd_buf, sizeof cmd_buf);
    spi_read(buf, len);
    spi_chip_deselect(FPGA_CS_PIN);
}

STATIC mp_obj_t mod_fpga___init__(void)
{
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_fpga___init___obj, mod_fpga___init__);

static inline void spi_write_u16(uint16_t u16)
{
    uint8_t buf[2] = {u16 >> 8, u16 >> 0};
    spi_write(buf, sizeof buf);
}

STATIC mp_obj_t fpga_read(mp_obj_t addr_in, mp_obj_t len_in)
{
    uint16_t addr = mp_obj_get_int(addr_in);
    size_t len = mp_obj_get_int(len_in);

    // Allocate a buffer for reading data into
    uint8_t *buf = m_malloc(len);

    // Create a list where we'll return the bytes
    mp_obj_t return_list = mp_obj_new_list(0, NULL);

    // Read on the SPI using the command and address given
    fpga_cmd_read(addr, buf, len);

    // Copy the read bytes into the list object
    for (size_t i = 0; i < len; i++)
    {
        mp_obj_list_append(return_list, MP_OBJ_NEW_SMALL_INT(buf[i]));
    }

    // Free the temporary buffer
    m_free(buf);

    // Return the list
    return MP_OBJ_FROM_PTR(return_list);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(fpga_read_obj, fpga_read);

STATIC mp_obj_t fpga_write(mp_obj_t addr_in, mp_obj_t list_in)
{
    uint16_t addr = mp_obj_get_int(addr_in);

    // Extract the buffer of elements and size from the python object.
    size_t len = 0;
    mp_obj_t *list = NULL;
    mp_obj_list_get(list_in, &len, &list);

    // Create a contiguous region with the bytes to read in.
    uint8_t *buf = m_malloc(len);

    // Copy the write bytes into the a continuous buffer
    for (size_t i = 0; i < len; i++)
    {
        buf[i] = mp_obj_get_int(list[i]);
    }

    // Fetch the buffer content from SPI
    fpga_cmd_write(addr, buf, len);

    // Free the temporary buffer
    m_free(buf);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(fpga_write_obj, &fpga_write);

STATIC mp_obj_t fpga_status(void)
{
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(fpga_status_obj, &fpga_status);

STATIC const mp_rom_map_elem_t fpga_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_fpga)},
    {MP_ROM_QSTR(MP_QSTR___init__), MP_ROM_PTR(&mod_fpga___init___obj)},

    // methods
    {MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&fpga_write_obj)},
    {MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&fpga_read_obj)},
    {MP_ROM_QSTR(MP_QSTR_status), MP_ROM_PTR(&fpga_status_obj)},
};
STATIC MP_DEFINE_CONST_DICT(fpga_module_globals, fpga_module_globals_table);

const mp_obj_module_t fpga_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&fpga_module_globals,
};
MP_REGISTER_MODULE(MP_QSTR_fpga, fpga_module);
