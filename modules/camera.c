/*
 * This file is part of the MicroPython for Monocle project:
 *      https://github.com/brilliantlabsAR/monocle-micropython
 *
 * Authored by: Josuah Demangeon (me@josuah.net)
 *              Raj Nakarja / Brilliant Labs Ltd. (raj@itsbrilliant.co)
 *
 * ISC Licence
 *
 * Copyright © 2023 Brilliant Labs Ltd.
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

#include "nrf_gpio.h"
#include "nrfx_systick.h"
#include "py/runtime.h"

#include "monocle.h"
#include "camera-config.h"
#include "fpgalib.h"


STATIC mp_obj_t camera_power_on() {
  // Start the camera clock
  if (!fpga_has_feature(camera_feat))
    mp_raise_ValueError(MP_ERROR_TEXT("Camera not available"));

  uint8_t cam_dev = fpga_feature_dev(camera_feat);
  uint8_t command[2] = {cam_dev, 0x09};
  spi_write(FPGA, command, 2, false);

  // Reset sequence taken from Datasheet figure 2-3
  nrf_gpio_pin_write(CAMERA_RESET_PIN, false);
  nrf_gpio_pin_write(CAMERA_SLEEP_PIN, true);
  nrfx_systick_delay_ms(5); // t2
  nrf_gpio_pin_write(CAMERA_SLEEP_PIN, false);
  nrfx_systick_delay_ms(1); // t3
  nrf_gpio_pin_write(CAMERA_RESET_PIN, true);
  nrfx_systick_delay_ms(20); // t4

  // Read the camera CID (one of them)
  i2c_response_t resp = i2c_read(CAMERA_I2C_ADDRESS, 0x300A, 0xFF);
  if (resp.fail || resp.value != 0x56)
    {
      NRFX_LOG_ERROR("Error: Camera not found.");
      return mp_obj_new_bool(false);
    }

  // Software reset
  i2c_write(CAMERA_I2C_ADDRESS, 0x3008, 0xFF, 0x82);
  nrfx_systick_delay_ms(5);

  // Send the default configuration
  for (size_t i = 0;
       i < sizeof(camera_config) / sizeof(camera_config_t);
       i++)
    {
      i2c_write(CAMERA_I2C_ADDRESS,
		camera_config[i].address,
		0xFF,
		camera_config[i].value);
    }

  return mp_obj_new_bool(true);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(camera_power_on_obj, camera_power_on);

STATIC mp_obj_t camera_command(mp_obj_t cmd) {
  // Start the camera clock
  if (!fpga_has_feature(camera_feat))
    mp_raise_ValueError(MP_ERROR_TEXT("Camera not available"));

  uint8_t cam_dev = fpga_feature_dev(camera_feat);
  uint8_t command[2] = {cam_dev, mp_obj_get_int(cmd)};
  spi_write(FPGA, command, 2, false);
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(camera_command_obj, camera_command);

STATIC mp_obj_t camera_sleep(void)
{
    nrf_gpio_pin_write(CAMERA_SLEEP_PIN, true);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(camera_sleep_obj, camera_sleep);

STATIC mp_obj_t camera_wake(void)
{
    nrf_gpio_pin_write(CAMERA_SLEEP_PIN, false);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(camera_wake_obj, camera_wake);

STATIC mp_obj_t readout_dev(mp_obj_t id)
{
  int n = mp_obj_get_int(id);
  uint8_t dev = fpga_feature_dev(readout0_feat+n);
  return MP_OBJ_NEW_SMALL_INT(dev);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(readout_dev_obj, readout_dev);

STATIC const mp_rom_map_elem_t camera_module_globals_table[] = {

    {MP_ROM_QSTR(MP_QSTR_power_on), MP_ROM_PTR(&camera_power_on_obj)},
    {MP_ROM_QSTR(MP_QSTR_sleep), MP_ROM_PTR(&camera_sleep_obj)},
    {MP_ROM_QSTR(MP_QSTR_wake), MP_ROM_PTR(&camera_wake_obj)},
    {MP_ROM_QSTR(MP_QSTR_command), MP_ROM_PTR(&camera_command_obj)},
    {MP_ROM_QSTR(MP_QSTR_readout_dev), MP_ROM_PTR(&readout_dev_obj)},
};
STATIC MP_DEFINE_CONST_DICT(camera_module_globals, camera_module_globals_table);

const mp_obj_module_t camera_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&camera_module_globals,
};
MP_REGISTER_MODULE(MP_QSTR___camera, camera_module);
