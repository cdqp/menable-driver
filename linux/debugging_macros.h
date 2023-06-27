/************************************************************************
 * Copyright 2006-2021 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef LINUX_DEBUGGING_MACROS_H_
#define LINUX_DEBUGGING_MACROS_H_

#include <linux/device.h>
#include <linux/printk.h>

#define DEV_INFO_IMPL(prfx, dev, msg, ... ) dev_printk(KERN_INFO,  dev, pr_fmt("["prfx"] " msg), ##__VA_ARGS__)
#define DEV_WARN_IMPL(prfx, dev, msg, ... )  dev_printk(KERN_WARNING,   dev, pr_fmt("["prfx"] " msg), ##__VA_ARGS__)
#define DEV_ERR_IMPL(prfx, dev, msg, ... )  dev_printk(KERN_ERR,   dev, pr_fmt("["prfx"] " msg), ##__VA_ARGS__)
#define DEV_DBG_IMPL(prfx, dev, msg, ... )  dev_printk(KERN_DEBUG, dev, pr_fmt("["prfx"] " msg), ##__VA_ARGS__)

#define PR_DBG_IMPL(prfx, dev, msg, ... )   printk(KERN_DEBUG, pr_fmt("["prfx"] " msg), ##__VA_ARGS__)

#if defined(DBG_MESSAGING_DMA) && !defined(DBG_MESSAGING_DMA_OFF)
    #define DEV_DBG_MESSAGING_DMA(dev, msg, ...) DEV_DBG_IMPL("MSG DMA", dev, msg, ##__VA_ARGS__)
#else
    #define DEV_DBG_MESSAGING_DMA(msg, ...)
#endif

#if defined(DBG_ACQ) && !defined(DBG_ACQ_OFF)
    #define DEV_DBG_ACQ(dev, msg, ...) DEV_DBG_IMPL("ACQ", dev, msg, ##__VA_ARGS__)
    #define PR_DBG_ACQ(msg, ...) PR_DBG_IMPL("ACQ", msg, ##__VA_ARGS__)
#else
    #define DEV_DBG_ACQ(dev, msg, ...)
    #define PR_DBG_ACQ(msg, ...)
#endif
#define DEV_WARN_ACQ(dev, msg, ...) DEV_WARN_IMPL("ACQ", dev, msg, ##__VA_ARGS__)

#if defined(DBG_UIQ) && !defined(DBG_UIQ_OFF)
    #define DEV_DBG_UIQ(dev, msg, ...) DEV_DBG_IMPL("UIQ", dev, msg, ##__VA_ARGS__)
    #define PR_DBG_UIQ(msg, ...) PR_DBG_IMPL("UIQ", msg, ##__VA_ARGS__)
#else
    #define DEV_DBG_UIQ(dev, msg, ...)
    #define PR_DBG_UIQ(msg, ...)
#endif

#if defined(DBG_CXP)
    #define DEV_DBG_CXP(dev, msg, ...) DEV_DBG_IMPL("CXP", dev, msg, ##__VA_ARGS__)
    #define PR_DBG_CXP(msg, ...) PR_DBG_IMPL("CXP", msg, ##__VA_ARGS__)
#else
    #define DEV_DBG_CXP(dev, msg, ...)
    #define PR_DBG_CXP(msg, ...)
#endif

#if defined(DEBUG_IRQS)
    #define DEV_DBG_IRQ(dev, msg, ...) DEV_DBG_IMPL("IRQ", dev, msg, ##__VA_ARGS__)
#else
    #define DEV_DBG_IRQ(dev, msg, ...)
#endif

#if defined(DBG_IOCTL) && !defined(DBG_IOCTL_OFF)
    #define DEV_DBG_IOCTL(dev, msg, ...) DEV_DBG_IMPL("IOCTL", dev, msg, ##__VA_ARGS__)
#else
    #define DEV_DBG_IOCTL(msg, ...)
#endif

#if defined(DBG_BUFS) && !defined(DBG_BUFFERS_OFF)
    #define DEV_DBG_BUFS(dev, msg, ...) DEV_DBG_IMPL("BUFS", dev, msg, ##__VA_ARGS__)
#else
    #define DEV_DBG_BUFS(msg, ...)
#endif

#define DEV_ERR_BUFS(dev, msg, ...) DEV_ERR_IMPL("BUFS", dev, msg, ##__VA_ARGS__)

/* A macro for debugging spinlocks.
 * Since it is not clear, who is holding the lock, the assertion may not fail
 * when you expect it to do so. But to find a locking bug, it would be sufficient,
 * if the assertion fails once in a while.
 */
#if defined(CONFIG_SMP) && defined(DBG_LOCKING)
#define ASSERT_SPINLOCK_HELD(lock) BUG_ON(!spin_is_locked(lock))
#else
/* on uniprocessor machines, spinlocks are never really locked, so the check is useless here. */
#define ASSERT_SPINLOCK_HELD(lock) do {} while (0)
#endif

#endif /* LINUX_DEBUGGING_MACROS_H_ */
