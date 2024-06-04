/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include "cxp_frontend.h"

/* debugging first */
#if defined(DBG_CXP_FRONTEND)
#	undef MEN_DEBUG
#	define MEN_DEBUG
#endif

#define DBG_NAME "[CXP] "
#include "../helpers/dbg.h"
#include "../os/print.h"


#include "../helpers/helper.h"
#include "../helpers/bits.h"
#include "../helpers/error_handling.h"
#include "../helpers/memory.h"
#include "../helpers/timeout.h"
#include "../helpers/type_hierarchy.h"
#include "../os/kernel_macros.h"
#include "../os/string.h"
#include "../os/types.h"
#include "../os/assert.h"

#include "../fpga/register_interface.h"

#include <multichar.h>
#include "sisoboards.h"

#define REG_CXP_POWER_CTRL           0x800
#define CXP_POWER_CTRL_ENABLE        BIT_0
#define CXP_POWER_CTRL_TEST_MODE     BIT_1
#define CXP_POWER_CTRL_PORT_MASK     (BIT_1 | BIT_0)
#define CXP_POWER_CTRL_PORT_SHIFT(p) (2 * p)

#define REG_CXP_RESET_CTRL                   0x801
#define CXP_RESET_CTRL_HOST_TX_BUFFER        BIT_0
#define CXP_RESET_CTRL_HOST_RX_PATH          BIT_1
#define CXP_RESET_CTRL_HOST_MONITOR          BIT_2
#define CXP_RESET_CTRL_TRANSCEIVER_MONITOR   BIT_3
#define CXP_RESET_CTRL_TRANSCEIVER           BIT_4
#define CXP_RESET_CTRL_HOST_PORT_MASK        (BIT_2 | BIT_1 | BIT_0)
#define CXP_RESET_CTRL_TRANSCEIVER_PORT_MASK (BIT_4 | BIT_3)
#define CXP_RESET_CTRL_PORT_SHIFT(p)         (5 * p)

#define REG_CXP_DATA_PATH_STATUS                 0x801
#define CXP_DATA_PATH_STATUS_READY               BIT_0
#define CXP_DATA_PATH_STATUS_PORT_SHIFT(p)       (p)
#define CXP_DATA_PATH_STATUS_READY_TIMEOUT_MSECS 100

#define REG_CXP_DOWNLINK_BITRATE_0 0x802
#define REG_CXP_DOWNLINK_BITRATE_1 0x803
#define REG_CXP_DOWNLINK_BITRATE_2 0x804
#define REG_CXP_DOWNLINK_BITRATE_3 0x805
#define REG_CXP_DOWNLINK_BITRATE_4 0x818

#define CXP_DOWNLINK_BITRATE_1250  0x0
#define CXP_DOWNLINK_BITRATE_2500  0x1
#define CXP_DOWNLINK_BITRATE_3125  0x2
#define CXP_DOWNLINK_BITRATE_5000  0x3
#define CXP_DOWNLINK_BITRATE_6250  0x4
#define CXP_DOWNLINK_BITRATE_10000 0x5
#define CXP_DOWNLINK_BITRATE_12500 0x6

#define REG_CXP_STANDARD_CTRL               0x806
#define CXP_STANDARD_CTRL_UPLINK_BITRATE_21 0
#define CXP_STANDARD_CTRL_UPLINK_BITRATE_42 BIT_0
#define CXP_STANDARD_CTRL_CXP_1_0           0
#define CXP_STANDARD_CTRL_CXP_1_1           BIT_1
#define CXP_STANDARD_CTRL_CXP_2_0           BIT_2
#define CXP_STANDARD_CTRL_PORT_MASK         (BIT_2 | BIT_1 | BIT_0)
#define CXP_STANDARD_CTRL_PORT_SHIFT(p)     (3 * p)

#define REG_ACQUISITION_CTRL           0x807
#define ACQUISITION_CTRL_HOST_ENABLE   BIT_0
#define ACQUISITION_CTRL_PORT_MASK     BIT_0
#define ACQUISITION_CTRL_PORT_SHIFT(p) (1 * p)

#define REG_CXP_DISCOVERY_CONFIG 0x808

#define REG_CXP_LED_CTRL_0               0x811
#define REG_CXP_LED_CTRL_1               0x812
#define REG_CXP_LED_CTRL_2               0x813
#define REG_CXP_LED_CTRL_3               0x814
#define REG_CXP_LED_CTRL_4               0x81B

#define CXP_LED_CTRL_BOOTING             0x0
#define CXP_LED_CTRL_POWERED             0x1
#define CXP_LED_CTRL_DISCOVERY           0x2
#define CXP_LED_CTRL_INCOMPATIBLE_DEVICE 0x3
#define CXP_LED_CTRL_WAIT_FOR_EVENT      0x4
#define CXP_LED_CTRL_CONNECTED           0x5
#define CXP_LED_CTRL_SYSTEM_ERROR        0x6

#define CXP_PORT_MAP_INVALID                0xffffffffffffffffull
#define CXP_PORT_MAP_PORT_MASK              0xf
#define CXP_PORT_MAP_LOG2PHYS_PORT_SHIFT(p) (p * 4)
#define CXP_PORT_MAP_PHYS2LOG_PORT_SHIFT(p) (32 + p * 4)

#define REG_CXP_LOAD_APPLET_CTRL 0x815
#define CXP_LOAD_APPLET_CTRL_REQUEST 0x1
#define REG_CXP_LOAD_APPLET_STATUS 0x815
#define CXP_LOAD_APPLET_STATUS_DONE(num_ports) ((1 << num_ports) - 1)
#define CXP_LOAD_APPLET_STATUS_TIMEOUT_IN_MS 100

#define REG_CXP_PORT_MONITOR_CTRL 0x816

#define REG_CXP_CAMERA_DOWNSCALE_CTRL            0x817
#define CXP_CAMERA_DOWNSCALE_MAX_CONNECTIONS     4
#define CXP_CAMERA_DOWNSCALE_CTRL_PORT_FROM(p) (2 * (p))
#define CXP_CAMERA_DOWNSCALE_CTRL_PORT_TO(p)   (CXP_CAMERA_DOWNSCALE_CTRL_PORT_FROM(p) + 1)

#define REG_CONFIG_IMAGE_STREAM_ID_0				0x081C

// clang-format off

static const uint32_t MEMORY_TAG_FRONTEND = MULTICHAR32('F', 'P', 'X', 'C');
static const uint32_t MEMORY_TAG_PORTS = MULTICHAR32('P', 'P', 'X', 'C');

#define CXP_NUM_PORT_MAP_ENTRIES_1C 2
static uint64_t cxp_port_maps_1ch[CXP_NUM_PORT_MAP_ENTRIES_1C] = {
	0x7654321076543210ull,
	CXP_PORT_MAP_INVALID
};

#define CXP_NUM_PORT_MAP_ENTRIES_2C 3
static uint64_t cxp_port_maps_2ch[CXP_NUM_PORT_MAP_ENTRIES_2C] = {
	0x7654321076543210ull,
	0x7654320176543201ull,
	CXP_PORT_MAP_INVALID
};

#define CXP_NUM_PORT_MAP_ENTRIES_4C 25
static uint64_t cxp_port_maps_4ch[CXP_NUM_PORT_MAP_ENTRIES_4C] = {
	0x7654321076543210ull,
	0x7654231076542310ull,
	0x7654312076543120ull,
	0x7654213076541320ull,
	0x7654132076542130ull,
	0x7654123076541230ull,
	0x7654320176543201ull,
	0x7654230176542301ull,
	0x7654310276543021ull,
	0x7654210376540321ull,
	0x7654130276542031ull,
	0x7654120376540231ull,
	0x7654302176543102ull,
	0x7654203176541302ull,
	0x7654301276543012ull,
	0x7654201376540312ull,
	0x7654103276541032ull,
	0x7654102376540132ull,
	0x7654032176542103ull,
	0x7654023176541203ull,
	0x7654031276542013ull,
	0x7654021376540213ull,
	0x7654013276541023ull,
	0x7654012376540123ull,
	CXP_PORT_MAP_INVALID
};

#define CXP_NUM_PORT_MAP_ENTRIES_5C 121
static uint64_t cxp_port_maps_5ch[CXP_NUM_PORT_MAP_ENTRIES_5C] = {
	0x7654321076543210ull,
	0x7653421076534210ull,
	0x7654231076542310ull,
	0x7653241076524310ull,
	0x7652431076532410ull,
	0x7652341076523410ull,
	0x7654312076543120ull,
	0x7653412076534120ull,
	0x7654213076541320ull,
	0x7653214076514320ull,
	0x7652413076531420ull,
	0x7652314076513420ull,
	0x7654132076542130ull,
	0x7653142076524130ull,
	0x7654123076541230ull,
	0x7653124076514230ull,
	0x7652143076521430ull,
	0x7652134076512430ull,
	0x7651432076532140ull,
	0x7651342076523140ull,
	0x7651423076531240ull,
	0x7651324076513240ull,
	0x7651243076521340ull,
	0x7651234076512340ull,
	0x7654320176543201ull,
	0x7653420176534201ull,
	0x7654230176542301ull,
	0x7653240176524301ull,
	0x7652430176532401ull,
	0x7652340176523401ull,
	0x7654310276543021ull,
	0x7653410276534021ull,
	0x7654210376540321ull,
	0x7653210476504321ull,
	0x7652410376530421ull,
	0x7652310476503421ull,
	0x7654130276542031ull,
	0x7653140276524031ull,
	0x7654120376540231ull,
	0x7653120476504231ull,
	0x7652140376520431ull,
	0x7652130476502431ull,
	0x7651430276532041ull,
	0x7651340276523041ull,
	0x7651420376530241ull,
	0x7651320476503241ull,
	0x7651240376520341ull,
	0x7651230476502341ull,
	0x7654302176543102ull,
	0x7653402176534102ull,
	0x7654203176541302ull,
	0x7653204176514302ull,
	0x7652403176531402ull,
	0x7652304176513402ull,
	0x7654301276543012ull,
	0x7653401276534012ull,
	0x7654201376540312ull,
	0x7653201476504312ull,
	0x7652401376530412ull,
	0x7652301476503412ull,
	0x7654103276541032ull,
	0x7653104276514032ull,
	0x7654102376540132ull,
	0x7653102476504132ull,
	0x7652104376510432ull,
	0x7652103476501432ull,
	0x7651403276531042ull,
	0x7651304276513042ull,
	0x7651402376530142ull,
	0x7651302476503142ull,
	0x7651204376510342ull,
	0x7651203476501342ull,
	0x7654032176542103ull,
	0x7653042176524103ull,
	0x7654023176541203ull,
	0x7653024176514203ull,
	0x7652043176521403ull,
	0x7652034176512403ull,
	0x7654031276542013ull,
	0x7653041276524013ull,
	0x7654021376540213ull,
	0x7653021476504213ull,
	0x7652041376520413ull,
	0x7652031476502413ull,
	0x7654013276541023ull,
	0x7653014276514023ull,
	0x7654012376540123ull,
	0x7653012476504123ull,
	0x7652014376510423ull,
	0x7652013476501423ull,
	0x7651043276521043ull,
	0x7651034276512043ull,
	0x7651042376520143ull,
	0x7651032476502143ull,
	0x7651024376510243ull,
	0x7651023476501243ull,
	0x7650432176532104ull,
	0x7650342176523104ull,
	0x7650423176531204ull,
	0x7650324176513204ull,
	0x7650243176521304ull,
	0x7650234176512304ull,
	0x7650431276532014ull,
	0x7650341276523014ull,
	0x7650421376530214ull,
	0x7650321476503214ull,
	0x7650241376520314ull,
	0x7650231476502314ull,
	0x7650413276531024ull,
	0x7650314276513024ull,
	0x7650412376530124ull,
	0x7650312476503124ull,
	0x7650214376510324ull,
	0x7650213476501324ull,
	0x7650143276521034ull,
	0x7650134276512034ull,
	0x7650142376520134ull,
	0x7650132476502134ull,
	0x7650124376510234ull,
	0x7650123476501234ull,
	CXP_PORT_MAP_INVALID
};

// clang-format on

/**
 * Translate physical port number to logical port number.
 */
static uint32_t cxp_get_logical_port_number_from_port_map(uint64_t port_map, uint32_t physical_port_number)
{
	return ((port_map >> CXP_PORT_MAP_PHYS2LOG_PORT_SHIFT(physical_port_number)) & CXP_PORT_MAP_PORT_MASK);
}

/**
 * Translate logical port number to physical port number.
 */
static uint32_t cxp_get_physical_port_number_from_port_map(uint64_t port_map, uint32_t logical_port_number)
{
	return ((port_map >> CXP_PORT_MAP_LOG2PHYS_PORT_SHIFT(logical_port_number)) & CXP_PORT_MAP_PORT_MASK);
}

static bool does_me6_firmware_support_tgs(uint32_t board_type, version_number firmware_version)
{
	typedef struct {
		uint32_t board_type;
		version_number min_firmware_version;
	} tgs_min_firmware_version;

	static const tgs_min_firmware_version min_firmware_versions_for_tgs[] = {
		{PN_MICROENABLE6_CXP12_IC_1C,        {3, 2, 0}},
		{PN_MICROENABLE6_CXP12_IC_2C,        {1, 1, 0}},
		{PN_MICROENABLE6_CXP12_IC_4C,        {1, 1, 0}},
		{PN_MICROENABLE6_IMAWORX_CXP12_QUAD, {1, 1, 0}},
	};

	for (size_t i = 0; i < ARRAY_SIZE(min_firmware_versions_for_tgs); ++i) {
		if (board_type == min_firmware_versions_for_tgs[i].board_type) {
			return is_version_greater_or_equal(firmware_version, min_firmware_versions_for_tgs[i].min_firmware_version);
		}
	}

	/* We assume that all firmwares, which are not listed here, do support TGS, since newer boards will always support it. */
	return true;
}

static bool does_board_applet_support_tgs(struct cxp_frontend* self) {

	// check if board and firmware support setting stream id
	unsigned int board_type = self->ri->read(self->ri, BOARDSTATUSEX);
	board_type = (board_type >> 16) & 0xffff;

	unsigned int board_status = self->ri->read(self->ri, ME6_REG_BOARD_STATUS);
	unsigned int firmware_version = ((board_status & 0x00ff00) >> 8) & 0xff;
	unsigned int firmware_revision = ((board_status & 0xff0000) >> 16) & 0xff;

	/* All me6 boards support TGS, except IC-1C with a firmware below 3.2 */
	const version_number fw_version = {firmware_version, firmware_revision, 0};
	const bool tgs_supported = SisoBoardIsMe6(board_type) && does_me6_firmware_support_tgs(board_type, fw_version);

	if (!tgs_supported) {
		pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set stream id; firmware does not support TGS. firmware version %#x, revision %#x, board %#x\n", firmware_version, firmware_revision, board_type);
	}

	return tgs_supported;
}

static const char* cxp_get_power_state_name(enum power_state state)
{
	switch (state) {
	case POWER_STATE_OFF: return "OFF";
	case POWER_STATE_ON: return "ON";
	case POWER_STATE_TEST_MODE: return "TEST";
	default: return "UNKNOWN";
	}
}

/**
 * Set power state.
 */
static int cxp_set_port_power_state(struct cxp_frontend* self, uint32_t physical_port_number, enum power_state new_power_state)
{
	int ret = 0;

	DBG_TRACE_BEGIN_FCT;

	if (physical_port_number >= self->num_ports) {
		ret = CXP_FRONTEND_ERROR_INVALID_PORT;
		pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set power state; invalid port %d", physical_port_number);
	} else if (new_power_state < POWER_STATE_OFF || new_power_state > POWER_STATE_TEST_MODE) {
		ret = CXP_FRONTEND_ERROR_INVALID_PARAMETER;
		pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set power state; invalid state %d", new_power_state);
	} else if (new_power_state != self->ports[physical_port_number].power_state_cache) {
		uint32_t port, power_ctrl;

		DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " changing port %d power state: %s -> %s\n", physical_port_number,
		              cxp_get_power_state_name(self->ports[physical_port_number].power_state_cache), cxp_get_power_state_name(new_power_state)));

		self->ports[physical_port_number].power_state_cache = new_power_state;

		/* Get power state of all ports for combined register */
		for (port = 0, power_ctrl = 0; port < self->num_ports; ++port) {
			uint32_t port_power_ctrl;

			switch (self->ports[port].power_state_cache) {
			case POWER_STATE_OFF: port_power_ctrl = 0; break;
			case POWER_STATE_TEST_MODE: port_power_ctrl = CXP_POWER_CTRL_ENABLE | CXP_POWER_CTRL_TEST_MODE; break;
			default: port_power_ctrl = CXP_POWER_CTRL_ENABLE; break;
			}

			power_ctrl |= ((port_power_ctrl & CXP_POWER_CTRL_PORT_MASK) << CXP_POWER_CTRL_PORT_SHIFT(port));
		}

		DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " writing 0x%08x to register 0x%04x\n", power_ctrl, self->power_ctrl_register));
		self->ri->write(self->ri, self->power_ctrl_register, power_ctrl);
	}

	DBG_TRACE_END_FCT;

	return ret;
}

static const char* cxp_get_data_path_state_name(enum data_path_state state)
{
	switch (state) {
	case DATA_PATH_STATE_FULL_RESET: return "RESET";
	case DATA_PATH_STATE_INACTIVE: return "INACTIVE";
	case DATA_PATH_STATE_SENDING_IDLES: return "IDLE";
	case DATA_PATH_STATE_MONITORING: return "MONITORING";
	case DATA_PATH_STATE_ACTIVE: return "ACTIVE";
	default: return "UNKNOWN";
	}
}

/**
 * Set data path state.
 */
static int cxp_set_port_data_path_state(struct cxp_frontend* self, uint32_t physical_port_number, enum data_path_state new_data_path_state)
{
	int ret = 0;

	DBG_TRACE_BEGIN_FCT;

	if (physical_port_number >= self->num_ports) {
		ret = CXP_FRONTEND_ERROR_INVALID_PORT;
		pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set data path state; invalid port %d", physical_port_number);
	} else if (new_data_path_state < DATA_PATH_STATE_FULL_RESET || new_data_path_state > DATA_PATH_STATE_ACTIVE) {
		ret = CXP_FRONTEND_ERROR_INVALID_PARAMETER;
		pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set data path state; invalid state %d", new_data_path_state);
	} else {
	
		uint64_t port_map = self->port_maps[self->port_map_index];
		uint32_t logical_port_number = cxp_get_logical_port_number_from_port_map(port_map, physical_port_number);

		if ((new_data_path_state != self->ports[physical_port_number].data_path_state_physical_cache) || 
			(new_data_path_state != self->ports[logical_port_number].data_path_state_logical_cache)) {

			DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " changing port %d physical data path state: %s -> %s\n", physical_port_number,
			              cxp_get_data_path_state_name(self->ports[physical_port_number].data_path_state_physical_cache),
			              cxp_get_data_path_state_name(new_data_path_state)));

			DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " changing port %d logical data path state: %s -> %s\n", logical_port_number,
			              cxp_get_data_path_state_name(self->ports[logical_port_number].data_path_state_logical_cache),
			              cxp_get_data_path_state_name(new_data_path_state)));

			self->ports[physical_port_number].data_path_state_physical_cache = new_data_path_state;
			self->ports[logical_port_number].data_path_state_logical_cache = new_data_path_state;

			/* Get data path state of all ports for combined register */
			uint32_t port, reset_ctrl;
			for (port = 0, reset_ctrl = 0; port < self->num_ports; ++port) {
				uint32_t port_reset_ctrl_host, port_reset_ctrl_transceiver, logical_port;

				switch (self->ports[port].data_path_state_physical_cache) {
				case DATA_PATH_STATE_FULL_RESET:
					port_reset_ctrl_host        = CXP_RESET_CTRL_HOST_MONITOR | CXP_RESET_CTRL_HOST_RX_PATH | CXP_RESET_CTRL_HOST_TX_BUFFER;
					port_reset_ctrl_transceiver = CXP_RESET_CTRL_TRANSCEIVER | CXP_RESET_CTRL_TRANSCEIVER_MONITOR;
					break;
				case DATA_PATH_STATE_SENDING_IDLES:
					port_reset_ctrl_host        = CXP_RESET_CTRL_HOST_MONITOR | CXP_RESET_CTRL_HOST_RX_PATH;
					port_reset_ctrl_transceiver = CXP_RESET_CTRL_TRANSCEIVER_MONITOR;
					break;
				case DATA_PATH_STATE_MONITORING:
					port_reset_ctrl_host        = CXP_RESET_CTRL_HOST_RX_PATH;
					port_reset_ctrl_transceiver = 0;
					break;
				case DATA_PATH_STATE_ACTIVE:
					port_reset_ctrl_host        = 0;
					port_reset_ctrl_transceiver = 0;
					break;
				default:
					port_reset_ctrl_host        = CXP_RESET_CTRL_HOST_MONITOR | CXP_RESET_CTRL_HOST_RX_PATH | CXP_RESET_CTRL_HOST_TX_BUFFER;
					port_reset_ctrl_transceiver = CXP_RESET_CTRL_TRANSCEIVER_MONITOR;
					break;
				}

				logical_port = cxp_get_logical_port_number_from_port_map(port_map, port);
				reset_ctrl |= (((port_reset_ctrl_host & CXP_RESET_CTRL_HOST_PORT_MASK) << CXP_RESET_CTRL_PORT_SHIFT(logical_port))
				               | ((port_reset_ctrl_transceiver & CXP_RESET_CTRL_TRANSCEIVER_PORT_MASK) << CXP_RESET_CTRL_PORT_SHIFT(port)));
			}

			DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " writing 0x%08x to register 0x%04x\n", reset_ctrl, self->reset_ctrl_register));
			self->ri->write(self->ri, self->reset_ctrl_register, reset_ctrl);
		}
	}

	DBG_TRACE_END_FCT;

	return ret;
}

/**
 * Update uplink speed and cxp standard version.
 */
static void cxp_update_standard_ctrl_register(struct cxp_frontend* self, uint32_t physical_port_number)
{
	uint32_t logical_port_number, standard_ctrl;

	DBG_TRACE_BEGIN_FCT;

	/* Get acquisition state of all ports for combined register */
	for (logical_port_number = 0, standard_ctrl = 0; logical_port_number < self->num_ports; ++logical_port_number) {
		uint32_t port_uplink_speed, port_standard_version;

		switch (self->ports[logical_port_number].data_path_up_speed_cache) {
			case DATA_PATH_UP_SPEED_LOW: port_uplink_speed = CXP_STANDARD_CTRL_UPLINK_BITRATE_21; break;
			case DATA_PATH_UP_SPEED_HIGH: port_uplink_speed = CXP_STANDARD_CTRL_UPLINK_BITRATE_42; break;
			default: port_uplink_speed = CXP_STANDARD_CTRL_UPLINK_BITRATE_21; break;
		}

		switch (self->ports[logical_port_number].standard_version_cache) {
			case CXP_STANDARD_VERSION_1_0: port_standard_version = CXP_STANDARD_CTRL_CXP_1_0; break;
			case CXP_STANDARD_VERSION_2_0: port_standard_version = CXP_STANDARD_CTRL_CXP_2_0; break;
			default: port_standard_version = CXP_STANDARD_CTRL_CXP_1_1; break;
		}

		standard_ctrl |= (((port_uplink_speed | port_standard_version) & CXP_STANDARD_CTRL_PORT_MASK) << CXP_STANDARD_CTRL_PORT_SHIFT(logical_port_number));
	}

	DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " writing 0x%08x to register 0x%04x\n", standard_ctrl, self->standard_ctrl_register));
	self->ri->write(self->ri, self->standard_ctrl_register, standard_ctrl);

	DBG_TRACE_END_FCT;
}

static const char* cxp_get_data_path_speed_name(enum data_path_speed speed)
{
	switch (speed) {
	case DATA_PATH_SPEED_1250: return "1.250 Gbit/s";
	case DATA_PATH_SPEED_2500: return "2.500 Gbit/s";
	case DATA_PATH_SPEED_3125: return "3.125 Gbit/s";
	case DATA_PATH_SPEED_5000: return "5.000 Gbit/s";
	case DATA_PATH_SPEED_6250: return "6.250 Gbit/s";
	case DATA_PATH_SPEED_10000: return "10.000 Gbit/s";
	case DATA_PATH_SPEED_12500: return "12.500 Gbit/s";
	case DATA_PATH_SPEED_UNKNOWN: return "UNKNOWN";
	default: return "INVALID";
	}
}

/**
 * Set data path speed.
 */
static int cxp_wait_port_data_path_speed_change_done(struct cxp_frontend* self, uint32_t physical_port_number)
{
    DBG_TRACE_BEGIN_FCT;

    const uint32_t port_bit = CXP_DATA_PATH_STATUS_READY << CXP_DATA_PATH_STATUS_PORT_SHIFT(physical_port_number);

	struct timeout timeout;
	timeout_init(&timeout, self->data_path_speed_change_timeout_msecs);

	self->ri->b2b_barrier(self->ri);

    uint32_t status = 0;
	int count = 0;
	do {
		status = self->ri->read(self->ri, self->data_path_status_register);
		++count;
	} while (((status & port_bit) != port_bit) && !timeout_has_elapsed(&timeout));
	DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " read 0x%08x from register 0x%04x; %d cycles\n", status, self->data_path_status_register, count));

	const int ret = ((status & port_bit) != port_bit)
                      ? CXP_FRONTEND_ERROR_TIMEOUT
                      : 0;

	if (ret != 0) {
		pr_err(KBUILD_MODNAME ": " DBG_NAME " timed out while waiting for data path speed change to be acknowledged");
	}

	return DBG_TRACE_RETURN(ret);
}

/**
 * Set data path speed.
 */
static int cxp_set_port_data_path_speed(struct cxp_frontend* self, uint32_t physical_port_number, enum data_path_speed new_data_path_speed)
{
	int ret = 0;

	DBG_TRACE_BEGIN_FCT;

	if (physical_port_number >= self->num_ports) {
		ret = CXP_FRONTEND_ERROR_INVALID_PORT;
		pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set data path speed; invalid port %d", physical_port_number);
	} else if (new_data_path_speed != DATA_PATH_SPEED_1250 && new_data_path_speed != DATA_PATH_SPEED_2500 && new_data_path_speed != DATA_PATH_SPEED_3125
	           && new_data_path_speed != DATA_PATH_SPEED_5000 && new_data_path_speed != DATA_PATH_SPEED_6250 && new_data_path_speed != DATA_PATH_SPEED_10000
	           && new_data_path_speed != DATA_PATH_SPEED_12500) {
		ret = CXP_FRONTEND_ERROR_INVALID_PARAMETER;
		pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set data path speed; invalid speed %d", new_data_path_speed);
	} else {
		/* Get the logical port*/
		uint64_t port_map = self->port_maps[self->port_map_index];
		uint32_t logical_port_number = cxp_get_logical_port_number_from_port_map(port_map, physical_port_number);

		/* Get downlink bitrate */
		uint32_t downlink_bitrate;
		switch (new_data_path_speed) {
			case DATA_PATH_SPEED_1250: downlink_bitrate = CXP_DOWNLINK_BITRATE_1250; break;
			case DATA_PATH_SPEED_2500: downlink_bitrate = CXP_DOWNLINK_BITRATE_2500; break;
			case DATA_PATH_SPEED_5000: downlink_bitrate = CXP_DOWNLINK_BITRATE_5000; break;
			case DATA_PATH_SPEED_6250: downlink_bitrate = CXP_DOWNLINK_BITRATE_6250; break;
			case DATA_PATH_SPEED_10000: downlink_bitrate = CXP_DOWNLINK_BITRATE_10000; break;
			case DATA_PATH_SPEED_12500: downlink_bitrate = CXP_DOWNLINK_BITRATE_12500; break;
			default: downlink_bitrate = CXP_DOWNLINK_BITRATE_3125; break;
		}

		/* Get uplink bitrate */
        data_path_up_speed new_data_path_up_speed = DATA_PATH_UP_SPEED_LOW;
		if(new_data_path_speed >= DATA_PATH_SPEED_10000) {
			new_data_path_up_speed = DATA_PATH_UP_SPEED_HIGH;
		}
	
		if(	(new_data_path_speed != self->ports[physical_port_number].data_path_dw_speed_cache) ||
			(new_data_path_up_speed != self->ports[logical_port_number].data_path_up_speed_cache)) {

			ret = cxp_wait_port_data_path_speed_change_done(self, physical_port_number);
			if (ret == 0) {
				if (new_data_path_speed != self->ports[physical_port_number].data_path_dw_speed_cache) {
					DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " changing port %d data path speed: %s -> %s\n", physical_port_number,
					              cxp_get_data_path_speed_name(self->ports[physical_port_number].data_path_dw_speed_cache),
					              cxp_get_data_path_speed_name(new_data_path_speed)));

					self->ports[physical_port_number].data_path_dw_speed_cache = new_data_path_speed;

					DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " writing 0x%08x to register 0x%04x\n", downlink_bitrate,
					              self->ports[physical_port_number].downlink_bitrate_register));
					self->ri->write(self->ri, self->ports[physical_port_number].downlink_bitrate_register, downlink_bitrate);

					ret = cxp_wait_port_data_path_speed_change_done(self, physical_port_number);
				}
				if (new_data_path_up_speed != self->ports[logical_port_number].data_path_up_speed_cache) {
					self->ports[logical_port_number].data_path_up_speed_cache = new_data_path_up_speed;
					cxp_update_standard_ctrl_register(self, physical_port_number);
				}
			}
		}
	}

	return DBG_TRACE_RETURN(ret);
}

static const char* cxp_get_standard_version_name(enum cxp_standard_version version)
{
	switch (version) {
	case CXP_STANDARD_VERSION_1_0: return "CXP 1.0";
	case CXP_STANDARD_VERSION_1_1: return "CXP 1.1";
	case CXP_STANDARD_VERSION_2_0: return "CXP 2.0";
	default: return "UNKNOWN";
	}
}

/**
 * Set cxp standard version.
 */
static int cxp_set_port_standard_version(struct cxp_frontend* self, uint32_t physical_port_number, enum cxp_standard_version new_standard_version)
{
	int ret = 0;

	DBG_TRACE_BEGIN_FCT;

	if (physical_port_number >= self->num_ports) {
		ret = CXP_FRONTEND_ERROR_INVALID_PORT;
		pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set standard version; invalid port %d", physical_port_number);
	} else if (new_standard_version < CXP_STANDARD_VERSION_1_0 || new_standard_version > CXP_STANDARD_VERSION_2_0) {
		ret = CXP_FRONTEND_ERROR_INVALID_PARAMETER;
		pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set standard version; invalid version %d", new_standard_version);
	} else{
		uint64_t port_map = self->port_maps[self->port_map_index];
		uint32_t logical_port_number = cxp_get_logical_port_number_from_port_map(port_map, physical_port_number);
		if(new_standard_version != self->ports[logical_port_number].standard_version_cache){
			DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " changing phyiscal port %d, logical port %d, standard version: %s -> %s\n",
				physical_port_number, logical_port_number,
				cxp_get_standard_version_name(self->ports[logical_port_number].standard_version_cache),
				cxp_get_standard_version_name(new_standard_version)));

			self->ports[logical_port_number].standard_version_cache = new_standard_version;
			cxp_update_standard_ctrl_register(self, physical_port_number);
		}
	}

	DBG_TRACE_END_FCT;

	return ret;
}

static const char* cxp_get_led_state_name(enum cxp_led_state state)
{
	switch (state) {
	case CXP_LED_STATE_BOOTING: return "BOOTING";
	case CXP_LED_STATE_POWERED: return "POWERED";
	case CXP_LED_STATE_DISCOVERY: return "DISCOVERY";
	case CXP_LED_STATE_CONNECTED: return "CONNECTED";
	case CXP_LED_STATE_INCOMPATIBLE_DEVICE: return "INCOMPATIBLE_DEVICE";
	case CXP_LED_STATE_SYSTEM_ERROR: return "SYSTEM_ERROR";
	default: return "UNKNOWN";
	}
}

/**
 * Set cxp led state.
 */
static int cxp_set_port_led_state(struct cxp_frontend* self, uint32_t physical_port_number, enum cxp_led_state new_led_state)
{
	int ret = 0;

	DBG_TRACE_BEGIN_FCT;

	if (physical_port_number >= self->num_ports) {
		ret = CXP_FRONTEND_ERROR_INVALID_PORT;
		pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set led state; invalid port %d", physical_port_number);
	} else if (new_led_state < CXP_LED_STATE_BOOTING || new_led_state > CXP_LED_STATE_SYSTEM_ERROR) {
		ret = CXP_FRONTEND_ERROR_INVALID_PARAMETER;
		pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set led state; invalid state %d", new_led_state);
	} else if (new_led_state != self->ports[physical_port_number].led_state_cache) {
		uint32_t led_ctrl;

		DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " changing port %d led state: %s -> %s\n", physical_port_number,
		              cxp_get_led_state_name(self->ports[physical_port_number].led_state_cache), cxp_get_led_state_name(new_led_state)));

		self->ports[physical_port_number].led_state_cache = new_led_state;

		switch (new_led_state) {
		case CXP_LED_STATE_BOOTING: led_ctrl = CXP_LED_CTRL_BOOTING; break;
		case CXP_LED_STATE_POWERED: led_ctrl = CXP_LED_CTRL_POWERED; break;
		case CXP_LED_STATE_DISCOVERY: led_ctrl = CXP_LED_CTRL_DISCOVERY; break;
		case CXP_LED_STATE_CONNECTED: led_ctrl = CXP_LED_CTRL_CONNECTED; break;
		case CXP_LED_STATE_WAITING_FOR_EVENT: led_ctrl = CXP_LED_CTRL_WAIT_FOR_EVENT; break;
		case CXP_LED_STATE_INCOMPATIBLE_DEVICE: led_ctrl = CXP_LED_CTRL_INCOMPATIBLE_DEVICE; break;
		default: led_ctrl = CXP_LED_CTRL_SYSTEM_ERROR; break;
		}

		DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " writing 0x%08x to register 0x%04x\n", led_ctrl, self->ports[physical_port_number].led_ctrl_register));
		self->ri->write(self->ri, self->ports[physical_port_number].led_ctrl_register, led_ctrl);
	}

	DBG_TRACE_END_FCT;

	return ret;
}

static const char* cxp_get_acquisition_state_name(enum acquisition_state state)
{
	switch (state) {
	case ACQUISITION_STATE_STOPPED: return "STOPPED";
	case ACQUISITION_STATE_STARTED: return "STARTED";
	default: return "UNKNOWN";
	}
}

/**
 * Set acquisition state.
 */
static int cxp_set_port_acquisition_state(struct cxp_frontend* self, uint32_t physical_port_number, enum acquisition_state new_acquisition_state)
{
	int ret = 0;

	DBG_TRACE_BEGIN_FCT;

	if (physical_port_number >= self->num_ports) {
		ret = CXP_FRONTEND_ERROR_INVALID_PORT;
		pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set acquisition state; invalid port %d", physical_port_number);
	} else if (new_acquisition_state < ACQUISITION_STATE_STOPPED || new_acquisition_state > ACQUISITION_STATE_STARTED) {
		ret = CXP_FRONTEND_ERROR_INVALID_PARAMETER;
		pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set acquisition state; invalid state %d", new_acquisition_state);
	} else {
	
		uint64_t port_map = self->port_maps[self->port_map_index];
		uint32_t logical_port_number = cxp_get_logical_port_number_from_port_map(port_map, physical_port_number);
		if(new_acquisition_state != self->ports[logical_port_number].acquisition_state_cache){
			DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " changing port %d , logical port %d, acquisition state: %s -> %s\n",
				physical_port_number, logical_port_number,
				cxp_get_acquisition_state_name(self->ports[logical_port_number].acquisition_state_cache),
				cxp_get_acquisition_state_name(new_acquisition_state)));

			self->ports[logical_port_number].acquisition_state_cache = new_acquisition_state;
	
			/* Get acquisition state of all ports for combined register */
			uint32_t logical_port, acquisition_ctrl;
			for (logical_port = 0, acquisition_ctrl = 0; logical_port < self->num_ports; ++logical_port) {
				uint32_t port_acquisition_ctrl;
				if (self->ports[logical_port].acquisition_state_cache == ACQUISITION_STATE_STARTED) {
					port_acquisition_ctrl = ACQUISITION_CTRL_HOST_ENABLE;
				} else {
					port_acquisition_ctrl = 0;
				}
				acquisition_ctrl |= ((port_acquisition_ctrl & ACQUISITION_CTRL_PORT_MASK) << ACQUISITION_CTRL_PORT_SHIFT(logical_port));
			}
	
			DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " writing 0x%08x to register 0x%04x\n", acquisition_ctrl, self->acquisition_status_register));
			self->ri->write(self->ri, self->acquisition_status_register, acquisition_ctrl);
		}
	}

	DBG_TRACE_END_FCT;

	return ret;
}

/**
 * Sets the number of connections for a specific physical port.
 * The port must be the master port of the camera for which the number of links should be set.
 *
 * This is only required on mE6 CXP boards with more than one link.
 * 
 * @param self  the cxp_frontend instance to work on
 * @param physical_port_number The number fo the physical master port to set the number of links for.
 * @param num_connections The number of connections that are used by the camera connected to the master port.
 * 
 * @return A status code. STATUS_OK on success or an 
 */
static int cxp_set_port_camera_donwscaling(struct cxp_frontend * self, unsigned int physical_port_number, unsigned int num_connections) {
	DBG_TRACE_BEGIN_FCT;

	if (physical_port_number >= self->num_ports) {
		pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set camera downscaling; invalid VA port %d\n", physical_port_number);
		return DBG_TRACE_RETURN(CXP_FRONTEND_ERROR_INVALID_PORT);

	} else if (num_connections > CXP_CAMERA_DOWNSCALE_MAX_CONNECTIONS) {
		pr_err(KBUILD_MODNAME ": " DBG_NAME " port %u: failed to set camera downscaling; invalid number of connections: %u\n", physical_port_number, num_connections);
		return DBG_TRACE_RETURN(CXP_FRONTEND_ERROR_INVALID_PARAMETER);

	} else if (num_connections == 0) {
		pr_err(KBUILD_MODNAME ": " DBG_NAME " port %u: failed to set camera downscaling; number of connections may not be 0\n", physical_port_number);
		return DBG_TRACE_RETURN(CXP_FRONTEND_ERROR_INVALID_PARAMETER);

	} else if (self->num_ports == 1) {
		/* Downscaling is only necessary for boards with multiple ports.
		 * For other boards, the downcale register does not exist in the firmware.
		 * 
		 * Attention: If the CXP frontend will be used for boards with multpiple ports
		 * that do not require camera downscaling then this check needs to be improved.
		 */
		pr_debug(KBUILD_MODNAME ": " DBG_NAME "Downscaling of physical port %u to %u connections for single channel board is ignored.\n", physical_port_number, num_connections);
		return DBG_TRACE_RETURN(STATUS_OK);

	} else {
		
		uint64_t port_map = self->port_maps[self->port_map_index];
		uint32_t logical_port_number = cxp_get_logical_port_number_from_port_map(port_map, physical_port_number);
	
		if (num_connections == self->ports[logical_port_number].camera_downscale_state_cache) {
			pr_debug(KBUILD_MODNAME ": " DBG_NAME " logical port %u, camera downscaling is already set to value %u\n", 
			         logical_port_number, num_connections);

		} else {
			pr_debug(KBUILD_MODNAME ": " DBG_NAME " logical port %u, changing number of connections: %u -> %u\n", 
			         logical_port_number, self->ports[logical_port_number].camera_downscale_state_cache, num_connections);

			self->ports[logical_port_number].camera_downscale_state_cache = num_connections;

			/* Compute the register value, which contains the number of links for all logical ports */
			uint8_t new_reg_value = 0;
			for (unsigned int port = 0; port < self->num_ports; ++port) {
				const uint8_t port_value = self->ports[port].camera_downscale_state_cache - 1; /* -1 because 0x00 represents one link, 0x01 represents 2 links etc. */
				SET_BITS_8(new_reg_value, port_value, CXP_CAMERA_DOWNSCALE_CTRL_PORT_FROM(port), CXP_CAMERA_DOWNSCALE_CTRL_PORT_TO(port));
			}

			pr_debug(KBUILD_MODNAME ": " DBG_NAME " writing '%s'(bin) to CxpCameraDownscaleControl@0x%04x\n", to_bin_8(new_reg_value), self->camera_operator_downscale_register);
			self->ri->write(self->ri, self->camera_operator_downscale_register, new_reg_value);
		}
	}

	return DBG_TRACE_RETURN(STATUS_OK);
}

static int cxp_set_port_image_stream_id(struct cxp_frontend* self, uint32_t master_port, short stream_id) {
	DBG_TRACE_BEGIN_FCT;

	if (master_port >= self->num_ports) {
		pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set stream id; invalid physical port %d\n", master_port);
		return DBG_TRACE_RETURN(CXP_FRONTEND_ERROR_INVALID_PORT);
	}

	if (stream_id < -1 || stream_id > 255) {
		pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set stream id; invalid stream id%d\n", stream_id);
		return DBG_TRACE_RETURN(CXP_FRONTEND_ERROR_INVALID_PARAMETER);
	}

	uint64_t port_map = self->port_maps[self->port_map_index];
	uint32_t logical_port_number = cxp_get_logical_port_number_from_port_map(port_map, master_port);

	if (stream_id == -1) {
		// reset
		self->ports[logical_port_number].stream_id_cache = -1;
		return DBG_TRACE_RETURN(STATUS_OK);
	}

	if (!does_board_applet_support_tgs(self))
		return DBG_TRACE_RETURN(CXP_FRONTEND_ERROR_APPLETDOESNOTSUPPORTTGS);

	// check if cache values are same as input parameter values
	if ((stream_id == self->ports[logical_port_number].stream_id_cache)) {
		pr_debug(KBUILD_MODNAME ": " DBG_NAME " stream id is already set to this value \n");
		return DBG_TRACE_RETURN(STATUS_OK);
	}

	self->ports[logical_port_number].stream_id_cache = stream_id;

	/*
		ConfigImageStreamId_[0..3]
			32 Bit Register
			Bit [0:7]: ImageNummer -1 (0.255) 
			Bit [8:15]: StreamId  0..255
			Reserved [16-31] (must be 0) 
	*/
	// This is valid only for image number 0
	uint32_t config_image_stream_Id = ((0xff00) & ((stream_id & 0xff) << 8)) & 0x0000ffff;
	
	pr_debug(KBUILD_MODNAME ": " DBG_NAME " writing 0x%08x to register 0x%04x\n", config_image_stream_Id, self->ports[logical_port_number].config_image_stream_id_register);
	
	self->ri->write(self->ri, self->ports[logical_port_number].config_image_stream_id_register, config_image_stream_Id);
	
	return DBG_TRACE_RETURN(STATUS_OK);
}

static void cxp_print_port_map(const char* prefix, uint64_t port_map, uint32_t num_ports)
{
	switch (num_ports) {
	case 4:
		pr_debug(KBUILD_MODNAME ": " DBG_NAME " %s FG0 -> VA%d, FG1 -> VA%d, FG2 -> VA%d, FG3 -> VA%d\n", prefix,
		         cxp_get_logical_port_number_from_port_map(port_map, 0), cxp_get_logical_port_number_from_port_map(port_map, 1),
		         cxp_get_logical_port_number_from_port_map(port_map, 2), cxp_get_logical_port_number_from_port_map(port_map, 3));
		break;
	case 2:
		pr_debug(KBUILD_MODNAME ": " DBG_NAME " %s FG0 -> VA%d, FG1 -> VA%d\n", prefix, cxp_get_logical_port_number_from_port_map(port_map, 0),
		         cxp_get_logical_port_number_from_port_map(port_map, 1));
		break;
	case 1:
		pr_debug(KBUILD_MODNAME ": " DBG_NAME " %s FG0 -> VA%d\n", prefix, cxp_get_logical_port_number_from_port_map(port_map, 0));
		break;
	default:
		break;
	}
}

/**
 * Set the port map.
 * This will put ports to be remapped into reset, initiate the switch and restore the previous reset values
 */
static int cxp_set_port_map(struct cxp_frontend* self, uint64_t new_port_map)
{
	int ret           = 0;
	uint64_t old_port_map = self->port_maps[self->port_map_index];

	DBG_TRACE_BEGIN_FCT;

	if (new_port_map != old_port_map) {
		/* Look for valid port map entry */
		uint32_t new_port_map_index;
		for (new_port_map_index = 0; self->port_maps[new_port_map_index] != new_port_map && self->port_maps[new_port_map_index] != CXP_PORT_MAP_INVALID;
		     ++new_port_map_index) { /* NOP */
		}

		if (self->port_maps[new_port_map_index] == CXP_PORT_MAP_INVALID) {
			ret = CXP_FRONTEND_ERROR_INVALID_PARAMETER;
			pr_err(KBUILD_MODNAME ": " DBG_NAME " failed to set port map; invalid map 0x%016llx", new_port_map);
		} else {
			enum data_path_state old_data_path_state[CXP_MAX_NUM_PORTS];
			enum data_path_speed old_data_path_speed[CXP_MAX_NUM_PORTS];
			enum acquisition_state old_acquisition_state[CXP_MAX_NUM_PORTS];
			enum cxp_standard_version old_standard_version[CXP_MAX_NUM_PORTS];
			uint8_t old_camera_downscale_state[CXP_MAX_NUM_PORTS];

			DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " changing port map\n"));
			DBG1(cxp_print_port_map("old", old_port_map, self->num_ports));
			DBG1(cxp_print_port_map("new", new_port_map, self->num_ports));

			/* We have a valid mapping, now check which ports states should be permuted */
			if(old_port_map != CXP_PORT_MAP_INVALID){
				for (uint32_t port = 0; port < self->num_ports; ++port) {
					old_data_path_state[port] = self->ports[port].data_path_state_physical_cache;
					old_data_path_speed[port] = self->ports[port].data_path_dw_speed_cache;
				}

				/* For the parameters that have a logical cache, one looks what was written in the physical
				   port from the values in the cache of the logical port. */
				for (uint32_t logical_port = 0; logical_port < self->num_ports; ++logical_port) {
					uint32_t physical_port = cxp_get_physical_port_number_from_port_map(old_port_map, logical_port);
					old_standard_version[physical_port] = self->ports[logical_port].standard_version_cache;
					old_acquisition_state[physical_port] = self->ports[logical_port].acquisition_state_cache;
					old_camera_downscale_state[physical_port] = self->ports[logical_port].camera_downscale_state_cache;
				}
			}

			/* Set the port map index of the multiplexer*/
			self->port_map_index = new_port_map_index;

			/* Reprogram the multiplexer */
			DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " writing 0x%08x to register 0x%04x\n", new_port_map_index, self->discovery_config_register));
			self->ri->write(self->ri, self->discovery_config_register, new_port_map_index);

			/* Restore previous port reset values */
			if(old_port_map != CXP_PORT_MAP_INVALID){
				for (uint32_t physical_port = 0; physical_port < self->num_ports; ++physical_port) {
					const uint32_t old_logical_port = cxp_get_logical_port_number_from_port_map(old_port_map, physical_port);
					const uint32_t new_logical_port = cxp_get_logical_port_number_from_port_map(new_port_map, physical_port);

					if (old_logical_port != new_logical_port) {
						cxp_set_port_data_path_state(self, physical_port, old_data_path_state[physical_port]);
						cxp_set_port_data_path_speed(self, physical_port, old_data_path_speed[physical_port]);
						cxp_set_port_standard_version(self, physical_port, old_standard_version[physical_port]);
						cxp_set_port_acquisition_state(self, physical_port, old_acquisition_state[physical_port]);
						cxp_set_port_camera_donwscaling(self, physical_port, old_camera_downscale_state[physical_port]);
					}
				}
			}
		}
	}

	DBG_TRACE_END_FCT;

	return ret;
}

/**
 * Initialise port to default state.
 */
static void cxp_reset_port(struct camera_frontend* base, unsigned int physical_port)
{
	DBG_TRACE_BEGIN_FCT;

	struct cxp_frontend * self = downcast(base, struct cxp_frontend);

	if (physical_port < self->num_ports) {
		cxp_set_port_data_path_state(self, physical_port, DATA_PATH_STATE_INACTIVE);
		cxp_set_port_data_path_speed(self, physical_port, DATA_PATH_SPEED_3125);
		cxp_set_port_standard_version(self, physical_port, CXP_STANDARD_VERSION_1_1);
		cxp_set_port_led_state(self, physical_port, CXP_LED_STATE_POWERED);
		cxp_set_port_acquisition_state(self, physical_port, ACQUISITION_STATE_STOPPED);
		cxp_set_port_camera_donwscaling(self, physical_port, 1);
		cxp_set_port_image_stream_id(self, physical_port, -1);

		// the PoCXP state is left as is. It is active on power up and afterwards it remains in the state which the user specified.
	}

	DBG_TRACE_END_FCT;
}

/**
 * Initialise port to default state.
 */
static int cxp_reset(struct camera_frontend * base)
{
	DBG_TRACE_BEGIN_FCT;

	struct cxp_frontend * self = downcast(base, struct cxp_frontend);

	DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " resetting cxp frontend\n"));

	self->set_port_map(self, 0x7654321076543210ull);

	for (uint32_t port = 0; port < self->num_ports; ++port) {
		base->reset_physical_port(base, port);
	}

	return DBG_TRACE_RETURN(0);
}

/**
 * Prepare cxp frontend for applet reload.
 */
static int cxp_prepare_applet_reload(struct camera_frontend * base)
{
	DBG_TRACE_BEGIN_FCT;

	struct cxp_frontend * self = downcast(base, struct cxp_frontend);

	base->reset(base);

	if ((self->flags & CXP_FLAGS_SUPPORTS_IDLE_VIOLATION_FIX) != 0) {
		const uint32_t done = CXP_LOAD_APPLET_STATUS_DONE(self->num_ports);
		uint32_t status = 0;
		struct timeout timeout;

		/* Request Applet Reconfiguration */
		DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " requesting applet reload\n"));
		self->ri->write(self->ri, self->load_applet_ctrl_register, CXP_LOAD_APPLET_CTRL_REQUEST);
		timeout_init(&timeout, CXP_LOAD_APPLET_STATUS_TIMEOUT_IN_MS);

		self->ri->b2b_barrier(self->ri);

		do {
			status = self->ri->read(self->ri, self->load_applet_status_register);
		} while (status != done && !timeout_has_elapsed(&timeout));
		if (status != done) {
			pr_err(KBUILD_MODNAME ": " DBG_NAME " timed out while requesting applet reload\n");
		} else {
			DBG1(pr_debug(KBUILD_MODNAME ": " DBG_NAME " requesting applet reload succeeded\n"));
		}
	}

	return DBG_TRACE_RETURN(0);
}

static void cxp_frontend_destroy(struct camera_frontend * base) {
    DBG_TRACE_BEGIN_FCT;

    struct cxp_frontend * self = downcast(base, struct cxp_frontend);

    base->reset(base);

    if (self->ports != NULL)
        free_nonpageable_cacheable_small(self->ports, MEMORY_TAG_PORTS);

    free_nonpageable_cacheable_small(self, MEMORY_TAG_FRONTEND);

    DBG_TRACE_END_FCT;
}

static int cxp_execute_command(struct camera_frontend * base, enum camera_command cmd, union camera_control_input_args * args) {
    DBG_TRACE_BEGIN_FCT;

    struct cxp_frontend * self = downcast(base, struct cxp_frontend);

    int ret = STATUS_OK;

    switch(cmd) {

    case CAMERA_COMMAND_RESET:
        ret = base->reset(base);
        break;

    case CAMERA_COMMAND_SET_PORT_MAP:
        ret = self->set_port_map(self, args->set_port_map.port_map);
        break;

    case CAMERA_COMMAND_SET_PORT_POWER_STATE:
        ret = self->set_port_power_state(self, args->set_port_param.port, (power_state)args->set_port_param.param);
        break;

    case CAMERA_COMMAND_SET_PORT_DATA_PATH_STATE:
        ret = self->set_port_data_path_state(self, args->set_port_param.port, (data_path_state)args->set_port_param.param);
        break;

    case CAMERA_COMMAND_SET_PORT_DATA_PATH_SPEED:
        ret = self->set_port_data_path_speed(self, args->set_port_param.port, (data_path_speed)args->set_port_param.param);
        break;

    case CAMERA_COMMAND_SET_PORT_CXP_STANDARD_VERSION:
        ret = self->set_port_standard_version(self, args->set_port_param.port, (cxp_standard_version)args->set_port_param.param);
        break;

    case CAMERA_COMMAND_SET_PORT_CXP_LED_STATE:
        ret = self->set_port_led_state(self, args->set_port_param.port, (cxp_led_state)args->set_port_param.param);
        break;

    case CAMERA_COMMAND_SET_PORT_ACQUISITION_STATE:
        ret = self->set_port_acquisition_state(self, args->set_port_param.port, (acquisition_state)args->set_port_param.param);
        break;

    case CAMERA_COMMAND_SET_PORT_CXP_CAMERA_DOWNSCALING:
        ret = cxp_set_port_camera_donwscaling(self, args->set_port_param.port, args->set_port_param.param);
        break;

	case CAMERA_COMMAND_SET_STREAM_ID:
		ret = self->set_port_image_stream_id(self, args->set_stream_id.master_port, args->set_stream_id.stream_id);
		break;

    default:
        pr_err(KBUILD_MODNAME ": " DBG_NAME "Invalid camera command: %d\n", cmd);
        ret = STATUS_ERR_INVALID_OPERATION;
    }

    return DBG_TRACE_RETURN(ret);
}

/**
 * Initialise cxp port struct
 */
static void cxp_port_init(struct cxp_port* self, uint32_t physical_port_number)
{
	DBG_TRACE_BEGIN_FCT;

	self->downlink_bitrate_register = (physical_port_number < 4) ? REG_CXP_DOWNLINK_BITRATE_0 + physical_port_number : REG_CXP_DOWNLINK_BITRATE_4;
	self->led_ctrl_register         = (physical_port_number < 4) ? REG_CXP_LED_CTRL_0 + physical_port_number : REG_CXP_LED_CTRL_4;
	self->config_image_stream_id_register = REG_CONFIG_IMAGE_STREAM_ID_0 + physical_port_number;

	self->power_state_cache = POWER_STATE_UNKNOWN;
	self->data_path_state_physical_cache = DATA_PATH_STATE_UNKNOWN;
	self->data_path_state_logical_cache = DATA_PATH_STATE_UNKNOWN;
	self->data_path_dw_speed_cache = DATA_PATH_SPEED_UNKNOWN;
	self->data_path_up_speed_cache = DATA_PATH_UP_SPEED_UNKNOWN;
	self->standard_version_cache = CXP_STANDARD_VERSION_UNKNOWN;
	self->led_state_cache = CXP_LED_STATE_UNKNOWN;
	self->acquisition_state_cache = ACQUISITION_STATE_UNKNOWN;
	self->camera_downscale_state_cache = 1;
	self->stream_id_cache = -1;

	DBG_TRACE_END_FCT;
}

/**
 * Initialise cxp frontend struct
 */
static int cxp_frontend_init(struct cxp_frontend* self, struct register_interface* ri, unsigned int num_physical_ports, int supports_idle_violation_fix)
{
	self->ports = (struct cxp_port*)alloc_nonpageable_cacheable_small_zeros(num_physical_ports * sizeof(*self->ports), MEMORY_TAG_PORTS);
    if (self->ports == NULL) {
        pr_err(KBUILD_MODNAME ": " DBG_NAME " Failed to alloc memory for ports.");
        return STATUS_ERR_INSUFFICIENT_MEM;
    }

	/* Base Constructor */
    int status = camera_frontend_init(upcast(self), num_physical_ports, cxp_reset_port, cxp_reset, cxp_prepare_applet_reload, cxp_frontend_destroy, cxp_execute_command);
    if (status != STATUS_OK) {
        free_nonpageable_cacheable_small(self->ports, MEMORY_TAG_PORTS);
        self->ports = NULL;
        return status;
    }

	self->ri = ri;

	switch (num_physical_ports) {
	case 5:
		self->port_maps      = cxp_port_maps_5ch;
		self->port_map_index = CXP_NUM_PORT_MAP_ENTRIES_5C - 1;
		self->num_ports      = 5;
		break;		
	case 4:
		self->port_maps      = cxp_port_maps_4ch;
		self->port_map_index = CXP_NUM_PORT_MAP_ENTRIES_4C - 1;
		self->num_ports      = 4;
		break;
	case 2:
		self->port_maps      = cxp_port_maps_2ch;
		self->port_map_index = CXP_NUM_PORT_MAP_ENTRIES_2C - 1;
		self->num_ports      = 2;
		break;
	default:
		self->port_maps      = cxp_port_maps_1ch;
		self->port_map_index = CXP_NUM_PORT_MAP_ENTRIES_1C - 1;
		self->num_ports      = 1;
		break;
	}

	for (uint32_t port = 0; port < self->num_ports; ++port) {
		cxp_port_init(&self->ports[port], port);
	}

	self->power_ctrl_register			= REG_CXP_POWER_CTRL;
	self->reset_ctrl_register			= REG_CXP_RESET_CTRL;
	self->standard_ctrl_register		= REG_CXP_STANDARD_CTRL;
	self->acquisition_status_register	= REG_ACQUISITION_CTRL;
	self->discovery_config_register		= REG_CXP_DISCOVERY_CONFIG;

	self->data_path_status_register            = REG_CXP_DATA_PATH_STATUS;
	self->data_path_speed_change_timeout_msecs = CXP_DATA_PATH_STATUS_READY_TIMEOUT_MSECS;

	self->load_applet_ctrl_register = REG_CXP_LOAD_APPLET_CTRL;
	self->load_applet_status_register = REG_CXP_LOAD_APPLET_STATUS;
	if (supports_idle_violation_fix) {
		self->flags |= CXP_FLAGS_SUPPORTS_IDLE_VIOLATION_FIX;
	}

    self->camera_operator_downscale_register = REG_CXP_CAMERA_DOWNSCALE_CTRL;

	self->set_port_map               = cxp_set_port_map;
	self->set_port_power_state       = cxp_set_port_power_state;
	self->set_port_data_path_state   = cxp_set_port_data_path_state;
	self->set_port_data_path_speed   = cxp_set_port_data_path_speed;
	self->set_port_standard_version  = cxp_set_port_standard_version;
	self->set_port_led_state         = cxp_set_port_led_state;
	self->set_port_acquisition_state = cxp_set_port_acquisition_state;
	self->set_port_image_stream_id	 = cxp_set_port_image_stream_id;

	return STATUS_OK;
}

struct cxp_frontend * cxp_frontend_alloc_and_init(struct register_interface* ri, unsigned int num_ports, int supports_idle_violation_fix) {
    struct cxp_frontend * frontend = (struct cxp_frontend*)alloc_nonpageable_cacheable_small_zeros(sizeof(struct cxp_frontend), MEMORY_TAG_FRONTEND);

    if (frontend == NULL)
        return NULL;

    int ret = cxp_frontend_init(frontend, ri, num_ports, supports_idle_violation_fix);
    if (ret != STATUS_OK) {
        cxp_frontend_destroy(upcast(frontend));
        return NULL;
    }

    return frontend;
}
