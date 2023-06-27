/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */
 
#ifndef _SISOBOARDS_H
#define _SISOBOARDS_H

/**
 * Board type identifiers.
 *
 * Every device is identified by one of these identifiers. They
 * can be queried e.g. by Fg_getBoardType() or with
 * Fg_getParameterWithType() using the #FG_BOARD_INFORMATION argument.
 *
 * Please note that subvariants of the boards that are only different by the
 * physical layout of the connectors (e.g. CL connectors vs. PoCL connectors)
 * use the same type value.
 */
enum siso_board_type {

    PN_MICROENABLE4AD1CL = 0xa40,                      /**< microEnable IV AD1-CL */
    PN_MICROENABLE4BASE =
        PN_MICROENABLE4AD1CL,                          /**< \deprecated old name for PN_MICROENABLE4AD1CL, maintained only for source compatibility */
    PN_MICROENABLE4BASEx4 = 0xa43,                     /**< \deprecated name for a prototype never used*/
    PN_MICROENABLE4AD4CL = 0xa42,                      /**< microEnable IV AD4-CL */
    PN_MICROENABLE4VD1CL = 0xa41,                      /**< microEnable IV VD1-CL */
    PN_MICROENABLE4FULLx1 =
        PN_MICROENABLE4VD1CL,                          /**< \deprecated old name for PN_MICROENABLE4VD1CL, maintained only for source compatibility */
    PN_MICROENABLE4VD4CL = 0xa44,                      /**< microEnable IV VD4-CL */
    PN_MICROENABLE4FULLx4 =
        PN_MICROENABLE4VD4CL,                          /**< \deprecated old name for PN_MICROENABLE4VD4CL, maintained only for source compatibility */
    PN_MICROENABLE4AS1CL = 0xa45,                      /**< microEnable IV AS1-CL */
    PN_MICROENABLE4VQ4GE = 0xe44,                      /**< microEnable IV VQ4-GE */
    PN_MICROENABLE4GIGEx4 =
        PN_MICROENABLE4VQ4GE,                          /**< \deprecated old name for PN_MICROENABLE4VQ4GE, maintained only for source compatibility */
    PN_MICROENABLE4AQ4GE = 0xe42,                      /**< microEnable IV AQ4-GE */
    PN_MICROENABLE4_H264CLx1 = 0xb41,                  /**< kappa h264 Fujitsu MB86H51 */
    PN_MICROENABLE4_H264pCLx1 = 0xb42,                 /**< kappa h264 Fujitsu MB86H46A */

    PN_PX100 = 0xc41,                                  /**< PixelPlant PX100 */
    PN_PX200 = 0xc42,                                  /**< PixelPlant PX200 */
    PN_PX210 = 0xc43,                                  /**< PixelPlant PX210-CL */
    PN_PX300 = 0xc44,                                  /**< PixelPlant PX300-CxP */

    PN_MICROENABLE5A1CXP4 = 0xa51,                     /**< microEnable 5 A01-CXP */
    PN_MICROENABLE5A1CLHSF2 = 0xa52,                   /**< microEnable 5 A1-CLHS-F2 */
    PN_MICROENABLE5AQ8CXP6B = 0xa53,                   /**< microEnable 5 AQ8-CXP6B */
    PN_MICROENABLE5AQ8CXP4 =
        PN_MICROENABLE5AQ8CXP6B,                       /**< \deprecated old name for PN_MICROENABLE5AQ8CXP6B, maintained only for source compatibility */
    PN_MICROENABLE5VQ8CXP6B = 0xa54,                   /**< microEnable 5 VQ8-CXP6B */
    PN_MICROENABLE5VQ8CXP4 =
        PN_MICROENABLE5VQ8CXP6B,                       /**<\deprecated old name for PN_MICROENABLE5VQ8CXP6B, maintained only for source compatibility */
    PN_MICROENABLE5AD8CLHSF2 = 0xa55,                  /**< microEnable 5 AD8-CLHS-F2 */
    PN_MICROENABLE5VQ8CXP6D = 0xa56,                   /**< microEnable 5 VQ8-CXP6D */
    PN_MICROENABLE5AQ8CXP6D = 0xa57,                   /**< microEnable 5 AQ8-CXP6D */
    PN_MICROENABLE5VD8CL = 0xa58,                      /**< microEnable 5 VD8-CL */
    PN_MICROENABLE5VF8CL =
        PN_MICROENABLE5VD8CL,                          /**< \deprecated old name for PN_MICROENABLE5VD8CL, maintained only for source compatibility */
    PN_MICROENABLE5A2CLHSF2 = 0xa59,                   /**< microEnable 5 A2-CLHS-F2 */
    PN_MICROENABLE5AD8CL = 0xa5a,                      /**< microEnable 5 AD8-CL */

    PN_MICROENABLE5_LIGHTBRIDGE_VCL_PROTOTYPE = 0x750, /**< LightBridge VCL Prototype */
    PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL = 0x751,  /**< LightBridge/Marathon VCL */
    PN_MICROENABLE5_LIGHTBRIDGE_VCL = 0x7510,          /**< LightBridge VCL */
    PN_MICROENABLE5_MARATHON_VCL = 0x7511,             /**< mE5 marathon VCL */
    PN_MICROENABLE5_LIGHTBRIDGE_VCL_SPI = 0x7512,      /**< LightBridge VCL SPI */
    PN_MICROENABLE5_MARATHON_VCL_SPI = 0x7513,         /**< mE5 marathon VCL SPI */
    PN_MICROENABLE5_MARATHON_AF2_DP = 0x752,           /**< mE5 marathon AF2 (CLHS dual port) */
    PN_MICROENABLE5_MARATHON_ACX_QP = 0x753,           /**< mE5 marathon ACX QP (CXP quad port) */
    PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL = 0x754,  /**< LightBridge/Marathon ACL */
    PN_MICROENABLE5_LIGHTBRIDGE_ACL = 0x7540,          /**< LightBridge ACL */
    PN_MICROENABLE5_MARATHON_ACL = 0x7541,             /**< mE5 marathon ACL */
    PN_MICROENABLE5_LIGHTBRIDGE_ACL_SPI = 0x7542,      /**< LightBridge ACL SPI */
    PN_MICROENABLE5_MARATHON_ACL_SPI = 0x7543,         /**< mE5 marathon ACL SPI */
    PN_MICROENABLE5_MARATHON_ACX_SP = 0x755,           /**< mE5 marathon ACX SP (CXP single port) */
    PN_MICROENABLE5_MARATHON_ACX_DP = 0x756,           /**< mE5 marathon ACX DP (CXP dual port) */
    PN_MICROENABLE5_MARATHON_VCX_QP = 0x757,           /**< mE5 marathon VCX QP (CXP quad port) */
    PN_MICROENABLE5_MARATHON_VF2_DP = 0x758,           /**< mE5 marathon VF2 (CLHS dual port) */
    PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx = 0x759, /**< LightBridge/Marathon VCLx */
    //PN_MICROENABLE5_LIGHTBRIDGE_VCLx = 0x7590,         /**< LightBridge VCLx */
    PN_MICROENABLE5_MARATHON_VCLx = 0x7591,            /**< mE5 marathon VCLx (VCL with XC7K410T FPGA) */
    PN_MICROENABLE5_MARATHON_DeepVCL = 0x7595,         /**< mE5 marathon DeepVCL (VCLx with Deep Learning VA license) */

    PN_TDI = 0xb50,                                    /**< Thunderbolt Device Interface/II */
    PN_TDI_I = 0xb500,                                 /**< Thunderbolt Device Interface */
    PN_TDI_II = 0xb501,                                /**< Thunderbolt Device Interface II*/
    PN_TDI_III = 0xb502,                                /**< Thunderbolt Device Interface III*/
    PN_TGATE_USB = 0xb57,                              /**< T-Gate USB/II */
    PN_TGATE_I_USB = 0xb570,                           /**< T-Gate USB/II */
    PN_TGATE_II_USB = 0xb571,                          /**< T-Gate USB/II */
    PN_TGATE = 0xb5e,                                  /**< T-Gate/II */
    PN_TGATE_I = 0xb5e0,                               /**< T-Gate/II */
    PN_TGATE_II = 0xb5e1,                              /**< T-Gate/II */
    PN_TGATE_35 = 0xb58,                               /**< T-Gate/II 35 USB */
    PN_TGATE_I_35 = 0xb580,                            /**< T-Gate/II 35 USB */
    PN_TGATE_II_35 = 0xb581,                           /**< T-Gate/II 35 USB */
    PN_TGATE_35_USB = 0xb59,                           /**< T-Gate/II 35 USB */
    PN_TGATE_I_35_USB = 0xb590,                        /**< T-Gate/II 35 USB */
    PN_TGATE_II_35_USB = 0xb591,                       /**< T-Gate/II 35 USB */
    PN_TTDI = 0xb5f,                                   /**< Test Thunderbolt Device Interface */

    PN_MICROENABLE5_ABACUS_4G_PROTOTYPE = 0xb51,       /**< microEnable 5 Abacus 4G Prototype */
    PN_MICROENABLE5_ABACUS_4G =
        PN_MICROENABLE5_ABACUS_4G_PROTOTYPE,           /**< \deprecated old name for PN_MICROENABLE5_ABACUS_4G_PROTOTYPE, maintained only for source compatibility */
    PN_MICROENABLE5_ABACUS_4G_BASE = 0xb52,            /**< microEnable 5 Abacus 4G Base */
    PN_MICROENABLE5_ABACUS_4G_BASE_II = 0xb53,         /**< microEnable 5 Abacus 4G Base II (7K70T FPGA) */

    PN_MICROENABLE6_IMPULSE_TEST_CXP12_QUAD = 0xA60,   /**< Test-CXP12-A-4C */
    PN_MICROENABLE6_IMPULSE_KCU116 = 0xA61,            /**< EVAL board KCU116 */
    PN_MICROENABLE6_IMAWORX_CXP12_QUAD = 0xA64,        /**< imaWorx CXP-12 Quad (was impulse CXP-12 A-4C) */
    PN_MICROENABLE6_IMAFLEX_CXP12_QUAD = 0xA65,        /**< imaFlex CXP-12 Quad (was impulse CXP-12 V-4C) */

    PN_MICROENABLE6_ABACUS_4TG = 0xB61,                /**< microEnable 6 Abacus 4 TGE (10GigE/Copper quad port) */
    PN_MICROENABLE6_CXP12_IC_1C = 0xB63,               /**< CXP12 Interface Card 1C */
    PN_MICROENABLE6_CXP12_IC_2C = 0x0B65,               /**< CXP12 Interface Card 2C */
    PN_MICROENABLE6_CXP12_IC_4C = 0x0B64,               /**< CXP12 Interface Card 4C */
    PN_MICROENABLE6_CXP12_LB_2C = 0x0B66,               /**< CXP12 Light Bridge 2C */
    PN_MICROENABLE6_ELEGANCE_ECO = 0xB68,              /**< microEnable 6 Elegance.eco */

    PN_MICROENABLE6_IMPULSE_CX4S = 0xA62,              /**< Impulse CX4S */
    PN_MICROENABLE6_IMPULSE_CX1S = 0xA63,              /**< Impulse CX1S */
    PN_MICROENABLE6_IMPULSE_CX2S = 0xA66,              /**< Impulse CX2S */

    PN_MICROENABLE6_IMPULSE_CX4A = 0xA67,              /**< Impulse CX4A */
    PN_MICROENABLE6_IMPULSE_CX4X = 0xA68,              /**< Impulse CX4X */

    PN_MICROENABLE6_IMPULSE_CX5A = 0xA69,              /**< Impulse CX5A */

    PN_MICROENABLE6_IMPULSE_FB4X = 0xA6A,              /**< Impulse FB4X */

    PN_MICROENABLE6_LIGHTBRIDGE_FB2A = 0xA6B,          /**< LightBridge FB2A */

    PN_MICROENABLE6_IMPULSE_PROTOTYPE = 0xA6C,         /**< Impulse Prototype */

    PN_MIPI_1200_PB_12C = 0xB62,                       /**< MIPI-1200 Processing Board 12C */

    PN_UNKNOWN = 0xffff,
    PN_GENERIC_EVA    = 0x10000000, /**< Mask bit for generic eVA platform*/
    PN_GENERIC_CXP    = 0x01000000, /**< Mask bit for generic CXP platform*/
    PN_GENERIC_CXP12  = 0x02000000, /**< Mask bit for generic CXP12 platform*/
    PN_GENERIC_GIGE   = 0x00100000, /**< Mask bit for generic GigabitEthernet platform*/
    PN_GENERIC_10GIGE = 0x00200000, /**< Mask bit for generic 10-GigE platform*/
    PN_GENERIC_CL     = 0x00010000, /**< Mask bit for generic CameraLink platform*/
    PN_GENERIC_CLHS   = 0x00020000, /**< Mask bit for generic CLHS platform*/
    PN_NONE = 0
};

#if defined(__cplusplus) || defined(_MSC_VER) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
    /* Inline functions are supported by the language. Enable them. */

    /* For C99, the stdbool header is required. Exception: When compiling a linux kernel module, we have to use the kernel header to avoid conflicts. */
    #if defined(__linux) && defined(__KERNEL__)
        #include <linux/types.h>
    #elif defined(_MSC_VER) || defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
        #include <stdbool.h>
    #endif

    #ifndef SISOBOARDS_INLINE_FUNCTIONS
        #define SISOBOARDS_INLINE_FUNCTIONS
    #endif

#elif defined(SISOBOARDS_INLINE_FUNCTIONS)
    /* No inline support by the language. Enable only if requested explicitly. This may cause name clashes. */

    /* Provide the bool type, since we're most likely in ANSI C */
    typedef enum _bool { false, true } bool;

    /* remove the inline keyword from the function declarations.*/
    #define inline
#endif

#ifdef SISOBOARDS_INLINE_FUNCTIONS

static inline const char* GetBoardName(int boardType) {
    switch (boardType) {
    case PN_MICROENABLE4AD1CL: return "microEnable IV AD1-CL";
    case PN_MICROENABLE4AD4CL: return "microEnable IV AD4-CL";
    case PN_MICROENABLE4VD1CL: return "microEnable IV VD1-CL";
    case PN_MICROENABLE4VD4CL: return "microEnable IV VD4-CL";
    case PN_MICROENABLE4AS1CL: return "microEnable IV AS1-CL";
    case PN_MICROENABLE4VQ4GE: return "microEnable IV VQ4-GE";
    case PN_MICROENABLE4AQ4GE: return "microEnable IV AQ4-GE";
    case PN_MICROENABLE4_H264CLx1: return "kappa h264 Fujitsu MB86H51";
    case PN_MICROENABLE4_H264pCLx1: return "kappa h264 Fujitsu MB86H46A";
    case PN_PX100: return "PixelPlant PX100";
    case PN_PX200: return "PixelPlant PX200";
    case PN_PX210: return "PixelPlant PX210-CL";
    case PN_PX300: return "PixelPlant PX300-CxP";
    case PN_MICROENABLE5A1CXP4: return "microEnable 5 A01-CXP";
    case PN_MICROENABLE5A1CLHSF2: return "microEnable 5 A1-CLHS-F2";
    case PN_MICROENABLE5AQ8CXP6B: return "microEnable 5 AQ8-CXP6B";
    case PN_MICROENABLE5VQ8CXP6B: return "microEnable 5 VQ8-CXP6B";
    case PN_MICROENABLE5AD8CLHSF2: return "microEnable 5 AD8-CLHS-F2";
    case PN_MICROENABLE5VQ8CXP6D: return "microEnable 5 VQ8-CXP6D";
    case PN_MICROENABLE5AQ8CXP6D: return "microEnable 5 AQ8-CXP6D";
    case PN_MICROENABLE5VD8CL: return "microEnable 5 VD8-CL";
    case PN_MICROENABLE5A2CLHSF2: return "microEnable 5 A2-CLHS-F2";
    case PN_MICROENABLE5AD8CL: return "microEnable 5 AD8-CL";
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL_PROTOTYPE: return "LightBridge VCL Prototype";
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL: return "LightBridge/Marathon VCL";
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL: return "LightBridge VCL";
    case PN_MICROENABLE5_MARATHON_VCL: return "mE5 marathon VCL";
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL_SPI: return "LightBridge VCL SPI";
    case PN_MICROENABLE5_MARATHON_VCL_SPI: return "mE5 marathon VCL SPI";
    case PN_MICROENABLE5_MARATHON_AF2_DP: return "mE5 marathon AF2";
    case PN_MICROENABLE5_MARATHON_ACX_QP: return "mE5 marathon ACX QP";
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL: return "LightBridge/Marathon ACL";
    case PN_MICROENABLE5_LIGHTBRIDGE_ACL: return "LightBridge ACL";
    case PN_MICROENABLE5_MARATHON_ACL: return "mE5 marathon ACL";
    case PN_MICROENABLE5_LIGHTBRIDGE_ACL_SPI: return "LightBridge ACL SPI";
    case PN_MICROENABLE5_MARATHON_ACL_SPI: return "mE5 marathon ACL SPI";
    case PN_MICROENABLE5_MARATHON_ACX_SP: return "mE5 marathon ACX SP";
    case PN_MICROENABLE5_MARATHON_ACX_DP: return "mE5 marathon ACX DP";
    case PN_MICROENABLE5_MARATHON_VCX_QP: return "mE5 marathon VCX QP";
    case PN_MICROENABLE5_MARATHON_VF2_DP: return "mE5 marathon VF2";
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx: return "LightBridge/Marathon VCLx";
    case PN_MICROENABLE5_MARATHON_VCLx: return "mE5 marathon VCLx";
    case PN_MICROENABLE5_MARATHON_DeepVCL: return "mE5 marathon DeepVCL";
    case PN_TDI: return "Thunderbolt Device Interface/II";
    case PN_TDI_I: return "Thunderbolt Device Interface";
    case PN_TDI_II: return "Thunderbolt Device Interface II";
    case PN_TGATE_USB: return "T-Gate USB/II";
    case PN_TGATE_I_USB: return "T-Gate USB/II";
    case PN_TGATE_II_USB: return "T-Gate USB/II";
    case PN_TGATE: return "T-Gate/II";
    case PN_TGATE_I: return "T-Gate/II";
    case PN_TGATE_II: return "T-Gate/II";
    case PN_TGATE_35: return "T-Gate/II 35 USB";
    case PN_TGATE_I_35: return "T-Gate/II 35 USB";
    case PN_TGATE_II_35: return "T-Gate/II 35 USB";
    case PN_TGATE_35_USB: return "T-Gate/II 35 USB";
    case PN_TGATE_I_35_USB: return "T-Gate/II 35 USB";
    case PN_TGATE_II_35_USB: return "T-Gate/II 35 USB";
    case PN_TTDI: return "Test Thunderbolt Device Interface";
    case PN_MICROENABLE5_ABACUS_4G_PROTOTYPE: return "microEnable 5 Abacus 4G Prototype";
    case PN_MICROENABLE5_ABACUS_4G_BASE: return "microEnable 5 Abacus 4G Base";
    case PN_MICROENABLE5_ABACUS_4G_BASE_II: return "microEnable 5 Abacus 4G Base II";
    case PN_MICROENABLE6_IMPULSE_TEST_CXP12_QUAD: return "Test-CXP12-A-4C";
	case PN_MICROENABLE6_IMPULSE_KCU116: return "EVAL board KCU116";
    case PN_MICROENABLE6_IMAWORX_CXP12_QUAD: return "imaWorx CXP-12 Quad";
    case PN_MICROENABLE6_IMAFLEX_CXP12_QUAD: return "imaFlex CXP-12 Quad";
    case PN_MICROENABLE6_ABACUS_4TG: return "microEnable 6 Abacus 4 TGE";
    case PN_MICROENABLE6_CXP12_IC_1C: return "CXP12 Interface Card 1C";
    case PN_MICROENABLE6_CXP12_IC_2C: return "CXP12 Interface Card 2C";
    case PN_MICROENABLE6_CXP12_IC_4C: return "CXP12 Interface Card 4C";
    case PN_MICROENABLE6_CXP12_LB_2C: return "CXP12 Light Bridge 2C";
    case PN_MICROENABLE6_ELEGANCE_ECO: return "microEnable 6 Elegance.eco";
    case PN_MICROENABLE6_IMPULSE_CX4S: return "Impulse CX4S";
    case PN_MICROENABLE6_IMPULSE_CX1S: return "Impulse CX1S";
    case PN_MICROENABLE6_IMPULSE_CX2S: return "Impulse CX2S";
    case PN_MICROENABLE6_IMPULSE_CX4A: return "Impulse CX4A";
    case PN_MICROENABLE6_IMPULSE_CX4X: return "Impulse CX4X";
    case PN_MICROENABLE6_IMPULSE_CX5A: return "Impulse CX5A";
    case PN_MICROENABLE6_IMPULSE_FB4X: return "Impulse FB4X";
    case PN_MICROENABLE6_LIGHTBRIDGE_FB2A: return "LightBridge FB2A";
    case PN_MICROENABLE6_IMPULSE_PROTOTYPE: return "Impulse Prototype";
    case PN_MIPI_1200_PB_12C: return "MIPI-1200 Processing Board 12C";
	default: return "UNKNOWN";
    }
}

static inline bool SisoBoardIsMe4(const int boardType)
{
    switch (boardType) {
        case PN_MICROENABLE4AD1CL:
        case PN_MICROENABLE4BASEx4:
        case PN_MICROENABLE4AD4CL:
        case PN_MICROENABLE4VD1CL:
        case PN_MICROENABLE4VD4CL:
        case PN_MICROENABLE4AS1CL:
        case PN_MICROENABLE4_H264CLx1:
        case PN_MICROENABLE4_H264pCLx1:
        case PN_MICROENABLE4VQ4GE:
        case PN_MICROENABLE4AQ4GE:
            return true;
        default:
            return false;
    }
}

static inline bool SisoBoardIsPxp(const int boardType)
{
    switch (boardType) {
        case PN_PX100:
        case PN_PX200:
        case PN_PX210:
        case PN_PX300:
            return true;
        default:
            return false;
    }
}

static inline bool SisoBoardIsMe5(const int boardType)
{
    switch (boardType) {
        case PN_MICROENABLE5A1CLHSF2:
        case PN_MICROENABLE5AD8CLHSF2:
        case PN_MICROENABLE5A2CLHSF2:
        case PN_MICROENABLE5A1CXP4:
        case PN_MICROENABLE5AQ8CXP6B:
        case PN_MICROENABLE5VQ8CXP6B:
        case PN_MICROENABLE5AQ8CXP6D:
        case PN_MICROENABLE5VQ8CXP6D:
        case PN_MICROENABLE5VD8CL:
        case PN_MICROENABLE5AD8CL:
        case PN_TDI:
        case PN_TDI_I:
        case PN_TDI_II:
        case PN_TDI_III:
        case PN_TTDI:
        case PN_MICROENABLE5_ABACUS_4G_PROTOTYPE:
        case PN_MICROENABLE5_ABACUS_4G_BASE:
        case PN_MICROENABLE5_ABACUS_4G_BASE_II:
        case PN_MICROENABLE5_LIGHTBRIDGE_VCL_PROTOTYPE:
        case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL:
        case PN_MICROENABLE5_LIGHTBRIDGE_VCL:
        case PN_MICROENABLE5_MARATHON_VCL:
        case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx:
        case PN_MICROENABLE5_MARATHON_VCLx:
        case PN_MICROENABLE5_MARATHON_DeepVCL:
        case PN_MICROENABLE5_MARATHON_AF2_DP:
        case PN_MICROENABLE5_MARATHON_ACX_QP:
        case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL:
        case PN_MICROENABLE5_LIGHTBRIDGE_ACL:
        case PN_MICROENABLE5_MARATHON_ACL:
        case PN_MICROENABLE5_MARATHON_ACX_SP:
        case PN_MICROENABLE5_MARATHON_ACX_DP:
        case PN_MICROENABLE5_MARATHON_VCX_QP:
        case PN_MICROENABLE5_MARATHON_VF2_DP:
            return true;
        default:
            return false;
    }
}

static inline bool SisoBoardIsMe6(const int boardType)
{
    switch (boardType) {
    case PN_MICROENABLE6_IMAWORX_CXP12_QUAD:
    case PN_MICROENABLE6_IMAFLEX_CXP12_QUAD:
    case PN_MICROENABLE6_IMPULSE_KCU116:
    case PN_MICROENABLE6_ABACUS_4TG:
    case PN_MICROENABLE6_CXP12_IC_1C:
    case PN_MICROENABLE6_CXP12_IC_2C:
    case PN_MICROENABLE6_CXP12_IC_4C:
    case PN_MICROENABLE6_ELEGANCE_ECO:

	case PN_MICROENABLE6_IMPULSE_CX4S:
    case PN_MICROENABLE6_IMPULSE_CX1S:
    case PN_MICROENABLE6_IMPULSE_CX2S:
    case PN_MICROENABLE6_IMPULSE_CX4A:
    case PN_MICROENABLE6_IMPULSE_CX4X:
    case PN_MICROENABLE6_IMPULSE_CX5A:
    case PN_MICROENABLE6_IMPULSE_FB4X:
    case PN_MICROENABLE6_IMPULSE_PROTOTYPE:
        return true;
    default:
        return false;
    }
}

static inline bool SisoBoardIsAbacus(const int boardType)
{
    switch (boardType) {
    case PN_MICROENABLE5_ABACUS_4G_PROTOTYPE:
    case PN_MICROENABLE5_ABACUS_4G_BASE:
    case PN_MICROENABLE5_ABACUS_4G_BASE_II:
    case PN_MICROENABLE6_ABACUS_4TG:
        return true;
    default:
        return false;
    }
}

static inline bool SisoBoardIsIronMan(const int boardType)
{
    switch (boardType) {
    case PN_MICROENABLE5A1CLHSF2:
    case PN_MICROENABLE5AD8CLHSF2:
    case PN_MICROENABLE5A2CLHSF2:
    case PN_MICROENABLE5A1CXP4:
    case PN_MICROENABLE5AQ8CXP6B:
    case PN_MICROENABLE5VQ8CXP6B:
    case PN_MICROENABLE5AQ8CXP6D:
    case PN_MICROENABLE5VQ8CXP6D:
    case PN_MICROENABLE5VD8CL:
    case PN_MICROENABLE5AD8CL:
        return true;
    default:
        return false;
    }
}

static inline bool SisoBoardIsMarathon(const int boardType)
{
    switch (boardType) {
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL_PROTOTYPE:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL:
    case PN_MICROENABLE5_MARATHON_VCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx:
    case PN_MICROENABLE5_MARATHON_VCLx:
    case PN_MICROENABLE5_MARATHON_DeepVCL:
    case PN_MICROENABLE5_MARATHON_AF2_DP:
    case PN_MICROENABLE5_MARATHON_ACX_QP:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL:
    case PN_MICROENABLE5_LIGHTBRIDGE_ACL:
    case PN_MICROENABLE5_MARATHON_ACL:
    case PN_MICROENABLE5_MARATHON_ACX_SP:
    case PN_MICROENABLE5_MARATHON_ACX_DP:
    case PN_MICROENABLE5_MARATHON_VCX_QP:
    case PN_MICROENABLE5_MARATHON_VF2_DP:
        return true;
    default:
        return false;
    }
}

static inline bool SisoBoardIsImpulse(const int boardType)
{
    switch (boardType) {
    case PN_MICROENABLE6_IMAWORX_CXP12_QUAD:
    case PN_MICROENABLE6_IMAFLEX_CXP12_QUAD:
    case PN_MICROENABLE6_IMPULSE_KCU116:
    case PN_MICROENABLE6_CXP12_IC_1C:
    case PN_MICROENABLE6_CXP12_IC_2C:
    case PN_MICROENABLE6_CXP12_IC_4C:

    case PN_MICROENABLE6_IMPULSE_CX4S:
    case PN_MICROENABLE6_IMPULSE_CX1S:
    case PN_MICROENABLE6_IMPULSE_CX2S:
    case PN_MICROENABLE6_IMPULSE_CX4A:
    case PN_MICROENABLE6_IMPULSE_CX4X:
    case PN_MICROENABLE6_IMPULSE_CX5A:
    case PN_MICROENABLE6_IMPULSE_FB4X:
    case PN_MICROENABLE6_IMPULSE_PROTOTYPE:
        return true;
    default:
        return false;
    }
}

static inline bool SisoBoardIsBaslerIC(const int boardType)
{
	switch (boardType) {
	case PN_MICROENABLE6_CXP12_IC_1C:
    case PN_MICROENABLE6_CXP12_IC_2C:
    case PN_MICROENABLE6_CXP12_IC_4C:
		return true;
	default:
		return false;
	}
}

static inline bool SisoBoardIsElegance(const int boardType)
{
	switch (boardType) {
	case PN_MICROENABLE6_ELEGANCE_ECO:
		return true;
	default:
		return false;
	}
}

static inline bool SisoBoardIsTdi(const int boardType)
{
    switch (boardType) {
    case PN_TDI:
    case PN_TDI_I:
    case PN_TDI_II:
    case PN_TDI_III:
    case PN_TTDI:
        return true;
    default:
        return false;
    }
}

static inline bool SisoBoardIsExternal(const int boardType)
{
    switch (boardType) {
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL_PROTOTYPE:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL:
    case PN_MICROENABLE5_LIGHTBRIDGE_ACL:
    case PN_TDI:
    case PN_TDI_I:
    case PN_TDI_II:
    case PN_TDI_III:
    case PN_TTDI:
    case PN_TGATE:
    case PN_TGATE_I:
    case PN_TGATE_II:
    case PN_TGATE_USB:
    case PN_TGATE_I_USB:
    case PN_TGATE_II_USB:
    case PN_TGATE_35:
    case PN_TGATE_I_35:
    case PN_TGATE_II_35:
    case PN_TGATE_35_USB:
    case PN_TGATE_I_35_USB:
    case PN_TGATE_II_35_USB:
        return true;
    default:
        return false;
    }
}

static inline bool SisoBoardIsCL(const int boardType)
{
    switch (boardType) {
        case PN_MICROENABLE4AD1CL:
        case PN_MICROENABLE4BASEx4:
        case PN_MICROENABLE4AD4CL:
        case PN_MICROENABLE4VD1CL:
        case PN_MICROENABLE4VD4CL:
        case PN_MICROENABLE4AS1CL:
        case PN_MICROENABLE5VD8CL:
        case PN_MICROENABLE5AD8CL:
        case PN_MICROENABLE5_LIGHTBRIDGE_VCL_PROTOTYPE:
        case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL:
        case PN_MICROENABLE5_LIGHTBRIDGE_VCL:
        case PN_MICROENABLE5_MARATHON_VCL:
        case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx:
        case PN_MICROENABLE5_MARATHON_VCLx:
        case PN_MICROENABLE5_MARATHON_DeepVCL:
        case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL:
        case PN_MICROENABLE5_LIGHTBRIDGE_ACL:
        case PN_MICROENABLE5_MARATHON_ACL:
            return true;
        default:
        {
          int mask = PN_GENERIC_EVA | PN_GENERIC_CL;
          return (boardType & mask) == mask;
        }
    }
}

static inline bool SisoBoardIsGigE(const int boardType)
{
    switch (boardType) {
        case PN_MICROENABLE4VQ4GE:
        case PN_MICROENABLE4AQ4GE:
        case PN_MICROENABLE5_ABACUS_4G_PROTOTYPE:
        case PN_MICROENABLE5_ABACUS_4G_BASE:
        case PN_MICROENABLE5_ABACUS_4G_BASE_II:
        case PN_MICROENABLE6_ABACUS_4TG:
            return true;
        default:
        {
          int mask = PN_GENERIC_EVA | PN_GENERIC_GIGE;
          return (boardType & mask) == mask;
        }
    }
}

static inline bool SisoBoardIs10GigE(const int boardType)
{
    switch (boardType) {
        case PN_MICROENABLE6_ABACUS_4TG:
            return true;
        default:
        {
          int mask = PN_GENERIC_EVA | PN_GENERIC_10GIGE;
          return (boardType & mask) == mask;
        }
    }
}

static inline bool SisoBoardIsCXP(const int boardType)
{
    switch (boardType) {
        case PN_MICROENABLE5A1CXP4:
        case PN_MICROENABLE5AQ8CXP6B:
        case PN_MICROENABLE5VQ8CXP6B:
        case PN_MICROENABLE5AQ8CXP6D:
        case PN_MICROENABLE5VQ8CXP6D:
        case PN_MICROENABLE5_MARATHON_ACX_QP:
        case PN_MICROENABLE5_MARATHON_ACX_SP:
        case PN_MICROENABLE5_MARATHON_ACX_DP:
        case PN_MICROENABLE5_MARATHON_VCX_QP:
        case PN_MICROENABLE6_IMAWORX_CXP12_QUAD:
        case PN_MICROENABLE6_IMAFLEX_CXP12_QUAD:
        case PN_MICROENABLE6_CXP12_IC_1C:
        case PN_MICROENABLE6_CXP12_IC_2C:
        case PN_MICROENABLE6_CXP12_IC_4C:
        case PN_MICROENABLE6_ELEGANCE_ECO:

        case PN_MICROENABLE6_IMPULSE_CX4S:
        case PN_MICROENABLE6_IMPULSE_CX1S:
        case PN_MICROENABLE6_IMPULSE_CX2S:
        case PN_MICROENABLE6_IMPULSE_CX4A:
        case PN_MICROENABLE6_IMPULSE_CX4X:
        case PN_MICROENABLE6_IMPULSE_CX5A:
        case PN_MICROENABLE6_IMPULSE_PROTOTYPE:
        case PN_MICROENABLE6_CXP12_LB_2C:
            return true;
        default:
        {
          int mask = PN_GENERIC_EVA | PN_GENERIC_CXP;
          return (boardType & mask) == mask;
        }
    }
}

static inline bool SisoBoardIsCXP12(const int boardType)
{
    switch (boardType) {
        case PN_MICROENABLE6_IMAWORX_CXP12_QUAD:
        case PN_MICROENABLE6_IMAFLEX_CXP12_QUAD:
        case PN_MICROENABLE6_CXP12_IC_1C:
        case PN_MICROENABLE6_CXP12_IC_2C:
        case PN_MICROENABLE6_CXP12_IC_4C:
        case PN_MICROENABLE6_ELEGANCE_ECO:

        case PN_MICROENABLE6_IMPULSE_CX4S:
        case PN_MICROENABLE6_IMPULSE_CX1S:
        case PN_MICROENABLE6_IMPULSE_CX2S:
        case PN_MICROENABLE6_IMPULSE_CX4A:
        case PN_MICROENABLE6_IMPULSE_CX4X:
        case PN_MICROENABLE6_IMPULSE_CX5A:
        case PN_MICROENABLE6_IMPULSE_PROTOTYPE:
        case PN_MICROENABLE6_CXP12_LB_2C:
            return true;
        default:
        {
          int mask = PN_GENERIC_EVA | PN_GENERIC_CXP12;
          return (boardType & mask) == mask;
        }
    }
}

static inline bool SisoBoardIsCLHS(const int boardType)
{
    switch (boardType) {
        case PN_MICROENABLE5A1CLHSF2:
        case PN_MICROENABLE5AD8CLHSF2:
        case PN_MICROENABLE5A2CLHSF2:
        case PN_MICROENABLE5_MARATHON_AF2_DP:
        case PN_MICROENABLE5_MARATHON_VF2_DP:
            return true;
        default:
        {
          int mask = PN_GENERIC_EVA | PN_GENERIC_CLHS;
          return (boardType & mask) == mask;
        }
    }
}

static inline bool SisoBoardIsV(const int boardType)
{
    switch (boardType) {
    case PN_MICROENABLE4VD1CL:
    case PN_MICROENABLE4VD4CL:
    case PN_MICROENABLE4VQ4GE:
    case PN_MICROENABLE5VD8CL:
    case PN_MICROENABLE5VQ8CXP6B:
    case PN_MICROENABLE5VQ8CXP6D:
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL_PROTOTYPE:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL:
    case PN_MICROENABLE5_MARATHON_VCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx:
    case PN_MICROENABLE5_MARATHON_VCLx:
    case PN_MICROENABLE5_MARATHON_DeepVCL:
    case PN_MICROENABLE5_MARATHON_VCX_QP:
    case PN_MICROENABLE5_MARATHON_VF2_DP:
    case PN_MICROENABLE6_IMPULSE_KCU116:
    case PN_MICROENABLE6_IMAFLEX_CXP12_QUAD:
        return true;
    default:
        return false;
    }
}

static inline int SisoBoardNumberOfPhysicalPorts(const int boardType)
{
    switch (boardType) {
    case PN_MICROENABLE4AS1CL:
    case PN_MICROENABLE5_MARATHON_ACX_SP:
    case PN_MICROENABLE6_CXP12_IC_1C:
    case PN_MICROENABLE6_IMPULSE_CX1S:
        return 1;
    case PN_MICROENABLE4AD1CL:
    case PN_MICROENABLE4BASEx4:
    case PN_MICROENABLE4AD4CL:
    case PN_MICROENABLE4VD1CL:
    case PN_MICROENABLE4VD4CL:
    case PN_MICROENABLE5VD8CL:
    case PN_MICROENABLE5AD8CL:
    case PN_MICROENABLE5A1CLHSF2:
    case PN_MICROENABLE5AD8CLHSF2:
    case PN_MICROENABLE5A2CLHSF2:
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL_PROTOTYPE:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL:
    case PN_MICROENABLE5_MARATHON_VCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx:
    case PN_MICROENABLE5_MARATHON_VCLx:
    case PN_MICROENABLE5_MARATHON_DeepVCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL:
    case PN_MICROENABLE5_LIGHTBRIDGE_ACL:
    case PN_MICROENABLE5_MARATHON_ACL:
    case PN_MICROENABLE5_MARATHON_ACX_DP:
    case PN_MICROENABLE5_MARATHON_AF2_DP:
    case PN_MICROENABLE5_MARATHON_VF2_DP:
    case PN_MICROENABLE6_CXP12_IC_2C:
    case PN_MICROENABLE6_IMPULSE_CX2S:
    case PN_MICROENABLE6_LIGHTBRIDGE_FB2A:
    case PN_MICROENABLE6_CXP12_LB_2C:
        return 2;
    case PN_MICROENABLE4VQ4GE:
    case PN_MICROENABLE4AQ4GE:
    case PN_MICROENABLE5A1CXP4:
    case PN_MICROENABLE5AQ8CXP6B:
    case PN_MICROENABLE5VQ8CXP6B:
    case PN_MICROENABLE5AQ8CXP6D:
    case PN_MICROENABLE5VQ8CXP6D:
    case PN_MICROENABLE5_MARATHON_ACX_QP:
    case PN_MICROENABLE5_MARATHON_VCX_QP:
    case PN_MICROENABLE5_ABACUS_4G_PROTOTYPE:
    case PN_MICROENABLE5_ABACUS_4G_BASE:
    case PN_MICROENABLE5_ABACUS_4G_BASE_II:
    case PN_MICROENABLE6_IMPULSE_KCU116:
    case PN_MICROENABLE6_IMAWORX_CXP12_QUAD:
    case PN_MICROENABLE6_IMAFLEX_CXP12_QUAD:
    case PN_MICROENABLE6_CXP12_IC_4C:
    case PN_MICROENABLE6_ABACUS_4TG:
    case PN_MICROENABLE6_ELEGANCE_ECO:
    case PN_MICROENABLE6_IMPULSE_CX4S:
    case PN_MICROENABLE6_IMPULSE_CX4A:
    case PN_MICROENABLE6_IMPULSE_CX4X:
    case PN_MICROENABLE6_IMPULSE_FB4X:
        return 4;
    case PN_MICROENABLE6_IMPULSE_CX5A:
    case PN_MICROENABLE6_IMPULSE_PROTOTYPE:
        return 5;
    default:
        return 0;
    }
}

static inline int SisoBoardNumberOfPCIeLanes(const int boardType)
{
    switch (boardType) {
    case PN_MICROENABLE4AS1CL:
    case PN_MICROENABLE4AD1CL:
    case PN_MICROENABLE4VD1CL:
        return 1;
    case PN_MICROENABLE4BASEx4:
    case PN_MICROENABLE4AD4CL:
    case PN_MICROENABLE4VD4CL:
    case PN_MICROENABLE4VQ4GE:
    case PN_MICROENABLE4AQ4GE:
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL_PROTOTYPE:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL:
    case PN_MICROENABLE5_MARATHON_VCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx:
    case PN_MICROENABLE5_MARATHON_VCLx:
    case PN_MICROENABLE5_MARATHON_DeepVCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL:
    case PN_MICROENABLE5_LIGHTBRIDGE_ACL:
    case PN_MICROENABLE5_MARATHON_ACL:
    case PN_MICROENABLE5_MARATHON_ACX_DP:
    case PN_MICROENABLE5_MARATHON_ACX_SP:
    case PN_MICROENABLE5_MARATHON_ACX_QP:
    case PN_MICROENABLE5_MARATHON_VCX_QP:
    case PN_MICROENABLE5_MARATHON_AF2_DP:
    case PN_MICROENABLE5_MARATHON_VF2_DP:
    case PN_MICROENABLE5_ABACUS_4G_PROTOTYPE:
    case PN_MICROENABLE5_ABACUS_4G_BASE:
    case PN_MICROENABLE5_ABACUS_4G_BASE_II:
    case PN_MICROENABLE6_CXP12_IC_1C:
    case PN_MICROENABLE6_IMPULSE_CX1S:
    case PN_MICROENABLE6_LIGHTBRIDGE_FB2A:
    case PN_MIPI_1200_PB_12C:
    case PN_MICROENABLE6_CXP12_LB_2C:
        return 4;
    case PN_MICROENABLE5VD8CL:
    case PN_MICROENABLE5AD8CL:
    case PN_MICROENABLE5A1CLHSF2:
    case PN_MICROENABLE5AD8CLHSF2:
    case PN_MICROENABLE5A2CLHSF2:
    case PN_MICROENABLE5A1CXP4:
    case PN_MICROENABLE5AQ8CXP6B:
    case PN_MICROENABLE5VQ8CXP6B:
    case PN_MICROENABLE5AQ8CXP6D:
    case PN_MICROENABLE5VQ8CXP6D:
    case PN_MICROENABLE6_IMPULSE_KCU116:
    case PN_MICROENABLE6_IMAWORX_CXP12_QUAD:
    case PN_MICROENABLE6_IMAFLEX_CXP12_QUAD:
    case PN_MICROENABLE6_CXP12_IC_2C:
    case PN_MICROENABLE6_CXP12_IC_4C:
    case PN_MICROENABLE6_ABACUS_4TG:
    case PN_MICROENABLE6_ELEGANCE_ECO:

    case PN_MICROENABLE6_IMPULSE_CX4S:
    case PN_MICROENABLE6_IMPULSE_CX2S:
    case PN_MICROENABLE6_IMPULSE_CX4A:
    case PN_MICROENABLE6_IMPULSE_CX4X:
    case PN_MICROENABLE6_IMPULSE_CX5A:
    case PN_MICROENABLE6_IMPULSE_FB4X:
    case PN_MICROENABLE6_IMPULSE_PROTOTYPE:
        return 8;
    default:
        return 0;
    }
}

static inline int SisoBoardPCIeGeneration(const int boardType)
{
    switch (boardType) {
    case PN_MICROENABLE4AS1CL:
    case PN_MICROENABLE4AD1CL:
    case PN_MICROENABLE4VD1CL:
    case PN_MICROENABLE4BASEx4:
    case PN_MICROENABLE4AD4CL:
    case PN_MICROENABLE4VD4CL:
    case PN_MICROENABLE4VQ4GE:
    case PN_MICROENABLE4AQ4GE:
        return 1;
    case PN_MICROENABLE5VD8CL:
    case PN_MICROENABLE5AD8CL:
    case PN_MICROENABLE5A1CLHSF2:
    case PN_MICROENABLE5AD8CLHSF2:
    case PN_MICROENABLE5A2CLHSF2:
    case PN_MICROENABLE5A1CXP4:
    case PN_MICROENABLE5AQ8CXP6B:
    case PN_MICROENABLE5VQ8CXP6B:
    case PN_MICROENABLE5AQ8CXP6D:
    case PN_MICROENABLE5VQ8CXP6D:
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL_PROTOTYPE:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL:
    case PN_MICROENABLE5_MARATHON_VCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx:
    case PN_MICROENABLE5_MARATHON_VCLx:
    case PN_MICROENABLE5_MARATHON_DeepVCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL:
    case PN_MICROENABLE5_LIGHTBRIDGE_ACL:
    case PN_MICROENABLE5_MARATHON_ACL:
    case PN_MICROENABLE5_MARATHON_ACX_DP:
    case PN_MICROENABLE5_MARATHON_ACX_SP:
    case PN_MICROENABLE5_MARATHON_ACX_QP:
    case PN_MICROENABLE5_MARATHON_VCX_QP:
    case PN_MICROENABLE5_MARATHON_AF2_DP:
    case PN_MICROENABLE5_MARATHON_VF2_DP:
    case PN_MICROENABLE5_ABACUS_4G_PROTOTYPE:
    case PN_MICROENABLE5_ABACUS_4G_BASE:
    case PN_MICROENABLE5_ABACUS_4G_BASE_II:
        return 2;
    case PN_MICROENABLE6_IMPULSE_KCU116:
    case PN_MICROENABLE6_IMAWORX_CXP12_QUAD:
    case PN_MICROENABLE6_IMAFLEX_CXP12_QUAD:
    case PN_MICROENABLE6_ABACUS_4TG:
    case PN_MICROENABLE6_CXP12_IC_1C:
    case PN_MICROENABLE6_CXP12_IC_2C:
    case PN_MICROENABLE6_CXP12_IC_4C:
    case PN_MICROENABLE6_ELEGANCE_ECO:

    case PN_MICROENABLE6_IMPULSE_CX4S:
    case PN_MICROENABLE6_IMPULSE_CX1S:
    case PN_MICROENABLE6_IMPULSE_CX2S:
    case PN_MICROENABLE6_IMPULSE_CX4A:
    case PN_MICROENABLE6_IMPULSE_CX4X:
    case PN_MICROENABLE6_IMPULSE_CX5A:
    case PN_MICROENABLE6_IMPULSE_FB4X:
    case PN_MICROENABLE6_LIGHTBRIDGE_FB2A:
    case PN_MICROENABLE6_IMPULSE_PROTOTYPE:
    case PN_MIPI_1200_PB_12C:
    case PN_MICROENABLE6_CXP12_LB_2C:
        return 3;
    default:
        return 0;
    }
}

static inline bool SisoBoardIsOEM(const int boardType)
{
    switch (boardType) {
    case PN_TDI:
    case PN_TDI_I:
    case PN_TDI_II:
    case PN_TDI_III:
    case PN_TTDI:
    case PN_TGATE:
    case PN_TGATE_I:
    case PN_TGATE_II:
    case PN_TGATE_USB:
    case PN_TGATE_I_USB:
    case PN_TGATE_II_USB:
    case PN_TGATE_35:
    case PN_TGATE_I_35:
    case PN_TGATE_II_35:
    case PN_TGATE_35_USB:
    case PN_TGATE_I_35_USB:
    case PN_TGATE_II_35_USB:
    case PN_MICROENABLE5_ABACUS_4G_PROTOTYPE:
    case PN_MICROENABLE5_ABACUS_4G_BASE:
    case PN_MICROENABLE5_ABACUS_4G_BASE_II:
    case PN_MICROENABLE6_ABACUS_4TG:
    case PN_MICROENABLE6_CXP12_IC_1C:
    case PN_MICROENABLE6_CXP12_IC_2C:
    case PN_MICROENABLE6_CXP12_IC_4C:
    case PN_MICROENABLE6_ELEGANCE_ECO:
        return true;
    default:
        return false;
    }
}

static inline bool SisoBoardProdHasExtendedType(const int boardType)
{
    switch (boardType) {
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_ACL:
    case PN_MICROENABLE5_MARATHON_ACL:
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL:
    case PN_MICROENABLE5_MARATHON_VCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx:
    case PN_MICROENABLE5_MARATHON_VCLx:
    case PN_MICROENABLE5_MARATHON_DeepVCL:
    case PN_TDI:
    case PN_TDI_I:
    case PN_TDI_II:
    case PN_TDI_III:
    case PN_TGATE:
    case PN_TGATE_I:
    case PN_TGATE_II:
    case PN_TGATE_USB:
    case PN_TGATE_I_USB:
    case PN_TGATE_II_USB:
    case PN_TGATE_35:
    case PN_TGATE_I_35:
    case PN_TGATE_II_35:
    case PN_TGATE_35_USB:
    case PN_TGATE_I_35_USB:
    case PN_TGATE_II_35_USB:
        return true;
    default:
        return false;
    }
}

static inline bool SisoBoardHasExtendedType(const int boardType)
{
	switch (boardType) {
	case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL:
	case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL:
	case PN_MICROENABLE5_LIGHTBRIDGE_ACL:
	case PN_MICROENABLE5_MARATHON_ACL:
	case PN_MICROENABLE5_LIGHTBRIDGE_VCL:
	case PN_MICROENABLE5_MARATHON_VCL:
	case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx:
	case PN_MICROENABLE5_MARATHON_VCLx:
	case PN_MICROENABLE5_MARATHON_DeepVCL:
		return true;
	default:
		return false;
	}
}

static inline enum siso_board_type SisoBoardTypeFromSerialNumber(unsigned int serial)
{
    enum siso_board_type boardType = (enum siso_board_type) ((serial >> 20) & 0xfff);
    if (boardType == PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL ||
        boardType == PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL ||
        boardType == PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx) {
            boardType = (enum siso_board_type) ((serial >> 16) & 0xfffd);
    }
    if (boardType == PN_TDI ||
        boardType == PN_TGATE ||
        boardType == PN_TGATE_USB ||
        boardType == PN_TGATE_35 ||
        boardType == PN_TGATE_35_USB) {
            boardType = (enum siso_board_type) ((serial >> 16) & 0xffff);
    }

    return boardType;
};

static inline enum siso_board_type SisoBoardBaseTypeFromExtendedType(const int boardType)
{
    switch (boardType) {
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL:
    case PN_MICROENABLE5_LIGHTBRIDGE_ACL:
    case PN_MICROENABLE5_MARATHON_ACL:
        return PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_ACL;
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL:
    case PN_MICROENABLE5_LIGHTBRIDGE_VCL:
    case PN_MICROENABLE5_MARATHON_VCL:
        return PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCL;
    case PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx:
    case PN_MICROENABLE5_MARATHON_VCLx:
    case PN_MICROENABLE5_MARATHON_DeepVCL:
        return PN_MICROENABLE5_LIGHTBRIDGE_MARATHON_VCLx;
    case PN_TDI:
    case PN_TDI_I:
    case PN_TDI_II:
    case PN_TDI_III:
        return PN_TDI;
    case PN_TGATE:
    case PN_TGATE_I:
    case PN_TGATE_II:
        return PN_TGATE;
    case PN_TGATE_USB:
    case PN_TGATE_I_USB:
    case PN_TGATE_II_USB:
        return PN_TGATE_USB;
    case PN_TGATE_35:
    case PN_TGATE_I_35:
    case PN_TGATE_II_35:
        return PN_TGATE_35;
    case PN_TGATE_35_USB:
    case PN_TGATE_I_35_USB:
    case PN_TGATE_II_35_USB:
        return PN_TGATE_35_USB;
    default:
        return (enum siso_board_type)boardType;
    }
}

static inline bool SisoBoardSupportTGS(const int boardType)
{
    switch (boardType) {
    case PN_MICROENABLE6_IMAWORX_CXP12_QUAD:
    case PN_MICROENABLE6_CXP12_IC_1C:
    case PN_MICROENABLE6_CXP12_IC_2C:
    case PN_MICROENABLE6_CXP12_IC_4C:
        return true;
    default:
        return false;
    }
}

#endif

#endif
