/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef _MEN_LIB_FRONTENDS_CAMERA_FRONTEND_H_
#define _MEN_LIB_FRONTENDS_CAMERA_FRONTEND_H_

#include <lib/ioctl_interface/camera.h>
#include <lib/fpga/register_interface.h>
#include <lib/os/types.h>
#include <lib/helpers/error_handling.h>

#define CAMERA_FRONTEND_VERSION 1

/**
 * Abstract base class for all camera frontends.
 */
struct camera_frontend {

    unsigned int num_physical_ports;

    void (*reset_physical_port)(struct camera_frontend * self, unsigned int port_num);

    /**
     * Initialise frontend and all ports to default state.
     */
    int (*reset)(struct camera_frontend * self);

    /**
     * Prepare camera frontend for applet reloading.
     */
    int (*prepare_applet_reload)(struct camera_frontend * self);

    /**
     * Destructor.
     */
    void (*destroy)(struct camera_frontend * self);

    /**
     * Dispatcher for command execution
     */
    int (*execute_command)(struct camera_frontend * self, enum camera_command cmd, union camera_control_input_args * args);
};

/**
 * Factory for camera frontends.
 */
struct camera_frontend * camera_frontend_factory(unsigned int boardType, unsigned int pcieDsnLow, unsigned int pcieDsnHigh, struct register_interface * ri);

/**
 * Constructor for the base class.

 * @attention: Do not use outside a subclass.
 */
int camera_frontend_init(struct camera_frontend * self,
                         unsigned int num_physical_ports,
                         void (*reset_physical_port)(struct camera_frontend * self, unsigned int port_num),
                         int (*reset)(struct camera_frontend *),
                         int (*prepare_applet_reload)(struct camera_frontend *),
                         void (*destroy)(struct camera_frontend *),
                         int (*execute_command)(struct camera_frontend *, enum camera_command, union camera_control_input_args *));

#endif
