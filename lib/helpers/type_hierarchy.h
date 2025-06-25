/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef TYPE_HIERARCHY_H_
#define TYPE_HIERARCHY_H_

#include "../os/kernel_macros.h"

#define BASETYPE_MEMBER_NAME base_type
#define DERIVE_FROM(basetype) struct basetype BASETYPE_MEMBER_NAME
#define downcast(base_instance, derived_type) (container_of((base_instance), derived_type, BASETYPE_MEMBER_NAME))
#define upcast(derived_ptr) (&(derived_ptr)->BASETYPE_MEMBER_NAME)


#endif /* TYPE_HIERARCHY_H_ */
