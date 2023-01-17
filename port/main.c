/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Raj Nakarja - Silicon Witchery AB
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "py/compile.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/repl.h"
#include "py/runtime.h"
#include "py/stackctrl.h"

#include "shared/readline/readline.h"
#include "shared/runtime/pyexec.h"

#include "nrfx_log.h"
#include "nrf_sdm.h"
#include "nrfx_twi.h"
#include "nrfx_spim.h"
#include "nrfx_systick.h"
#include "nrfx_gpiote.h"
#include "nrfx_saadc.h"

#include "driver/battery.h"
#include "driver/bluetooth_data_protocol.h"
#include "driver/bluetooth_low_energy.h"
#include "driver/config.h"
#include "driver/ecx336cn.h"
#include "driver/flash.h"
#include "driver/fpga.h"
#include "driver/i2c.h"
#include "driver/iqs620.h"
#include "driver/max77654.h"
#include "driver/ov5640.h"
#include "driver/spi.h"
#include "driver/timer.h"

/** Variable that holds the Softdevice NVIC state.  */
nrf_nvic_state_t nrf_nvic_state = {{0}, 0};

/** This is the top of stack pointer as set in the nrf52832.ld file */
extern uint32_t _stack_top;

/** This is the bottom of stack pointer as set in the nrf52832.ld file */
extern uint32_t _stack_bot;

/** This is the start of the heap as set in the nrf52832.ld file */
extern uint32_t _heap_start;

/** This is the end of the heap as set in the nrf52832.ld file */
extern uint32_t _heap_end;

void nrfx_gpio_spi(void)
{
    // configure CS pin for the Display (for active low)
    nrf_gpio_pin_set(ECX336CN_CS_N_PIN);
    nrf_gpio_cfg_output(ECX336CN_CS_N_PIN);

    // for now, pull high to disable external flash chip
    nrf_gpio_pin_set(FLASH_CS_N_PIN);
    nrf_gpio_cfg_output(FLASH_CS_N_PIN);

    // for now, pull high to disable external flash chip
    nrf_gpio_pin_set(FPGA_CS_N_PIN);
    nrf_gpio_cfg_output(FPGA_CS_N_PIN);
}

void nrfx_gpio_fpga(void)
{
    // MODE1 set low for AUTOBOOT from FPGA internal flash
    nrf_gpio_pin_write(FPGA_MODE1_PIN, false);
    nrf_gpio_cfg(
        FPGA_MODE1_PIN,
        NRF_GPIO_PIN_DIR_OUTPUT,
        NRF_GPIO_PIN_INPUT_CONNECT,
        NRF_GPIO_PIN_NOPULL,
        NRF_GPIO_PIN_S0S1,
        NRF_GPIO_PIN_NOSENSE
    );

    // Let the FPGA start as soon as it has the power on.
    nrf_gpio_pin_write(FPGA_RECONFIG_N_PIN, true);
    nrf_gpio_cfg(
        FPGA_RECONFIG_N_PIN,
        NRF_GPIO_PIN_DIR_OUTPUT,
        NRF_GPIO_PIN_INPUT_CONNECT,
        NRF_GPIO_PIN_NOPULL,
        NRF_GPIO_PIN_S0S1,
        NRF_GPIO_PIN_NOSENSE
    );
}

/**
 * @brief Garbage collection route for nRF.
 */
void gc_collect(void)
{
    // start the GC
    gc_collect_start();

    // Get stack pointer
    uintptr_t sp;
    __asm__("mov %0, sp\n" : "=r"(sp));

    // Trace the stack, including the registers
    // (since they live on the stack in this function)
    gc_collect_root((void **)sp, ((uint32_t)&_stack_top - sp) / sizeof(uint32_t));

    // end the GC
    gc_collect_end();
}

/**
 * @brief Called if an exception is raised outside all C exception-catching handlers.
 */
NORETURN void nlr_jump_fail(void *val)
{
    (void)val;
    assert(!"exception raised without any handlers for it");
}

/**
 * @brief Main application called from Reset_Handler().
 */
int main(void)
{
    // All logging through SEGGER RTT interface
    SEGGER_RTT_Init();
    PRINTF("\r\n" "Brilliant Monocle " BUILD_VERSION " " GIT_COMMIT "\r\n\r\n");

    // Initialise drivers

    // Start by the Bluetooth driver, which will let the user scan for
    // a network, and by the time the negociation would happen, and the
    // host application gets started, this device would be likely ready.

    LOG("BLE");
    ble_init();

    // Start the drivers that only rely on the MCU peripherals.

    LOG("NRFX");
    nrfx_gpio_spi();
    nrfx_gpio_fpga();

    nrfx_systick_init();
    nrfx_gpiote_init(NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY);
    nrfx_saadc_init(NRFX_SAADC_DEFAULT_CONFIG_IRQ_PRIORITY);

    LOG("TIMER");
    timer_init();

    // Initialize the battery eary as it would check the remaining power
    // and eventually decide to go to sleep if too low.

    LOG("BATTERY");
    battery_init(MAX77654_ADC_PIN);

    // Setup the GPIO states before powering-on the chips,
    // to provide particular pin state at each chip's bootup.

    LOG("GPIO (OV5640");
    // Set to 0V = hold camera in reset.
    nrf_gpio_pin_write(OV5640_RESETB_N_PIN, false);
    nrf_gpio_cfg_output(OV5640_RESETB_N_PIN);
    // Set to 0V = not asserted.
    nrf_gpio_pin_write(OV5640_PWDN_PIN, false);
    nrf_gpio_cfg_output(OV5640_PWDN_PIN);

    LOG("GPIO (ECX336CN)");
    // Set to 0V on boot (datasheet p.11)
    nrf_gpio_pin_write(ECX336CN_XCLR_PIN, false);
    nrf_gpio_cfg_output(ECX336CN_XCLR_PIN);
    spi_chip_deselect(ECX336CN_CS_N_PIN);

    LOG("GPIO (FPGA)");
    // MODE1 set low for AUTOBOOT from FPGA internal flash
    nrf_gpio_pin_write(FPGA_MODE1_PIN, false);
    nrf_gpio_cfg_output(FPGA_MODE1_PIN);
    // Let the FPGA start as soon as it has the power on.
    nrf_gpio_pin_write(FPGA_RECONFIG_N_PIN, true);
    nrf_gpio_cfg_output(FPGA_RECONFIG_N_PIN);

    LOG("GPIO (FLASH)");
    // The FPGA might try to access this chip, let it do so.
    nrf_gpio_pin_write(FLASH_CS_N_PIN, true);
    nrf_gpio_cfg_output(FLASH_CS_N_PIN);

    // Setup the power rails over I2C through the PMIC.  This will first power
    // off everything as part of the PMIC's init(), then we power the rails one
    // by one and wait that each chip started before configuring them.
    // Do not start the 10V power rail yet.

    LOG("I2C");
    i2c_init(i2c0, I2C0_SCL_PIN, I2C0_SDA_PIN);
    i2c_init(i2c1, I2C1_SCL_PIN, I2C1_SDA_PIN);

    LOG("SPI");
    spi_init(spi2, SPI2_SCK_PIN, SPI2_MOSI_PIN, SPI2_MISO_PIN);

    LOG("MAX77654");
    max77654_init();
    max77654_rail_1v2(true);
    max77654_rail_1v8(true);
    max77654_rail_2v7(true);
    max77654_rail_vled(true);
    // Wait the power rail to stabilize, and other chips to boot.
    nrfx_systick_delay_ms(300);
    max77654_rail_10v(true);
    nrfx_systick_delay_ms(10);

    // Now we can setup the various peripherals over SPI, starting by the FPGA
    // upon which the other depend for their configuration.

    LOG("FPGA");
    fpga_init();

    LOG("ECX336CN");
    ecx336cn_init();

    LOG("OV5540");
    ov5640_init();

    // Finally initiate user-input related peripherals, which do not make
    // sense until everything else is setup.
    LOG("IQS620");
    iqs620_init();

    LOG("setup done");

    // Initialise the stack pointer for the main thread
    mp_stack_set_top(&_stack_top);

    // Set the stack limit as smaller than the real stack so we can recover
    mp_stack_set_limit((char *)&_stack_top - (char *)&_stack_bot - 400);

    // Initialise the garbage collector
    gc_init(&_heap_start, &_heap_end); // TODO optimize away GC if space needed later

    // Initialise the micropython runtime
    mp_init();

    // Initialise the readline module for REPL
    readline_init0();

    // If main.py exits, fallback to a REPL.
    //pyexec_frozen_module("main.py");

    // REPL mode can change, or it can request a soft reset
    for (int stop = false; !stop;)
    {
        if (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL)
        {
            stop = pyexec_raw_repl();
        }
        else
        {
            stop = pyexec_friendly_repl();
        }
        LOG("switching the interpreter mode");
    }

    // Deinitialize the board and power things off early
    max77654_power_off();

    // Garbage collection ready to exit
    gc_sweep_all(); // TODO optimize away GC if space needed later

    // Deinitialize the runtime.
    mp_deinit();

    // Stop the softdevice
    sd_softdevice_disable();

    // Reset chip
    NVIC_SystemReset();
}

NORETURN void __assert_func(const char *file, int line, const char *func, const char *expr)
{
    LOG("%s:%d: %s: %s", file, line, func, expr);
    for (;;) __asm__("bkpt");
}
