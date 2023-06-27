/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "boards.h"
#include "../helpers/helper.h"

#include "basler_ic.h"
#include "me6_impulse.h"
#include "me6_elegance.h"
#include "me6_abacus.h"

unsigned int men_get_uiq_declaration(uint16_t device_id, uiq_declaration ** out_uiq_declaration) {
    switch (device_id) {

    case PN_MICROENABLE6_IMAWORX_CXP12_QUAD:
    case PN_MICROENABLE6_IMAFLEX_CXP12_QUAD:
    case PN_MICROENABLE6_IMPULSE_TEST_CXP12_QUAD:
        *out_uiq_declaration = me6_impulse_cxp_uiq_declaration;
        return ARRAY_SIZE(me6_impulse_cxp_uiq_declaration);


    case PN_MICROENABLE6_CXP12_IC_1C:
        *out_uiq_declaration = basler_cxp12_ic_1c_uiq_declaration;
        return ARRAY_SIZE(basler_cxp12_ic_1c_uiq_declaration);

    case PN_MICROENABLE6_CXP12_IC_2C:
    case PN_MICROENABLE6_CXP12_LB_2C:
        *out_uiq_declaration = basler_cxp12_ic_2c_uiq_declaration;
        return ARRAY_SIZE(basler_cxp12_ic_2c_uiq_declaration);

    case PN_MICROENABLE6_CXP12_IC_4C:
        *out_uiq_declaration = basler_cxp12_ic_4c_uiq_declaration;
        return ARRAY_SIZE(basler_cxp12_ic_4c_uiq_declaration);


    case PN_MICROENABLE6_ABACUS_4TG:
        *out_uiq_declaration = me6_abacus_uiq_declaration;
        return ARRAY_SIZE(me6_abacus_uiq_declaration);

    case PN_MICROENABLE6_ELEGANCE_ECO:
        *out_uiq_declaration = me6_elegance_uiq_declaration;
        return ARRAY_SIZE(me6_elegance_uiq_declaration);


    default:
        *out_uiq_declaration = NULL;
        return 0;
    }
}

void men_get_messaging_dma_declaration(uint16_t device_id, uint32_t pcieDsnLow, messaging_dma_declaration ** out_messaging_dma_declaration) {
    
    *out_messaging_dma_declaration = NULL;
        
    switch (device_id) {
    case PN_MICROENABLE6_IMAWORX_CXP12_QUAD:
    case PN_MICROENABLE6_IMAFLEX_CXP12_QUAD:
    case PN_MICROENABLE6_IMPULSE_TEST_CXP12_QUAD:
    case PN_MICROENABLE6_CXP12_IC_2C:
    case PN_MICROENABLE6_CXP12_IC_4C:
    case PN_MICROENABLE6_CXP12_LB_2C:
        *out_messaging_dma_declaration = &me6_impulse_messaging_dma_declaration;
        break;

    case PN_MICROENABLE6_CXP12_IC_1C:
        if (BASLER_CXP12_IC_1C_SUPPORTS_MESSAGING_DMA(pcieDsnLow)) {
            *out_messaging_dma_declaration = &basler_cxp12_ic_messaging_dma_declaration;
        }
        break;
    }
}
