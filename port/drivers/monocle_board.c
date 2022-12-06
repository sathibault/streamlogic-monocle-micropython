/*
 * Copyright (c) 2022 Brilliant Labs Limited
 * Licensed under the MIT License
 */

/**
 * General board setups.
 * @file monocle_board.c
 * @author Nathan Ashelman
 */

#include "monocle_battery.h"
#include "monocle_ble.h"
#include "monocle_board.h"
#include "monocle_config.h"
#include "monocle_ecx335af.h"
#include "monocle_flash.h"
#include "monocle_fpga.h"
#include "monocle_i2c.h"
#include "monocle_iqs620.h"
#include "monocle_max77654.h"
#include "monocle_ov5640.h"
#include "monocle_spi.h"
#include "nrfx_gpiote.h"
#include "nrfx_log.h"
#include "nrfx_systick.h"
#include <stdbool.h>

void board_power_on(void)
{
    // FPGA requires VCC(1.2V) before VCCX(2.7V) or VCCO(1.8V).
    max77654_rail_1v2(true);
    nrfx_systick_delay_ms(20);

    // Camera requires 1.8V before 2.7V.
    max77654_rail_1v8(true);
    nrfx_systick_delay_ms(20);

    max77654_rail_2v7(true);
    nrfx_systick_delay_ms(20);

    // Used by the display.
    max77654_rail_10v(true);
    nrfx_systick_delay_ms(20);

    // Used by the red and green LEDs.
    max77654_rail_vled(true);
    nrfx_systick_delay_ms(20);
}

static void board_power_off(void)
{
    max77654_rail_vled(false);
    nrfx_systick_delay_ms(10);
    max77654_rail_10v(false);
    nrfx_systick_delay_ms(10);
    max77654_rail_2v7(false);
    nrfx_systick_delay_ms(10);
    max77654_rail_1v8(false);
    nrfx_systick_delay_ms(10);
    max77654_rail_1v2(false);
}

/**
 * Initialises the hardware drivers and IO.
 */
void board_init(void)
{
    // Initialise SysTick ARM timer driver for nrfx_systick_delay_ms().
    nrfx_systick_init();

    // Initialise the GPIO driver with event support.
    nrfx_gpiote_init(NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY);

    // Custom wrapper around I2C used by the other drivers.
    i2c_init();

    // I2C-controlled PMIC, also controlling the red/green LEDs over I2C
    // Needs: i2c
    max77654_init();

    // I2C calls to make sure all power rails are turned off.
    // Needs: max77654
    board_power_off();

    // Initialise GPIO before the chips are powered on.
    ecx335af_prepare();
    fpga_prepare();
    ov5640_prepare();
    flash_prepare();

    // I2C calls to setup power rails of the MAX77654.
    // Needs: max77654
    board_power_on();

    // Initialise the Capacitive Touch Button controller over I2C.
    // Needs: i2c, gpiote
    iqs620_init();

    // Initialise the battery level sensing with the ADC.
    //battery_init();

    // Custom wrapper around SPI used by the other drivers.
    spi_init();

    // Initialise the FPGA: providing the clock for the display and screen.
    // Needs: power, spi
    fpga_init();
    fpga_xclk_on();

    // Initialise the Screen
    // Needs: power, spi, fpga
    ecx335af_init();
    fpga_disp_bars();

    // Initialise the Camera: startup sequence then I2C config.
    // Needs: power, fpga, i2c
    ov5640_init();
    ov5640_pwr_on();
    ov5640_light_mode(0);
    ov5640_color_saturation(3);
    ov5640_brightness(4);
    ov5640_contrast(3);
    ov5640_sharpness(33);
    ov5640_flip(1);

    // Initialise the SPI conection to the flash.
    // Needs: power
    flash_init();
}

void board_deinit(void)
{
    // Call deinit() hook of each driver.
    ecx335af_deinit();
    fpga_deinit();
    ov5640_deinit();

    // Turn the power rails off.
    board_power_off();
}