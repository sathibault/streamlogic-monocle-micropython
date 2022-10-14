/*
 * Copyright (c) 2022 Brilliant Labs Limited
 * This software is owned by Brilliant Labs Limited.
 * It is proprietary, and can be disclosed only with written authorization.
 */

/**
 * @file
 * @author Nathan Ashelman
 * @author Shreyas Hemachandra
 * @bug i2c_scan_bus() doesn't work (but is not used, so not urgent)
 */

#include "monocle_config.h"
#include "monocle_i2c.h"
#include "nrf_gpio.h"
#include "nrfx_log.h"
#include "nrfx_twi.h"

#define LOG_DEBUG(...) NRFX_LOG_DEBUG(__VA_ARGS__)
#define CHECK(err) check(__func__, err)

/** TWI operation ended, may have been successful, may have been NACK. */
static volatile bool m_xfer_done = false;

/** NACK returned, operation was unsuccessful. */
static volatile bool m_xfer_nack = false;

/** TWI instance. */
static const nrfx_twi_t m_twi = NRFX_TWI_INSTANCE(TWI_INSTANCE_ID);

/**
 * Workaround the fact taht nordic returns an ENUM instead of a simple integer.
 */
static inline bool check(char const *func, nrfx_err_t err)
{
    if (err != NRFX_SUCCESS)
        NRFX_LOG_ERROR("%s: %s", func, NRFX_LOG_ERROR_STRING_GET(err));
    return err == NRFX_SUCCESS;
}

/**
 * Configure the I2C bus.
 */
// TODO: validate that 400kH speed works & increase to that
void i2c_init(void)
{
    const nrfx_twi_config_t twi_main_config = {
       .scl                = TWI_SCL_PIN,
       .sda                = TWI_SDA_PIN,
       .frequency          = NRF_TWI_FREQ_100K,
       .interrupt_priority = NRFX_TWI_DEFAULT_CONFIG_IRQ_PRIORITY,
    };

    CHECK(nrfx_twi_init(&m_twi, &twi_main_config, NULL, NULL));
    nrfx_twi_enable(&m_twi);
}

/**
 * Write a buffer over I2C.
 * @param addr The address at which write the data.
 * @param buf The buffer to write.
 * @param sz The length of that bufer.
 * @return True if no I2C errors were reported.
 */
bool i2c_write(uint8_t addr, uint8_t *buf, uint8_t sz)
{
    nrfx_twi_xfer_desc_t xfer = NRFX_TWI_XFER_DESC_TX(addr, buf, sz);
    return CHECK(nrfx_twi_xfer(&m_twi, &xfer, 0));
}

/**
 * Write a buffer over I2C without stop condition.
 * The I2C transaction will still be ongoing, permitting to more data to be written.
 * @param addr The address at which write the data.
 * @param buf The buffer containing the data to write.
 * @param sz The length of that bufer.
 * @return True if no I2C errors were reported.
 */
bool i2c_write_no_stop(uint8_t addr, uint8_t *buf, uint8_t sz)
{
    nrfx_twi_xfer_desc_t xfer = NRFX_TWI_XFER_DESC_TX(addr, buf, sz);
    return CHECK(nrfx_twi_xfer(&m_twi, &xfer, NRFX_TWI_FLAG_TX_NO_STOP));
}

/**
 * Read a buffer from I2C.
 * @param addr Address of the peripheral.
 * @param buf The buffer receiving the data.
 * @param sz The length of that bufer.
 * @return True if no I2C errors were reported.
 */
bool i2c_read(uint8_t addr, uint8_t *buf, uint8_t sz)
{
    nrfx_twi_xfer_desc_t xfer = NRFX_TWI_XFER_DESC_RX(addr, buf, sz);;
    return CHECK(nrfx_twi_xfer(&m_twi, &xfer, 0));
}

/**
 * For debugging, scan the bus and expect MBR3@55=0x37 & OV5640@60=0x3C.
 * @bug this generates a lot of false positives, and didn't detect @55.
 * @return True if all expected addresses were found.
 */
bool i2c_scan_bus(void)
{
    uint8_t addr;
    uint8_t sample_data;
    bool detected_device = false;

    for (addr = 1; addr <= 127; addr++) {
        if (i2c_read(addr, &sample_data, sizeof(sample_data)) == NRF_SUCCESS) {
            detected_device = true;
            LOG_DEBUG("I2C device detected at address 0x%x.", addr);
        }
    }
    return detected_device;
}