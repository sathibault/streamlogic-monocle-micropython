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
#include <math.h>

#include "py/compile.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/repl.h"
#include "py/runtime.h"
#include "py/stackctrl.h"

#include "shared/readline/readline.h"
#include "shared/runtime/pyexec.h"
#include "genhdr/mpversion.h"

#include "nrfx.h"

#include "nrfx_log.h"
#include "nrf_sdm.h"
#include "nrf_power.h"
#include "nrfx_twi.h"
#include "nrfx_spim.h"
#include "nrfx_systick.h"
#include "nrfx_gpiote.h"
#include "nrfx_saadc.h"
#include "nrfx_rtc.h"
#include "nrfx_glue.h"

#include "driver/battery.h"
#include "driver/bluetooth_data_protocol.h"
#include "driver/bluetooth_low_energy.h"
#include "driver/config.h"
#include "driver/ecx336cn.h"
#include "driver/flash.h"
#include "driver/fpga.h"
#include "critical_functions.h"
#include "driver/iqs620.h"
#include "driver/max77654.h"
#include "driver/ov5640.h"
#include "driver/spi.h"
#include "driver/timer.h"

/** Variable that holds the Softdevice NVIC state.  */
// nrf_nvic_state_t nrf_nvic_state = {{0}, 0};

/** This is the top of stack pointer as set in the nrf52832.ld file */
extern uint32_t _stack_top;

/** This is the bottom of stack pointer as set in the nrf52832.ld file */
extern uint32_t _stack_bot;

/** This is the start of the heap as set in the nrf52832.ld file */
extern uint32_t _heap_start;

/** This is the end of the heap as set in the nrf52832.ld file */
extern uint32_t _heap_end;

/** Encode the number of the peripheral that starts as a blinking pattern. */
static uint8_t assert_blink_num;

/**
 * @brief Garbage collection route for nRF.
 */
void gc_collect(void)
{
    // start the GC
    gc_collect_start();

    // Get stack pointer
    uintptr_t sp;
    __asm__("mov %0, sp\n"
            : "=r"(sp));

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
    app_err(1); // exception raised without any handlers for it
    while (1)
    {
    }
}

void ble_on_connect(void)
{
    // max77654_led_green(false);
}

void blink(uint8_t num)
{
    for (size_t i = 0; i < num; i++)
    {
        // max77654_led_red(true);
        nrfx_systick_delay_ms(50);
        // max77654_led_red(false);
        nrfx_systick_delay_ms(100);
    }
}

NORETURN void __assert_func(const char *file, int line, const char *func, const char *expr)
{
    // upon an assert failure, alert that something went wrong
    for (;;)
    {
        // repeatedly display the error message, which helps a bit with RTT
        log("%s:%d: %s: %s", file, line, func, expr);

        blink(assert_blink_num);
        nrfx_systick_delay_ms(1000);
    }
}

// void charge_status_timer(void)
// {
//     // Divide the timer frequency a bit, let it overflow
//     // then check for the PMIC charge status.
//     if (max77654_is_connected_to_charger())
//     {
//         // Warn that we are going to sleep
//         max77654_rail_vled(true);
//         max77654_led_red(true);

//         // Power everything around off.
//         max77654_power_off();

//         // Wakeup from events of IQS620 touch controller .
//         nrf_gpio_cfg_sense_input(TOUCH_INTERRUPT_PIN, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);

//         // Power the SoftDevice and the whole chip off.
//         sd_power_system_off();
//         nrf_power_system_off(NRF_POWER);
//     }
// }

/**
 * @brief Main application called from Reset_Handler().
 */
int main(void)
{
    SEGGER_RTT_Init();
    log_clear();
    log("MicroPython on Monocle - " BUILD_VERSION " (" MICROPY_GIT_HASH ") ");

    setup_pmic_and_sleep_mode();

    while (1)
    {
    }

    // Turn on the SoftDevice early to allow use of softdevice-dependent calls
    // before the full Bluetooth Low Energy stack is setup (to save power).

    // Setup the GPIO states before powering-on the chips,
    // to provide particular pin state at each chip's bootup.

    log("GPIO");
    {
        // OV5640

        // Set to 0V = hold camera in reset.
        nrf_gpio_pin_write(OV5640_RESETB_N_PIN, false);
        nrf_gpio_cfg_output(OV5640_RESETB_N_PIN);

        // Set to 0V = not asserted.
        nrf_gpio_pin_write(OV5640_PWDN_PIN, false);
        nrf_gpio_cfg_output(OV5640_PWDN_PIN);

        // ECX336CN

        // configure CS pin for the Display (for active low)
        nrf_gpio_pin_set(ECX336CN_CS_N_PIN);
        nrf_gpio_cfg_output(ECX336CN_CS_N_PIN);

        // Set low on boot (datasheet p.11)
        nrf_gpio_pin_write(ECX336CN_XCLR_PIN, false);
        nrf_gpio_cfg_output(ECX336CN_XCLR_PIN);

        // FPGA

        // MODE1 set low for AUTOBOOT from FPGA internal flash
        nrf_gpio_pin_write(FPGA_MODE1_PIN, false);
        nrf_gpio_cfg_output(FPGA_MODE1_PIN);

        // Let the FPGA start as soon as it has the power on.
        nrf_gpio_pin_write(FPGA_RECONFIG_N_PIN, true);
        nrf_gpio_cfg_output(FPGA_RECONFIG_N_PIN);

        // FLASH

        // The FPGA might try to access this chip, let it do so.
        nrf_gpio_pin_write(FLASH_CS_N_PIN, true);
        nrf_gpio_cfg_output(FLASH_CS_N_PIN);
    }

    // Initialize the battery now that the MAX77654 is configured,
    // and check the battery charge status immediately.

    log("BATTERY");
    {
        // The battery module needs the ADC setup.
        nrfx_saadc_init(NRFX_SAADC_DEFAULT_CONFIG_IRQ_PRIORITY);

        // Periodically check the charger connection status.
        // charge_status_timer();
        // timer_add_task(timer_500ms, charge_status_timer);

        battery_init(MAX77654_ADC_PIN);
    }

    // Initialise SoftDevice, used as an intermediate layer for many things below.

    // Start the drivers that only rely on the MCU peripherals.

    log("SYSTEM");
    {
        // Seems required to power the device off.
        ble_enable_softdevice();

        // Init the built-in peripherals
        nrfx_systick_init();
    }

    // Start by the Bluetooth driver, which will let the user scan for
    // a network, and by the time the negociation would happen, and the
    // host application gets started, this firmware would be likely ready.

    log("BLE");
    {
        ble_init();
    }

    // power on everything and wait

    log("POWER");
    {
        max77654_rail_1v2(true);
        max77654_rail_1v8(true);
        max77654_rail_2v7(true);
        max77654_rail_vled(true);

        // Wait the power rail to stabilize, and other chips to boot.
        nrfx_systick_delay_ms(300);
        max77654_rail_10v(true);
        nrfx_systick_delay_ms(10);
    }

    // Now we can setup the various peripherals over SPI, starting by the FPGA
    // upon which the other depend for their configuration.

    log("SPI");
    {
        spi_init(spi2, SPI2_SCK_PIN, SPI2_MOSI_PIN, SPI2_MISO_PIN);
    }

    log("FPGA");
    assert_blink_num = 3;
    {
        fpga_init();
    }

    log("ECX336CN");
    assert_blink_num = 4;
    {
        // 1ms after 1.8V on, device has finished initializing (datasheet section 9)
        ecx336cn_init();
    }

    log("OV5640");
    assert_blink_num = 5;
    {
        // ov5640_init();
    }

    log("FLASH");
    assert_blink_num = 6;
    {
        flash_init();
    }

    // Initiate user-input peripherals, which did not make
    // sense before everything else is setup.

    log("IQS620");
    assert_blink_num = 2;
    {
        nrfx_gpiote_init(NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY);
        iqs620_init();
    }

    log("DONE");
    assert_blink_num = 10;
    {
        // Enable user interaction touch buttons only now that it started.
        iqs620_enable();

        // Let the user know everything is ready and that the device
        max77654_led_green(true);

        // Start the generic timer, used for various periodic software tasks.
        // Other drivers added function callbacks to its task table above.
        timer_start();
    }

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
        log("switching the interpreter mode");
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
