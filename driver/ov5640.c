/*
 * This file is part of the MicroPython for Monocle:
 *      https://github.com/Itsbrilliantlabs/monocle-micropython
 *
 * Authored by: Shreyas Hemachandra <shreyas.hemachandran@gmail.com>
 * Authored by: Nathan Ashelman <nathan@itsbrilliant.co>
 * Authored by: Josuah Demangeon <me@josuah.net>
 *
 * ISC Licence
 *
 * Copyright © 2022 Brilliant Labs Inc.
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

/**
 * OV5640 camera module driver.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nrfx_systick.h"
#include "nrfx_log.h"
#include "nrfx_twi.h"

#include "critical_functions.h"

#include "driver/config.h"
#include "driver/ov5640_data.h"
#include "driver/ov5640.h"
#include "driver/timer.h"

#define LEN(x) (sizeof(x) / sizeof *(x))

/**
 * Swap the byte order.
 * @param in Input usinged value.
 * @return Swapped order value.
 */
static uint16_t __bswap_16(uint16_t in)
{
    return (in << 8) | (in >> 8);
}

/**
 * Set the chip on or off by changing its reset pin.
 * @param n True for on.
 */
static inline void ov5640_set_reset(bool state)
{
    nrf_gpio_pin_write(CAMERA_RESET_PIN, !state);
}

/*
 * Control the power state on/off of the chip.
 * @param n True for on.
 */
static void ov5640_set_power(bool state)
{
    nrf_gpio_pin_write(CAMERA_SLEEP_PIN, !state);
}

/**
 * Read a byte of data from the provided registry address.
 * @param reg Register address (16 bit)
 * @return uint8_t A byte read data
 */
static uint8_t ov5640_read_reg(uint16_t reg)
{
    uint8_t w_buf[2];
    uint8_t ret_val = 0;
    uint16_t swaped = __bswap_16(reg);

    memcpy(w_buf, &swaped, 2);
    // if (!i2c_write(OV5640_I2C, CAMERA_I2C_ADDRESS, w_buf, 2))
    // return 0;
    // if (!i2c_read(OV5640_I2C, CAMERA_I2C_ADDRESS, &ret_val, 1))
    // return 0;
    return ret_val;
}

/**
 * Revert the configuration of the camera module.
 */
void ov5640_deinit(void)
{
    nrf_gpio_cfg_default(CAMERA_RESET_PIN);
    nrf_gpio_cfg_default(CAMERA_SLEEP_PIN);
}

/**
 * Put camera into low-power mode, preserving configuration.
 */
void ov5640_pwr_sleep(void)
{
    ov5640_set_power(true);
}

/**
 * Wake camera up from low-power mode, prior configuration still valid.
 */
void ov5640_pwr_wake(void)
{
    ov5640_set_power(true);
}

/**
 * Change ov5640 config for 1x zoom.
 */
void ov5640_mode_1x(void)
{
    // use group write to update the group of registers in the same frame
    // (guaranteed to be written prior to the internal latch at the frame boundary).
    // see Datasheet section 2.6
    ov5640_write_reg(0x3212, 0x03); // start group 3 -- for some reason this makes transition worse!
    for (size_t i = 0; i < LEN(ov5640_rgb565_1x_tbl); i++)
        ov5640_write_reg(ov5640_rgb565_1x_tbl[i].addr, ov5640_rgb565_1x_tbl[i].value);
    ov5640_write_reg(0x3212, 0x13); // end group 3
    ov5640_write_reg(0x3212, 0xA3); // launch group 3
}

/**
 * NWA: change ov5640 config for 2x zoom, also used for 4x.
 */
void ov5640_mode_2x(void)
{
    // start group 3
    ov5640_write_reg(0x3212, 0x03);

    for (size_t i = 0; i < LEN(ov5640_rgb565_2x_tbl); i++)
        ov5640_write_reg(ov5640_rgb565_2x_tbl[i].addr, ov5640_rgb565_2x_tbl[i].value);

    // end group 3
    ov5640_write_reg(0x3212, 0x13);

    // launch group 3
    ov5640_write_reg(0x3212, 0xA3);
}

/**
 * Reduce camera output size.
 * @pre ov5640_rgb565_mode_1x() or _2x() should have already been called
 * @param h_pixels desired horizontal output resolution (should be <= 640)
 * @param v_pixels desired vertical output resolution (should be <= 400)
 * @todo This function has not been tested, but uses code from >
 * @todo Implement error checking on inputs, return a success/failure code>
 */
void ov5640_reduce_size(uint16_t h_pixels, uint16_t v_pixels)
{
    ov5640_write_reg(0x3212, 0x03); // start group 3

    ov5640_write_reg(0x3808, h_pixels >> 8);   // DVPHO, upper byte
    ov5640_write_reg(0x3809, h_pixels & 0xFF); // DVPHO, lower byte
    ov5640_write_reg(0x380a, v_pixels >> 8);   // DVPVO, upper byte
    ov5640_write_reg(0x380b, v_pixels & 0xFF); // DVPVO, lower byte

    // end group 3
    ov5640_write_reg(0x3212, 0x13);

    // launch group 3
    ov5640_write_reg(0x3212, 0xA3);
}

/** AWB Light mode config [0..4] [Auto, Sunny, Office, Cloudy, Home]. */
const static uint8_t ov5640_lightmode_tbl[5][7] =
    {
        {0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00},
        {0x06, 0x1C, 0x04, 0x00, 0x04, 0xF3, 0x01},
        {0x05, 0x48, 0x04, 0x00, 0x07, 0xCF, 0x01},
        {0x06, 0x48, 0x04, 0x00, 0x04, 0xD3, 0x01},
        {0x04, 0x10, 0x04, 0x00, 0x08, 0x40, 0x01},
};

/**
 * Configures AWB gain control.
 * @param mode Refer `AWB Light mode config`
 */
void ov5640_light_mode(uint8_t mode)
{
    ov5640_write_reg(0x3212, 0x03); // start group 3
    for (int i = 0; i < 7; i++)
        ov5640_write_reg(0x3400 + i, ov5640_lightmode_tbl[mode][i]);
    ov5640_write_reg(0x3212, 0x13); // end group 3
    ov5640_write_reg(0x3212, 0xA3); // launch group 3
}

/** Color saturation config [0..6] [-3, -2, -1, 0, 1, 2, 3]> */
const static uint8_t OV5640_SATURATION_TBL[7][6] =
    {
        {0x0C, 0x30, 0x3D, 0x3E, 0x3D, 0x01},
        {0x10, 0x3D, 0x4D, 0x4E, 0x4D, 0x01},
        {0x15, 0x52, 0x66, 0x68, 0x66, 0x02},
        {0x1A, 0x66, 0x80, 0x82, 0x80, 0x02},
        {0x1F, 0x7A, 0x9A, 0x9C, 0x9A, 0x02},
        {0x24, 0x8F, 0xB3, 0xB6, 0xB3, 0x03},
        {0x2B, 0xAB, 0xD6, 0xDA, 0xD6, 0x04},
};

/**
 * Configures color saturation.
 * @param sat Refer Color saturation config
 */
void ov5640_color_saturation(uint8_t sat)
{
    ov5640_write_reg(0x3212, 0x03); // start group 3
    ov5640_write_reg(0x5381, 0x1C);
    ov5640_write_reg(0x5382, 0x5A);
    ov5640_write_reg(0x5383, 0x06);
    for (int i = 0; i < 6; i++)
        ov5640_write_reg(0x5384 + i, OV5640_SATURATION_TBL[sat][i]);
    ov5640_write_reg(0x538b, 0x98);
    ov5640_write_reg(0x538a, 0x01);
    ov5640_write_reg(0x3212, 0x13); // end group 3
    ov5640_write_reg(0x3212, 0xA3); // launch group 3
}

/**
 * Configures brightness
 * @param bright Range 0 - 8
 */
void ov5640_brightness(uint8_t bright)
{
    uint8_t brtval = (bright < 4) ? 4 - bright : bright - 4;

    ov5640_write_reg(0x3212, 0x03); // start group 3
    ov5640_write_reg(0x5587, brtval << 4);
    if (bright < 4)
        ov5640_write_reg(0x5588, 0x09);
    else
        ov5640_write_reg(0x5588, 0x01);
    ov5640_write_reg(0x3212, 0x13); // end group 3
    ov5640_write_reg(0x3212, 0xA3); // launch group 3
}

/**
 * Configures contrast.
 * @param contrast Range 0 - 6.
 */
void ov5640_contrast(uint8_t contrast)
{
    uint8_t reg0val = 0x00; // contrast = 3
    uint8_t reg1val = 0x20;
    switch (contrast)
    {
    case 0: // -3
        reg1val = reg0val = 0x14;
        break;
    case 1: // -2
        reg1val = reg0val = 0x18;
        break;
    case 2: // -1
        reg1val = reg0val = 0x1C;
        break;
    case 4: // 1
        reg0val = 0x10;
        reg1val = 0x24;
        break;
    case 5: // 2
        reg0val = 0x18;
        reg1val = 0x28;
        break;
    case 6: // 3
        reg0val = 0x1C;
        reg1val = 0x2C;
        break;
    }
    ov5640_write_reg(0x3212, 0x03); // start group 3
    ov5640_write_reg(0x5585, reg0val);
    ov5640_write_reg(0x5586, reg1val);
    ov5640_write_reg(0x3212, 0x13); // end group 3
    ov5640_write_reg(0x3212, 0xA3); // launch group 3
}

/**
 * Configures sharpness.
 * @param sharp Range 0 to 33, 0: off 33: auto.
 */
void ov5640_sharpness(uint8_t sharp)
{
    if (sharp < 33)
    {
        ov5640_write_reg(0x5308, 0x65);
        ov5640_write_reg(0x5302, sharp);
    }
    else
    {
        ov5640_write_reg(0x5308, 0x25);
        ov5640_write_reg(0x5300, 0x08);
        ov5640_write_reg(0x5301, 0x30);
        ov5640_write_reg(0x5302, 0x10);
        ov5640_write_reg(0x5303, 0x00);
        ov5640_write_reg(0x5309, 0x08);
        ov5640_write_reg(0x530a, 0x30);
        ov5640_write_reg(0x530b, 0x04);
        ov5640_write_reg(0x530c, 0x06);
    }
}
/** Effect configs [0..6] [Normal (off), Blueish (cool light), Redish (warm), Black & White, Sepia, Negative, Greenish] */
const static uint8_t ov5640_effects_tbl[7][3] =
    {
        {0x06, 0x40, 0x10},
        {0x1E, 0xA0, 0x40},
        {0x1E, 0x80, 0xC0},
        {0x1E, 0x80, 0x80},
        {0x1E, 0x40, 0xA0},
        {0x40, 0x40, 0x10},
        {0x1E, 0x60, 0x60},
};

/**
 * Configures effects.
 * @param eft refer `Effect configs`
 */
void ov5640_special_effects(uint8_t eft)
{
    ov5640_write_reg(0x3212, 0x03); // start group 3
    ov5640_write_reg(0x5580, ov5640_effects_tbl[eft][0]);
    ov5640_write_reg(0x5583, ov5640_effects_tbl[eft][1]); // sat U
    ov5640_write_reg(0x5584, ov5640_effects_tbl[eft][2]); // sat V
    ov5640_write_reg(0x5003, 0x08);
    ov5640_write_reg(0x3212, 0x13); // end group 3
    ov5640_write_reg(0x3212, 0xA3); // launch group 3
}

/**
 * Controls flash.
 * @param sw 0: off 1: on
 */
void ov5640_flash_ctrl(bool on)
{
    ov5640_write_reg(0x3016, 0x02);
    ov5640_write_reg(0x301C, 0x02);
    ov5640_write_reg(0x3019, on ? 0x02 : 0x00);
}

/**
 * Mirrors camera output (horizontal flip).
 * @param on 1 to turn on, 0 to turn off
 */
void ov5640_mirror(bool on)
{
    uint8_t reg = ov5640_read_reg(0x3821);
    ov5640_write_reg(0x3821, on ? (reg | 0x06) : (reg & 0xF9));
}

/**
 * Flips camera output (vertical flip).
 * @param on 1 to turn on, 0 to turn off
 */
void ov5640_flip(bool on)
{
    uint8_t reg = ov5640_read_reg(0x3820);
    ov5640_write_reg(0x3820, (on) ? (reg | 0x06) : (reg & 0xF9));
}

/**
 * Configures window size.
 * @param offx Offset X
 * @param offy Offset Y
 * @param width Prescale width
 * @param height Presclae height
 */
void ov5640_outsize_set(uint16_t offx, uint16_t offy, uint16_t width, uint16_t height)
{
    ov5640_write_reg(0x3212, 0x03);

    // Set pre-scaling size
    ov5640_write_reg(0x3808, width >> 8);
    ov5640_write_reg(0x3809, width & 0xFF);
    ov5640_write_reg(0x380a, height >> 8);
    ov5640_write_reg(0x380b, height & 0xFF);

    // Set ofsset
    ov5640_write_reg(0x3810, offx >> 8);
    ov5640_write_reg(0x3811, offx & 0xFF);

    ov5640_write_reg(0x3812, offy >> 8);
    ov5640_write_reg(0x3813, offy & 0xFF);

    ov5640_write_reg(0x3212, 0x13);
    ov5640_write_reg(0x3212, 0xA3);
}

/**
 * Focus init transfer camera module firmware.
 */
void ov5640_focus_init(void)
{
    // reset MCU
    ov5640_write_reg(0x3000, 0x20);

    // program ov5640 MCU firmware
    for (size_t i = 0; i < LEN(ov5640_af_config_tbl); i++)
        ov5640_write_reg(0x8000 + i, ov5640_af_config_tbl[i]);

    ov5640_write_reg(0x3022, 0x00); // ? undocumented
    ov5640_write_reg(0x3023, 0x00); // ?
    ov5640_write_reg(0x3024, 0x00); // ?
    ov5640_write_reg(0x3025, 0x00); // ?
    ov5640_write_reg(0x3026, 0x00); // ?
    ov5640_write_reg(0x3027, 0x00); // ?
    ov5640_write_reg(0x3028, 0x00); // ?
    ov5640_write_reg(0x3029, 0x7F); // ?
    ov5640_write_reg(0x3000, 0x00); // enable MCU
}

/**
 * Init the camera.
 * Trigger initialisation of the chip, controlling its reset and power pins.
 */
void ov5640_init(void)
{
    // i2c_scan(i2c1);

    // Check the chip ID
    uint16_t id = ov5640_read_reg(OV5640_CHIPIDH) << 8 | ov5640_read_reg(OV5640_CHIPIDL);
    app_err(id == OV5640_ID ? NRFX_SUCCESS : 1);

    ov5640_reduce_size(640, 400);
    ov5640_mode_1x();

    ov5640_focus_init();
    ov5640_light_mode(0);
    ov5640_color_saturation(3);
    ov5640_brightness(4);
    ov5640_contrast(3);
    ov5640_sharpness(33);
    ov5640_flip(true);
}
