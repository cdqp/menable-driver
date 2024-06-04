/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */


#ifndef I2C_MASTER_CORE_H_
#define I2C_MASTER_CORE_H_

#include "../controllers/controller_base.h"
#include "../helpers/type_hierarchy.h"
#include "../helpers/timeout.h"
#include "../fpga/register_interface.h"

#ifdef __cplusplus 
extern "C" {
#endif

/* TODO: Implment transactions to aggregate bursts and synchronize device access transactionwise! */

/**
 * Represents the configuration of a single i2c bus that is managed by
 * an i2c_master_core.
 */
struct i2c_master_core_bus_cfg {

    /**
     * The index of the bank to access this bus.
     */
    /* TODO This may be obsolete since the bank index is also
     *      the array index of i2c_master_core.bus_configurations.
     */
    uint8_t bank_number;

    /**
     * The bit that has to be set in the core address register
     * to activate the i2c access. If this is 0, then no
     * activation is required.
     */
    uint8_t bank_activation_bit;

    /**
     * Some devices on the i2c bus may require explicit write enabling.
     * If this is the case for a device on this bus, the bitmask for
     * write enabling must be specified in this variable.
     *
     * Use the flags I2C_PRE_BURST_FLAG_WRITE_EANBLE in a write burst
     * when writing to a protected device.
     */
    uint8_t write_enable_bit;

    /**
     * The frequency of this bus in Hz.
     */
    uint32_t bus_frequency;
};

/**
 * Represents an i2c master core controller on the framegrabber.
 */
struct i2c_master_core {
    DERIVE_FROM(controller_base);

    /**
     * The address of the core address register.
     */
    uint32_t i2c_ctrl_address_register;

    /**
     * The address of the core write data regiter.
     */
    uint32_t i2c_ctrl_write_register;

    /**
     * The address of the core read data regiter.
     */
    uint32_t i2c_ctrl_read_register;

    /**
     * The frequency of the clock that is used for the i2c master core.
     */
    uint32_t firmware_clock_frequency;

    /**
     * The number of dummy write operations to ensure,
     * that a write operation is finished.
     */
    uint8_t num_safety_writes;

    /**
     * The configuration for all busses that are managed by the core instance.
     * The index for each bus corresponds to its bank index.
     */
    struct i2c_master_core_bus_cfg bus_configurations[8];

    /**
     * The configuration for the active bus.
     */
    struct i2c_master_core_bus_cfg * active_bus;

    /**
     * Activates a specific bank (resp. a specific bus) on the core.
     * If a transfer is in progress, the function waits for it to complete.
     *
     * @param bank_number The bank number to activate.
     */
    void (*activate_bank)(struct i2c_master_core * self, uint8_t bank_number);

};

/**
 * Initializes an i2c_master_core struct.
 * Call this function once on every i2c_master_core instance before using it.
 *
 * @param core
 *        the i2c_master_core instance to initialize.
 * @param ri
 *        The register interface for accessing the FPGA's registers.
 * @param i2c_ctrl_address_register
 *        The register for addressing a core register
 * @param i2c_ctrl_write_register
 *        The register for writing to / reading from the core register
 *        specified in i2c_ctrl_address_register
 * @param firmware_clock_frequency
 *        The frequency of the firmaware clock that is used for the i2c core.
 * @param num_dummy_writes
 *        The number of dummy write operations to ensure,
 *        that a write operation is finished.
 *
 * @return 0 on success, an error code on failure.
 */
int i2c_master_core_init(struct i2c_master_core * core,
                         struct register_interface * ri,
                         user_mode_lock * lock,
                         uint32_t i2c_ctrl_address_register,
                         uint32_t i2c_ctrl_write_register,
                         uint32_t i2c_ctrl_read_register,
                         uint32_t firmware_clock_frequency,
                         uint32_t num_safety_writes);

/**
 * Configures a single i2c bus for a i2c master core instance.
 *
 * @param core the master core instance that controls the bus
 * @param bank_number the bank number of the bus (0-7)
 * @param bank_activation_bitmask bitmask with only the bit for bank activation set to 1
 * @param write_enable_bitmask bitmask with the bits for enabling writing to a protected device
 * @param bus_frequency the frequncy for the bus
 *
 * @return 0 on success, an error code on failure
 */
int i2c_master_core_configure_bus(struct i2c_master_core * core,
                                  uint8_t bank_number,
                                  uint8_t bank_activation_bitmask,
                                  uint8_t write_enable_bitmask,
                                  uint32_t bus_frequency);


#ifdef __cplusplus 
} // extern "C"
#endif

#endif /* I2C_MASTER_CORE_H_ */
