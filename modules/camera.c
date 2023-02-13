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
#include "nrfx_twi.h"

#include "driver/fpga.h"
#include "monocle.h"
// #include "driver/ov5640.h"
#include "driver/config.h"
#include "driver/bluetooth_data_protocol.h"

STATIC mp_obj_t mod_camera___init__(void)
{
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_camera___init___obj, mod_camera___init__);

STATIC mp_obj_t camera_capture(void)
{
    fpga_cmd(FPGA_CAMERA_CAPTURE);
    bluetooth_data_camera_capture("camera_shot.jpg", 100);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(camera_capture_obj, &camera_capture);

STATIC mp_obj_t camera_live(void)
{
    fpga_cmd(FPGA_CAMERA_START);
    fpga_cmd(FPGA_LIVEVIDEO_START);
    fpga_cmd(FPGA_LIVEVIDEO_REPLAY);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(camera_live_obj, &camera_live);

STATIC mp_obj_t camera_stop(void)
{
    fpga_cmd(FPGA_CAMERA_STOP);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(camera_stop_obj, &camera_stop);

STATIC const mp_rom_map_elem_t camera_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_camera)},
    {MP_ROM_QSTR(MP_QSTR___init__), MP_ROM_PTR(&mod_camera___init___obj)},

    // methods
    {MP_ROM_QSTR(MP_QSTR_capture), MP_ROM_PTR(&camera_capture_obj)},
    {MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&camera_stop_obj)},
    {MP_ROM_QSTR(MP_QSTR_live), MP_ROM_PTR(&camera_live_obj)},
};
STATIC MP_DEFINE_CONST_DICT(camera_module_globals, camera_module_globals_table);

const mp_obj_module_t camera_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&camera_module_globals,
};
MP_REGISTER_MODULE(MP_QSTR_camera, camera_module);
