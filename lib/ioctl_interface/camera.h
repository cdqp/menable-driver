/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/


#ifndef LIB_IOCTL_INTERFACE_CAMERA_H_
#define LIB_IOCTL_INTERFACE_CAMERA_H_

#include "../os/types.h"

/**
 * The possible power states.
 */
typedef enum power_state {
    POWER_STATE_UNKNOWN,   //!< Invalid value
    POWER_STATE_OFF,       //!< Power is set to off
    POWER_STATE_ON,        //!< Power is set to on
    POWER_STATE_TEST_MODE, //!< Test mode is active
} power_state;

// TODO: This doesn't belong here as it is specific to the mE5; replace the way things are done in the discovery with SHalCameraControl.. calls and implement stuff in a platform agnostic way!
typedef enum CONTROL_CORE_MASK {
	CONTROL_CORE_RESET_NONE = 0, //!< None of them is set to reset
	CONTROL_CORE_RESET_TX   = 1, //!< Transmitter is set to reset 
	CONTROL_CORE_RESET_RX   = 2, //!< Receiver is set to reset 
	CONTROL_CORE_RESET_MN   = 4, //!< Monitor is set to reset
	CONTROL_CORE_RESET_ALL  = 7, //!< Monitor is set to reset
} CONTROL_CORE_MASK;

/**
 * The possible data path states.
 */
typedef enum data_path_state {
    DATA_PATH_STATE_UNKNOWN,    //!< Invalid value
    DATA_PATH_STATE_FULL_RESET, //!< Data path is in full reset
    DATA_PATH_STATE_INACTIVE,   //!< Data path is inactive
    DATA_PATH_STATE_SENDING_IDLES, //!< Data path is sending idles, monitoring and receiver are in reset
    DATA_PATH_STATE_MONITORING, //!< Data path is sending idles, monitoring is active, receiver is in reset
    DATA_PATH_STATE_ACTIVE,     //!< Data path is active
} data_path_state;

/**
 * The possible data path speeds.
 */
typedef enum data_path_speed {
    DATA_PATH_SPEED_UNKNOWN,       //!< Invalid value
    DATA_PATH_SPEED_1000 = 1000,   //!< 1000MBit/s
    DATA_PATH_SPEED_1250 = 1250,   //!< 1250MBit/s
    DATA_PATH_SPEED_2500 = 2500,   //!< 2500MBit/s
    DATA_PATH_SPEED_3125 = 3125,   //!< 3125MBit/s
    DATA_PATH_SPEED_5000 = 5000,   //!< 5000MBit/s
    DATA_PATH_SPEED_6250 = 6250,   //!< 6250MBit/s
    DATA_PATH_SPEED_10000 = 10000, //!< 10000MBit/s
    DATA_PATH_SPEED_12500 = 12500, //!< 12500MBit/s
} data_path_speed;

/**
 * The possible up link data path speeds.
 */
typedef enum data_path_up_speed {
    DATA_PATH_UP_SPEED_UNKNOWN, //!< Invalid value
    DATA_PATH_UP_SPEED_LOW,     //!< Up link speed low
    DATA_PATH_UP_SPEED_HIGH,    //!< Up link speed high
} data_path_up_speed;

/**
 * The possible cxp versions.
 */
typedef enum cxp_standard_version {
    CXP_STANDARD_VERSION_UNKNOWN, //!< Invalid value
    CXP_STANDARD_VERSION_1_0,     //!< CXP Standard 1.0
    CXP_STANDARD_VERSION_1_1,     //!< CXP Standard 1.1
    CXP_STANDARD_VERSION_2_0,     //!< CXP Standard 2.0
} cxp_standard_version;

/**
 * The possible cxp led states.
 */
typedef enum cxp_led_state {
    CXP_LED_STATE_UNKNOWN,             //!< Invalid value
    CXP_LED_STATE_BOOTING,             //!< Booting
    CXP_LED_STATE_POWERED,             //!< Software is up, power is enabled
    CXP_LED_STATE_DISCOVERY,           //!< Software is scanning the port for devices
    CXP_LED_STATE_CONNECTED,           //!< Software detected a device
    CXP_LED_STATE_WAITING_FOR_EVENT,   //!< Software is waiting for an event
    CXP_LED_STATE_INCOMPATIBLE_DEVICE, //!< Software detected an incompatible device
    CXP_LED_STATE_SYSTEM_ERROR ,       //!< Software detected an error
} cxp_led_state;

/**
 * The possible acquisition states.
 */
typedef enum acquisition_state {
    ACQUISITION_STATE_UNKNOWN, //!< Invalid value
    ACQUISITION_STATE_STOPPED, //!< Acquisition stopped
    ACQUISITION_STATE_STARTED, //!< Acquisition started
} acquisition_state;

typedef enum camera_command {
    CAMERA_COMMAND_GET_VERSION,
    CAMERA_COMMAND_RESET,
    CAMERA_COMMAND_SET_PORT_MAP,
    CAMERA_COMMAND_SET_PORT_POWER_STATE,
    CAMERA_COMMAND_SET_PORT_DATA_PATH_STATE,
    CAMERA_COMMAND_SET_PORT_DATA_PATH_SPEED,
    CAMERA_COMMAND_SET_PORT_CXP_STANDARD_VERSION,
    CAMERA_COMMAND_SET_PORT_CXP_LED_STATE,
    CAMERA_COMMAND_SET_PORT_ACQUISITION_STATE,
    CAMERA_COMMAND_SET_PORT_CXP_CAMERA_DOWNSCALING,
    CAMERA_COMMAND_SET_STREAM_ID
} camera_command;

// To be sure that the following structs have the same layout across compiler versions,
// all padding is removed.
#pragma pack(push, 1)

typedef union camera_control_input_args {
    struct {
        unsigned long long port_map;
    } set_port_map;
    struct {
        unsigned int port;
        unsigned int param;
    } set_port_param;
    struct {
        uint16_t stream_id;
        uint32_t master_port;
    } set_stream_id;
    /* Version 1 */
} camera_control_input_args;

typedef struct camera_control_input {
    unsigned int _size;      /* Size of this header */
    unsigned int _version;   /* Version of this header */
    unsigned int command;
    union camera_control_input_args args;
    /* Version 1 */
} camera_control_input;

typedef struct camera_control_output {
    unsigned int _size;      /* Size of this header */
    unsigned int _version;   /* Version of this header */
    /* Version 1 */
} camera_control_output;

#ifdef __linux__
/**
 * A union for linux ioctls, since there we have only one buffer for
 * input and output.
 */
typedef union camera_control_io {
    struct camera_control_input in;
    struct camera_control_output out;
} camera_control_io;

#endif

#pragma pack(pop)

#endif /* LIB_IOCTL_INTERFACE_CAMERA_H_ */
