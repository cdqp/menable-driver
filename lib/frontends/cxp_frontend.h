/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */


#ifndef LIB_CXP_FRONTEND_H_
#define LIB_CXP_FRONTEND_H_

#include "../helpers/type_hierarchy.h"
#include "../os/types.h"
#include "../ioctl_interface/camera.h"
#include "../fpga/register_interface.h"
#include "camera_frontend.h"

//#define DBG_CXP_FRONTEND 1

#ifdef __cplusplus
extern "C" {
#endif

/* forward declaration for typedefs */
struct register_interface;

#define CXP_MAX_NUM_PORTS 5

#define CXP_FLAGS_SUPPORTS_IDLE_VIOLATION_FIX 0x1

#define ME6_REG_BOARD_STATUS            0x0000 /**< Board status register */
#define BOARDSTATUSEX                   0x0001     /**< Board ID */

struct cxp_port
{
    uint32_t downlink_bitrate_register;
    uint32_t led_ctrl_register;
    uint32_t config_image_stream_id_register;

    enum power_state power_state_cache;
    enum data_path_state data_path_state_physical_cache;
    enum data_path_state data_path_state_logical_cache;
    enum data_path_speed data_path_dw_speed_cache;
    enum data_path_up_speed data_path_up_speed_cache;
    enum cxp_standard_version standard_version_cache;
    enum cxp_led_state led_state_cache;
    enum acquisition_state acquisition_state_cache;

    uint8_t camera_downscale_state_cache;
    short stream_id_cache;
};

typedef struct cxp_frontend
{
    DERIVE_FROM(camera_frontend);

    struct register_interface * ri;

    uint32_t flags;

    struct cxp_port * ports;
    uint64_t * port_maps;
    uint32_t port_map_index;
    uint32_t num_ports;

    uint32_t power_ctrl_register;
    uint32_t reset_ctrl_register;
    uint32_t standard_ctrl_register;
    uint32_t acquisition_status_register;
    uint32_t discovery_config_register;
    uint32_t camera_operator_downscale_register;

    uint32_t data_path_status_register;
    unsigned int data_path_speed_change_timeout_msecs;

    uint32_t load_applet_ctrl_register;
    uint32_t load_applet_status_register;    
    /* TODO: [RKN] Why are all these functions exposed via function pointers? Shouldn't they be called via camera_frontend->execute_command? */

    /**
     * Set the port map.
     * This will put ports to be remapped into reset, initiate the switch and restore the previous reset values
     */
    int (*set_port_map)(struct cxp_frontend * self, uint64_t new_port_map);

    /**
     * Set power state.
     */
    int (*set_port_power_state)(struct cxp_frontend * self, uint32_t physical_port_number, enum power_state new_power_state);

    /**
     * Set data path state.
     */
    int (*set_port_data_path_state)(struct cxp_frontend * self, uint32_t physical_port_number, enum data_path_state new_data_path_state);

    /**
     * Set data path speed.
     */
    int (*set_port_data_path_speed)(struct cxp_frontend * self, uint32_t physical_port_number, enum data_path_speed new_data_path_speed);

    /**
     * Set cxp standard version.
     */
    int (*set_port_standard_version)(struct cxp_frontend * self, uint32_t physical_port_number, enum cxp_standard_version new_standard_version);

    /**
     * Set cxp led state.
     */
    int (*set_port_led_state)(struct cxp_frontend * self, uint32_t physical_port_number, enum cxp_led_state new_led_state);

    /**
     * Set acquisition state.
     */
    int (*set_port_acquisition_state)(struct cxp_frontend * self, uint32_t physical_port_number, enum acquisition_state new_acquisition_state);

    /**
     * Set  stream id and image number.
     */
    int (*set_port_image_stream_id)(struct cxp_frontend* self, uint32_t master_port, short stream_id);
} cxp_frontend;

#define CXP_FRONTEND_ERROR_INVALID_PORT (-1)
#define CXP_FRONTEND_ERROR_INVALID_PARAMETER (-2)
#define CXP_FRONTEND_ERROR_TIMEOUT (-3)
#define CXP_FRONTEND_ERROR_APPLETDOESNOTSUPPORTTGS (-4)

/**
 * Constructor.
 */
cxp_frontend * cxp_frontend_alloc_and_init(struct register_interface* ri, unsigned int num_ports, int support_idle_violation_fix);

#ifdef __cplusplus
}
#endif

#endif /* LIB_CXP_FRONTEND_H_ */
