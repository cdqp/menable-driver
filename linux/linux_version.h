/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2025 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 *
 * This adds some functions of newer kernels for older versions so
 * no #ifdef fiddling is required in-source.
 */

#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/scatterlist.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/stat.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))

static inline void
sg_init_table(struct scatterlist *sg, unsigned int len)
{
}

static inline void
sg_set_page(struct scatterlist *sg, struct page *page,
		unsigned int len, unsigned int offset)
{
	sg->page = page;
	sg->offset = offset;
	sg->length = len;
}

static inline struct page *
sg_page(struct scatterlist *sg)
{
	return sg->page;
}
#endif /* LINUX < 2.6.24 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 25)

#define DEFINE_PCI_DEVICE_TABLE(_table) \
	struct pci_device_id _table[] __devinitdata

extern int strict_strtoul(const char *cp, unsigned int base, unsigned long *res);

/* taken from include/linux/sched.h from kernel 2.6.32 */

static inline int __fatal_signal_pending(struct task_struct *p)
{
	return unlikely(sigismember(&p->pending.signal, SIGKILL));
}

static inline int fatal_signal_pending(struct task_struct *p)
{
	return signal_pending(p) && __fatal_signal_pending(p);
}

#endif /* LINUX < 2.6.25 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)

extern int dev_set_name(struct device *dev, const char *fmt, ...);

extern const char *dev_name(const struct device *dev);

#endif /* LINUX < 2.6.26 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
#define ktime_get_ts(ts) do_posix_clock_monotonic_gettime(ts)
#else
#include <linux/ktime.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)

static inline void dev_set_uevent_suppress(struct device *dev, int val)
{
	dev->uevent_suppress = val;
}

#endif /* LINUX < 2.6.30 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)

#define __devinit
#define __devinitdata
#define __devexit
#define __devexit_p(x) x

#endif /* LINUX >= 3.8.0 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)

#define DEFINE_PCI_DEVICE_TABLE(_table) \
struct pci_device_id _table[] __devinitdata

#endif /* LINUX >= 4.8.0 */

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)) || (!defined(RHEL7) && (LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0))))
static inline void reinit_completion(struct completion *x)
{
    INIT_COMPLETION(*x);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)

static inline void vm_flags_set(struct vm_area_struct *vma,
	vm_flags_t flags)
{
	vma->vm_flags |= flags;
}

#endif /* LINUX < 6.3.0 */

#define to_delayed_work(_work)  container_of(_work, struct delayed_work, work)
