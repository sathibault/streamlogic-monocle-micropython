/*
 * Copyright (c) 2022 Raj Nakarja - Silicon Witchery AB
 * Copyright (c) 2022 Brilliant Labs Limited
 * Licensed under the MIT License
 */

/**
 * Bluetooth Device Firmware Upgrade (DFU)
 * @file driver_ble.c
 * @author Raj Nakarja - Silicon Witchery AB
 * @author Josuah Demangeon - Panoramix Labs
 */

#include "nrf_soc.h"
#include "nrfx_log.h"

#include "driver_dfu.h"

#define LOG NRFX_LOG_ERROR

/**
 * Magic pattern written to GPREGRET register to signal between main
 * app and DFU. The 3 lower bits are assumed to be used for signalling
 * purposes.
 */
#define BOOTLOADER_DFU_GPREGRET         (0xB0)

/**
 * Bit mask to signal from main application to enter DFU mode using a
 * buttonless service.
 */
#define BOOTLOADER_DFU_START_BIT_MASK   (0x01)

/**
 * Magic number to signal that bootloader should enter DFU mode because of
 * signal from Buttonless DFU in main app.
 */
#define BOOTLOADER_DFU_START            (BOOTLOADER_DFU_GPREGRET | BOOTLOADER_DFU_START_BIT_MASK)

_Noreturn void dfu_reboot_bootloader(void)
{
    LOG("resetting system now");

    // Set the flag telling the bootloaer to go into DFU mode.
    sd_power_gpregret_set(0, BOOTLOADER_DFU_START);

    // Reset the CPU, giving controll to the bootloader.
    NVIC_SystemReset();
}
