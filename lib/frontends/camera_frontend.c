/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#include "camera_frontend.h"
#include "cxp_frontend.h"
#include "../helpers/error_handling.h"

#include "../boards/basler_ic.h"
#include "../boards/me6_impulse.h"
#include "../boards/me6_elegance.h"

#include <sisoboards.h>

int camera_frontend_init(struct camera_frontend * self,
                         unsigned int num_physical_ports,
                         void (*reset_physical_port)(struct camera_frontend * self, unsigned int port_num),
                         int (*reset)(struct camera_frontend *),
                         int (*prepare_applet_reload)(struct camera_frontend *),
                         void (*destroy)(struct camera_frontend *),
                         int (*execute_command)(struct camera_frontend *, enum camera_command, union camera_control_input_args *)) {

    if (reset == NULL || destroy == NULL || execute_command == NULL) {
        return STATUS_ERR_INVALID_ARGUMENT;
    }

    self->reset_physical_port = reset_physical_port;
    self->reset = reset;
    self->prepare_applet_reload = prepare_applet_reload;
    self->destroy = destroy;
    self->execute_command = execute_command;

    return STATUS_OK;
}

struct camera_frontend * camera_frontend_factory(unsigned int boardType, unsigned int pcieDsnLow, unsigned int pcieDsnHigh, struct register_interface* ri) {
	switch (boardType) {
	case PN_MICROENABLE6_CXP12_IC_1C:
	{
		struct cxp_frontend * cxp = NULL;
		if (BASLER_CXP12_IC_1C_SUPPORTS_CAMERA_FRONTEND(pcieDsnLow)) {
			cxp = cxp_frontend_alloc_and_init(ri, 1, BASLER_CXP12_IC_1C_SUPPORTS_IDLE_VIOLATION_FIX(pcieDsnLow));
			if (cxp != NULL) {
				return upcast(cxp);
			}
		}
		break;
	}

	case PN_MICROENABLE6_CXP12_IC_2C:
	case PN_MICROENABLE6_CXP12_LB_2C:
	{
		struct cxp_frontend * cxp = NULL;
		cxp = cxp_frontend_alloc_and_init(ri, 2, 1);
		if (cxp != NULL) {
			return upcast(cxp);
		}
		break;
	}

	case PN_MICROENABLE6_CXP12_IC_4C:
	{
		struct cxp_frontend * cxp = NULL;
		cxp = cxp_frontend_alloc_and_init(ri, 4, 1);
		if (cxp != NULL) {
			return upcast(cxp);
		}
		break;
	}

	case PN_MICROENABLE6_IMAWORX_CXP12_QUAD:
	case PN_MICROENABLE6_IMAFLEX_CXP12_QUAD:
	case PN_MICROENABLE6_IMPULSE_TEST_CXP12_QUAD:
	{
		struct cxp_frontend * cxp = NULL;
		cxp = cxp_frontend_alloc_and_init(ri, 4, ME6_IMPULSE_CXP_SUPPORTS_IDLE_VIOLATION_FIX(pcieDsnLow));
		if (cxp != NULL) {
			return upcast(cxp);
		}
		break;
	}

	case PN_MICROENABLE6_ELEGANCE_ECO:
	{
		struct cxp_frontend * cxp = NULL;
		if (ME6_ELEGANCE_ECO_SUPPORTS_CAMERA_FRONTEND(pcieDsnLow)) {
			cxp = cxp_frontend_alloc_and_init(ri, 4, 0);
			if (cxp != NULL) {
				return upcast(cxp);
			}
		}
		break;
	}
	}

	return NULL;
}
