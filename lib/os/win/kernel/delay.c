/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include <lib/os/win/delay.h>
#include <lib/helpers/helper.h>
#include <wdm.h>
#include "types.h"

#define NANOS_TO_SYSTEM_TIME_UNITS(nanos) CEIL_DIV(nanos, 100)

static NTSTATUS sleepAtLeast(ULONG nsecs, KPROCESSOR_MODE waitMode, BOOLEAN alertable)
{
    ULONG timeUnitsPerTick = KeQueryTimeIncrement();
    ULONG waitTimeUnits = NANOS_TO_SYSTEM_TIME_UNITS(nsecs);
    
    LARGE_INTEGER startTicks;
    KeQueryTickCount(&startTicks);
    
    LARGE_INTEGER endTicks;
    endTicks.QuadPart = startTicks.QuadPart + CEIL_DIV(waitTimeUnits, timeUnitsPerTick);

    NTSTATUS result = STATUS_SUCCESS;

    LARGE_INTEGER currentTicks;
    currentTicks = startTicks;
    while (result == STATUS_SUCCESS && currentTicks.QuadPart < endTicks.QuadPart) {
        LARGE_INTEGER waitTime;
        waitTime.QuadPart = -(endTicks.QuadPart - currentTicks.QuadPart) * timeUnitsPerTick;
        result = KeDelayExecutionThread(waitMode, alertable, &waitTime);
        
        KeQueryTickCount(&currentTicks);
    };

    return result;
}


void udelay(unsigned long usecs) {
    KeStallExecutionProcessor(usecs);
}
