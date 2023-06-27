/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef MENABLE_UAPI_H_
#define MENABLE_UAPI_H_

/* TODO [Documentation]
 *      Add some comments on
 *       - what each state means/implies
 *       - when which state is entered/left
 *       - what the ordering of the values means (i.e. >= read --> firmware is accessible)
 */
enum men_board_state {
    BOARD_STATE_UNKNOWN,
    BOARD_STATE_UNINITIALISED,
    BOARD_STATE_DEAD,
    BOARD_STATE_SLEEPING,
    BOARD_STATE_RECONFIGURING,
    BOARD_STATE_READY,
    BOARD_STATE_CONFIGURED,
    BOARD_STATE_ACTIVE,
};


#endif /* RT_ME6_DRIVER_LINUX_UAPI_H_ */
