/************************************************************************
* Copyright 2006-2020 Silicon Software GmbH, 2021-2022 Basler AG
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License (version 2) as
* published by the Free Software Foundation.
*/

#include "menable.h"

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/vmalloc.h>

#include "menable_ioctl.h"
#include "menable6.h"
#include "sisoboards.h"
#include "lib/ioctl_interface/transaction.h"
#include "lib/ioctl_interface/camera.h"
#include "lib/controllers/controller_base.h"

#ifdef DBG_IOCTL
    #undef DEBUG
    #define DEBUG

    #undef DBG_LEVEL
    #define DBG_LEVEL DBG_IOCTL

    #define DBG_NAME "[ioctl] "
#endif
#include "lib/helpers/dbg.h"
#include "debugging_macros.h"

/**
 * Macro to check the size of the user buffer and copy it into a local variable
 * if it fits.
 *
 * @param men        The menable struct to work on (used for logging only)
 * @param ioctl_code The ioctl code
 * @param target_var The name of the variable to which the buffer should be copied.
 */
#define CHECK_AND_COPY_INPUT_BUFFER(men, cmd, user_buf_addr, target_var) do { \
        if (unlikely(_IOC_SIZE((cmd)) != sizeof((target_var)))) { \
            warn_wrong_iosize((men), (cmd), sizeof((target_var))); \
            return -EINVAL; \
        } \
        \
        if (copy_from_user((&target_var), (void __user *) (user_buf_addr), sizeof((target_var))) != 0) { \
            return -EFAULT; \
        } \
    } while (0)

static int
process_transaction(struct siso_menable * men, struct transaction_header * th) {
    struct controller_base * controller = NULL;
    
    DBG_TRACE_BEGIN_FCT;

    /* Accessing DUMMY_PERIPHERAL_ID is a probing call to see, whether or not
     * transactions are supported. Simply acknowledge without further processing! */
    if (th->peripheral == DUMMY_PERIPHERAL_ID) {
        return DBG_TRACE_RETURN(0);
    }

    if (!men->get_controller) {
        dev_err(&men->dev, "Transaction failed: device does not support API\n");
        return DBG_TRACE_RETURN(-ENODEV);
    }

    controller = men->get_controller(men, th->peripheral);
    if (!controller) {
        return DBG_TRACE_RETURN(-EINVAL);
    }

    dev_dbg(&men->dev, "num_bursts: %u\n", th->num_bursts);

    mutex_lock(controller->lock);

    int ret = controller->begin_transaction(controller);
    if (ret != 0) {
        mutex_unlock(controller->lock);

        return DBG_TRACE_RETURN(-EFAULT);
	}

    for (int i = 0; i < th->num_bursts; ++i) {
        struct burst_header bh;
        struct burst_header __user * burst_headers_userbuf = (struct burst_header __user *)th->burst_headers_address;
        copy_from_user(&bh, &burst_headers_userbuf[i], sizeof(bh));

        dev_dbg(&men->dev, "burst %u - type: %u, len: %u, flags: 0x%08x\n",
                i, bh.type, bh.len, bh.flags);

        if (bh.len <= 0 && bh.type != BURST_TYPE_STATE_CHANGE) {
            dev_warn(&men->dev, "Invalid buffer length %u for burst number %d\n",
                     bh.len, i);
            continue;
        }

        uint8_t * buffer = NULL;

        int ret;
        switch(bh.type) {

        case BURST_TYPE_WRITE:
            buffer = vmalloc(bh.len);
            ret = copy_from_user(buffer, (void __user *)bh.buffer_address, bh.len);
            if (ret != 0) goto error;

            ret = controller->write_burst(controller, &bh, buffer, bh.len);
            if (ret != 0) goto error;

            break;

        case BURST_TYPE_READ:
            buffer = vmalloc(bh.len);
            ret = controller->read_burst(controller, &bh, buffer, bh.len);
            if (ret != 0) goto error;

            ret = copy_to_user((void __user *)bh.buffer_address, buffer, bh.len);
            if (ret != 0) goto error;

            break;

        case BURST_TYPE_STATE_CHANGE:
            /* TODO handle_post_burst_flags should be thought of as a
             *      private or protected method. Implement a
             *      state_change_burst(...) method for the controller. */
            ret = controller->handle_post_burst_flags(controller, bh.flags);
            if (ret != 0) goto error;

            break;

        case BURST_TYPE_COMMAND: {
            buffer = vmalloc(bh.len);
            if (!buffer)
                goto error;

            ret = copy_from_user(buffer, (void __user*)bh.buffer_address, bh.len);
            if (ret != 0)
                goto error;

            ret = controller->command_execution_burst(controller, &bh, buffer, bh.len);
            if (ret != 0)
                goto error;

        } break;

        }

        if (buffer)
            vfree(buffer);

        continue;

    error:
        if (buffer)
            vfree(buffer);

        mutex_unlock(controller->lock);

        DBG_TRACE_END_FCT;
        return -EFAULT;
    }

	controller->end_transaction(controller);
	
    mutex_unlock(controller->lock);

    DBG_TRACE_END_FCT;
    return 0;
}

static int
do_camera_control(struct siso_menable * men, void __user * user_io_buffer) {

    int ret = 0;

    mutex_lock(&men->camera_frontend_lock);

    if (men->camera_frontend == NULL) {
        dev_err(&men->dev, "No camera frontend available.");
        ret = -EFAULT;
    } else {

        union camera_control_io ctrl_io;
        if (copy_from_user(&ctrl_io, user_io_buffer, sizeof(ctrl_io)) != 0) {
            dev_err(&men->dev, "Failed to copy input buffer from user.");
            ret = -EFAULT;
        } else if (ctrl_io.in.command == CAMERA_COMMAND_GET_VERSION) {
            ctrl_io.out._size = sizeof(ctrl_io.out);
            ctrl_io.out._version = CAMERA_FRONTEND_VERSION;
            copy_to_user(user_io_buffer, &ctrl_io, sizeof(ctrl_io));
        } else {
            int status = men->camera_frontend->execute_command(men->camera_frontend, ctrl_io.in.command, &ctrl_io.in.args);
            if (status != STATUS_OK) {
                ret = -EFAULT;
            }
        }
    }
    mutex_unlock(&men->camera_frontend_lock);

    return ret;
}

/**
* warn_wrong_iosize - print warning about bad ioctl argument
* @men device that happened on
* @cmd ioctl command sent in
* @expsize size expected by this ioctl
*
* Basically this should never happen: all ioctls are sent in by the
* Silicon Software runtime environment and should do this right. This
* is basically to make internal debugging easier when someone
* accidentially messes up some struct layout.
*/
void
warn_wrong_iosize(struct siso_menable *men, unsigned int cmd,
                  const size_t expsize)
{
    // TODO: [RKN] Is this not an error? It is not supposed to happen and if it does, we cannot execute the ioctl...
    dev_dbg(&men->dev,
        "ioctl %i called with buffer length %i but expected %zi\n",
        _IOC_NR(cmd), _IOC_SIZE(cmd), expsize);
}

static long
men_ioctl_fgstart(struct siso_menable *men, unsigned int cmd, unsigned long arg)
{
    struct fg_ctrl fg;
    struct fg_ctrl_s fgr;

    if (unlikely(_IOC_SIZE(cmd) != sizeof(fgr))) {
        warn_wrong_iosize(men, cmd, sizeof(fgr));
        return -EINVAL;
    }

    if (copy_from_user(&fgr, (void __user *)arg, sizeof(fgr)))
        return -EFAULT;

    fg.mode = fgr.u.fg_start.mode;
    fg.timeout = fgr.u.fg_start.timeout;
    fg.transfer_todo = fgr.u.fg_start.transfer_todo;
    fg.chan = fgr.u.fg_start.chan;
    fg.head = fgr.u.fg_start.head;
    fg.start_buf = fgr.u.fg_start.start_buf;
    fg.dma_dir = fgr.u.fg_start.dma_dir;

    return fg_start_transfer(men, &fg, fgr.u.fg_start.act_size);
}

static long
men_ioctl_unlock_buffer(struct siso_menable *men, const unsigned int head,
                        const long index)
{
    struct menable_dmahead *dh;
    struct menable_dmabuf *sb;
    struct menable_dmachan *dc;
    int ret;
    unsigned long flags;

    if (unlikely(index < -1))
        return -EINVAL;

    dh = me_get_buf_head(men, head);

    if (dh == NULL) {
        DEV_ERR_BUFS(&men->dev, "Attempt to unlock a buffer for an invalid Memory Head.\n");
        return -EINVAL;
    }

    if (unlikely(index >= dh->num_sb)) {
        spin_unlock_bh(&men->buffer_heads_lock);
        dev_err(&men->dev, "Attempt to unlock a buffer with index %ld, but only %ld buffers exist.\n", index, dh->num_sb);
        return -EINVAL;
    }

    dc = dh->chan;
    if (unlikely(dc == NULL)) {
        /* The buffer does not paritcipate in a running acquisition.
         * This may happen if buffers are unlocked after the aquisition was stopped */
        spin_unlock_bh(&men->buffer_heads_lock);
        return 0;
    }

    spin_lock_irqsave(&dc->listlock, flags);
    if (index == -1) {
        long i;

        DEV_DBG_BUFS(&men->dev, "Unblocking all buffers.\n");
        for (i = 0; i < dh->num_sb; i++) {
            sb = dh->bufs[i];
            men_unblock_buffer(dc, sb);
        }
        BUG_ON(dc->grabbed_count);
        BUG_ON(dc->locked_count);
        ret = 0;
    } else {
        sb = dh->bufs[index];
        if (!sb) {
            DEV_ERR_BUFS(&men->dev, "Failed to unblock buffer %ld. No buffer found.\n", index);
            ret = -EINVAL;
        } else {
            DEV_DBG_BUFS(&men->dev, "Unblocking buffers %ld.\n", index);
            men_unblock_buffer(dc, sb);
            ret = 0;
        }
    }
    spin_unlock_irqrestore(&dc->listlock, flags);
    spin_unlock_bh(&men->buffer_heads_lock);
    return ret;
}

static const char* get_ioctl_name(unsigned int ioctl_code) {
	switch(ioctl_code) {
	case IOCTL_ADD_VIRT_USER_BUFFER: return "IOCTL_ADD_VIRT_USER_BUFFER";
	case IOCTL_ALLOCATE_VIRT_BUFFER: return "IOCTL_ALLOCATE_VIRT_BUFFER";
	case IOCTL_BOARD_INFO: return "IOCTL_BOARD_INFO";
	case IOCTL_DEL_VIRT_USER_BUFFER: return "IOCTL_DEL_VIRT_USER_BUFFER";
	case IOCTL_DESIGN_SETTINGS: return "IOCTL_DESIGN_SETTINGS";
	case IOCTL_DMA_LENGTH: return "IOCTL_DMA_LENGTH";
	case IOCTL_DMA_TAG: return "IOCTL_DMA_TAG";
	case IOCTL_DMA_FRAME_NUMBER: return "IOCTL_DMA_FRAME_NUMBER";
	case IOCTL_DMA_TIME_STAMP: return "IOCTL_DMA_TIME_STAMP";
	case IOCTL_EX_CAMERA_CONTROL: return "IOCTL_EX_CAMERA_CONTROL";
	case IOCTL_EX_CONFIGURE_FPGA: return "IOCTL_EX_CONFIGURE_FPGA";
	case IOCTL_EX_DATA_TRANSFER: return "IOCTL_EX_DATA_TRANSFER";
	case IOCTL_EX_DEVICE_CONTROL: return "IOCTL_EX_DEVICE_CONTROL";
	case IOCTL_EX_GET_DEVICE_STATUS: return "IOCTL_EX_GET_DEVICE_STATUS";
	case IOCTL_EX_GET_INTERFACE_VERSION: return "IOCTL_EX_GET_INTERFACE_VERSION";
	case IOCTL_FG_START_TRANSFER: return "IOCTL_FG_START_TRANSFER";
	case IOCTL_FG_STOP_CMD: return "IOCTL_FG_STOP_CMD";
	case IOCTL_FG_TEST_BUF_STATUS: return "IOCTL_FG_TEST_BUF_STATUS";
	case IOCTL_FG_WAIT_FOR_SUBBUF: return "IOCTL_FG_WAIT_FOR_SUBBUF";
	case IOCTL_FREE_VIRT_BUFFER: return "IOCTL_FREE_VIRT_BUFFER";
	case IOCTL_GET_BUFFER_STATUS: return "IOCTL_GET_BUFFER_STATUS";
	case IOCTL_GET_EEPROM_DATA: return "IOCTL_GET_EEPROM_DATA";
	case IOCTL_GET_HANDSHAKE_DMA_BUFFER: return "IOCTL_GET_HANDSHAKE_DMA_BUFFER";
	case IOCTL_GET_VERSION: return "IOCTL_GET_VERSION";
	case IOCTL_MAPMEM_MAP_USER_PHYSICAL_MEMORY: return "IOCTL_MAPMEM_MAP_USER_PHYSICAL_MEMORY";
	case IOCTL_MAPMEM_UNMAP_USER_PHYSICAL_MEMORY: return "IOCTL_MAPMEM_UNMAP_USER_PHYSICAL_MEMORY";
	case IOCTL_MAP_PCI_BAR: return "IOCTL_MAP_PCI_BAR";
	case IOCTL_PP_CONTROL: return "IOCTL_PP_CONTROL";
	case IOCTL_RESOURCE_NAME_READ: return "IOCTL_RESOURCE_NAME_READ";
	case IOCTL_RESOURCE_NAME_WRITE: return "IOCTL_RESOURCE_NAME_WRITE";
	case IOCTL_RESSOURCE_CONTROL: return "IOCTL_RESSOURCE_CONTROL";
	case IOCTL_SET_EEPROM_DATA: return "IOCTL_SET_EEPROM_DATA";
	case IOCTL_UIQ_FLUSH: return "IOCTL_UIQ_FLUSH";
	case IOCTL_UIQ_POLL: return "IOCTL_UIQ_POLL";
	case IOCTL_UIQ_SCALE: return "IOCTL_UIQ_SCALE";
	case IOCTL_UIQ_WRITE: return "IOCTL_UIQ_WRITE";
	case IOCTL_UNLOCK_BUFFER_NR: return "IOCTL_UNLOCK_BUFFER_NR";
	}
	return "UNKNOWN IOCTL CODE";
}

static bool men_is_ioctl_allowed(enum men_board_state state, unsigned int ioctl_code) {
	/*
	 * When configured, all ioctl codes are allowed.
	 * Not that this does not necessarily mean that they are actually supported
	 * by the board.
	 */
	if (state >= BOARD_STATE_READY)
		return true;

	/* in unconfigured state, only some ioctl codes are allowed */
	switch(ioctl_code) {
	case IOCTL_EX_GET_INTERFACE_VERSION:
	case IOCTL_EX_GET_DEVICE_STATUS:
	case IOCTL_EX_CONFIGURE_FPGA:
	case IOCTL_PP_CONTROL:
		return true;
	default:
		return false;
	}
}

static const char * get_buffer_list_name(int list_id) {
    switch(list_id) {
    case FREE_LIST: return "FREE";
    case GRABBED_LIST: return "GRABBED";
    case HOT_LIST: return "HOT";
    case NO_LIST: return "NONE";
    case READY_LIST: return "READY";
    default: return "UNKNOWN";
    }
}

static int men_queue_buffer(struct siso_menable *men, unsigned int head_idx, unsigned int buf_idx) {
    struct menable_dmahead * dma_head = me_get_buf_head(men, head_idx);

    if (unlikely(dma_head == NULL)) {
        dev_err(&men->dev, "Attempt to queue buffer for invalid dma index %d.\n", head_idx);
        goto err_no_locks;
    }

    if (unlikely(buf_idx >= dma_head->num_sb)) {
        dev_err(&men->dev, "Attempt to queue a buffer with invalid index %d.\n", buf_idx);
        goto err_bufheads_locked;
    }

    struct menable_dmabuf * buf = me_get_sub_buf_by_head(dma_head, buf_idx);
    if (unlikely(buf == NULL)) {
        dev_err(&men->dev, "Attempt to queue an non-existing buffer.\n");
        goto err_bufheads_locked;
    }

    if (buf->listname != FREE_LIST) {
        dev_warn(&men->dev, "Attempt to queue buffer that is in %s queue.\n", get_buffer_list_name(buf->listname));
        goto err_bufheads_locked;
    }

    struct menable_dmachan * dma_chan = dma_head->chan;
    if (dma_chan == NULL) {
        /* No active acquisition, just mark buffers as being queued */
        buf->listname = READY_LIST;
    } else if (dma_chan->mode != DMA_SELECTIVEMODE) {
        /* Acquisition running in wrong mode */
        dev_err(&men->dev, "Attempt to queue a buffer during active non selective mode acquisition.\n");
        goto err_bufheads_locked;
    } else {
        /* Active selective mode acquisition -> move buffer to READY list */
        unsigned long lock_flags;
        spin_lock_irqsave(&dma_chan->listlock, lock_flags);

        /* move buffer to ready list */
        list_move_tail(&buf->node, &dma_chan->ready_list);
        buf->listname = READY_LIST;
        dma_chan->free_count -= 1;
        dma_chan->ready_count += 1;

        /*
         * Just in case the DMA Fifo has run empty, we queue as
         * many buffers as possible / useful.
         */
        men_dma_queue_max(dma_chan);

        // TODO: [EXPLAIN] Why one time _irqsave and one time _bh?
        spin_unlock_irqrestore(&dma_chan->listlock, lock_flags);
    }

    spin_unlock_bh(&men->buffer_heads_lock);

    return 0;

err_bufheads_locked:
    spin_unlock_bh(&men->buffer_heads_lock);

err_no_locks:
    return -EINVAL;
}

static long men_ioctl_allocate_virt_buffer(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct mm_create_s mm_cmd;

    if (_IOC_SIZE(cmd) != sizeof(mm_cmd)) {
        warn_wrong_iosize(men, cmd, sizeof(mm_cmd));
        return -EINVAL;
    }

    if (copy_from_user(&mm_cmd,
        (void __user *) arg,
        sizeof(mm_cmd)))
        return -EFAULT;

#if BITS_PER_LONG < 64
    if (mm_cmd.maxsize > 0xffffffffULL)
        return -EINVAL;
#endif

    return men_create_buf_head(men, mm_cmd.maxsize, mm_cmd.subbufs);
}

static long men_ioctl_add_virt_user_buffer(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct men_io_range range;

    if (_IOC_SIZE(cmd) != sizeof(range)) {
        warn_wrong_iosize(men, cmd, sizeof(range));
        return -EINVAL;
    }

    if (copy_from_user(&range,
        (void __user *) arg,
        sizeof(range)))
        return -EFAULT;
    if (range.length == 0)
        return -EFAULT;
    return men_create_userbuf(men, &range);
}

static long men_ioctl_del_virt_user_buffer(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct men_io_bufidx num;
    struct menable_dmahead *buf_head;
    int r;

    if (_IOC_SIZE(cmd) != sizeof(num)) {
        warn_wrong_iosize(men, cmd, sizeof(num));
        return -EINVAL;
    }

    if (copy_from_user(&num,
        (void __user *) arg,
        sizeof(num)))
        return -EFAULT;

    buf_head = me_get_buf_head(men, num.headnr);

    if (buf_head == NULL) {
        r = -EINVAL;
    } else {
        struct menable_dmabuf *sub_buf = me_get_sub_buf_by_head(buf_head, num.index);
        r = men_free_userbuf(men, buf_head, num.index);
        spin_unlock_bh(&men->buffer_heads_lock);
        if (r == 0)
            men_destroy_sb(men, sub_buf);
        // TODO: Else?
    }
    return r;
}

static long men_ioctl_free_virt_buffer(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct menable_dmahead *bh;
    int ret;

    if (_IOC_SIZE(cmd) != 0) {
        warn_wrong_iosize(men, cmd, 0);
        return -EINVAL;
    }

    bh = me_get_buf_head(men, (unsigned int) arg);
    if (bh == NULL)
        return 0;
    ret = men_release_buf_head(men, bh);
    spin_unlock_bh(&men->buffer_heads_lock);
    if (ret == 0)
        men_free_buf_head(men, bh);
    return ret;
}

static long men_ioctl_dma_time_stamp(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct dma_timestamp ts;
    struct menable_dmabuf *sb;
    menable_ioctl_timespec_t tmp;
    int ret;

    if (unlikely(_IOC_SIZE(cmd) != sizeof(ts))) {
        warn_wrong_iosize(men, cmd, sizeof(ts));
        return -EINVAL;
    }

    if (copy_from_user(&ts, (void __user *) arg, sizeof(ts)))
        return -EFAULT;

    sb = me_get_sub_buf(men, ts.head, ts.buf);
    if (unlikely(sb == NULL))
        return -EINVAL;
    tmp.tv_sec = sb->timestamp.tv_sec & 0x7fffffffUL;
	tmp.tv_nsec = sb->timestamp.tv_nsec;
    spin_unlock_bh(&men->buffer_heads_lock);

    ret = copy_to_user(((void __user *) arg) +
        offsetof(typeof(ts), stamp),
        &tmp, sizeof(tmp));
    return ret ? -EFAULT : 0;
}

static long men_ioctl_fg_wait_for_subbuf(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct men_io_bufwait ctrl;
    struct menable_dmachan *db;
    uint64_t foundframe;
    int ret;

    if (unlikely(_IOC_SIZE(cmd) != sizeof(ctrl))) {
        warn_wrong_iosize(men, cmd, sizeof(ctrl));
        return -EINVAL;
    }

    if (copy_from_user(&ctrl, (void __user *) arg, sizeof(ctrl))) {
        return -EFAULT;
    }

    // Don't allow ridiculously high negative numbers
    if (unlikely(ctrl.index < INT_MIN / 2)) {
        return -EINVAL;
    }

    db = men_dma_channel(men, ctrl.dmachan);
    if (unlikely(db == NULL)) {
        return -ECHRNG;
    }

    ret = men_wait_dmaimg(db, ctrl.index, 1000 * ctrl.timeout, &foundframe, false);
    if (ret < 0)
        return ret;

    // casting uint64_t to long -> make sure to get a positive value
    ctrl.index = foundframe & LONG_MAX;

    if (copy_to_user((void __user *) arg, &ctrl, sizeof(ctrl)))
        return -EFAULT;

    return 0;
}

static long men_ioctl_fg_stop_cmd(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct menable_dmachan *dc;

    if (_IOC_SIZE(cmd) != 0) {
        warn_wrong_iosize(men, cmd, 0);
        return -EINVAL;
    }

    dc = men_dma_channel(men, arg);
    if (dc == NULL)
        return -ECHRNG;
    men_stop_dma(dc);
    return 0;

}

static long men_ioctl_fg_start_transfer(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
#if BITS_PER_LONG == 32
            return men_ioctl_fgstart(men, cmd, arg);
#else /* BITS_PER_LONG == 32 */
            struct fg_ctrl fgr;

            if (unlikely(_IOC_SIZE(cmd) != sizeof(fgr))) {
                warn_wrong_iosize(men, cmd, sizeof(fgr));
                return -EINVAL;
            }

            if (copy_from_user(&fgr, (void __user *)arg, sizeof(fgr)))
                return -EFAULT;

            return fg_start_transfer(men, &fgr, 0);
#endif /* BITS_PER_LONG == 32 */
}

static long men_ioctl_get_buffer_status(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct bufstatus data;
    struct menable_dmahead *dh;
    struct menable_dmachan *dc;

    if (unlikely(_IOC_SIZE(cmd) != sizeof(data))) {
        warn_wrong_iosize(men, cmd, sizeof(data));
        return -EINVAL;
    }

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    // me_get_buf_head acquires men->buffer_heads_lock
    dh = me_get_buf_head(men, data.idx.head);

    if (unlikely(dh == NULL))
        return -EINVAL;

    if (unlikely((data.idx.index >= dh->num_sb) || (data.idx.index < -1))) {
        spin_unlock_bh(&men->buffer_heads_lock);
        return -EINVAL;
    }

    if (data.idx.index == -1) {
        data.status.is_locked = 0;
    } else {
        struct menable_dmabuf *sb = dh->bufs[data.idx.index];
        if (unlikely(sb == NULL)) {
            spin_unlock_bh(&men->buffer_heads_lock);
            return -EINVAL;
        }
        data.status.is_locked = (sb->listname != FREE_LIST && sb->listname != READY_LIST);
    }

    dc = dh->chan;
    if (unlikely(dc == NULL)) {
        spin_unlock_bh(&men->buffer_heads_lock);
        return -EINVAL;
    }

    data.status.free = dc->free_count;
    data.status.grabbed = dc->grabbed_count;
    data.status.locked = dc->locked_count;
    data.status.lost = dc->lost_count;

    spin_unlock_bh(&men->buffer_heads_lock);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

static long men_ioctl_queue_buffer(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct bufstatus data;

    if (unlikely(_IOC_SIZE(cmd) != sizeof(data))) {
        warn_wrong_iosize(men, cmd, sizeof(data));
        return -EINVAL;
    }

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    return men_queue_buffer(men, data.idx.head, data.idx.index);
}

static long men_ioctl_unlock_buffer_number(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct men_io_bufidx data;

    if (unlikely(_IOC_SIZE(cmd) != sizeof(data))) {
        warn_wrong_iosize(men, cmd, sizeof(data));
        return -EINVAL;
    }

    if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
        return -EFAULT;

    return men_ioctl_unlock_buffer(men, data.headnr, data.index);
}

static long men_ioctl_get_handshake_dma_buffer(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct handshake_frame data;
    struct menable_dmahead *dh;
    struct menable_dmabuf *sb;
    struct menable_dmachan *dc;
    unsigned long flags;

    if (unlikely(_IOC_SIZE(cmd) != sizeof(data))) {
        warn_wrong_iosize(men, cmd, sizeof(data));
        return -EINVAL;
    }

    if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
        return -EFAULT;

    switch (data.mode) {
    case SEL_ACT_IMAGE:
    case SEL_NEXT_IMAGE:
        break;
    default:
        return -EINVAL;
    }

    dh = me_get_buf_head(men, data.head);

    if (unlikely(dh == NULL))
        return -EINVAL;

    dc = dh->chan;
    if (unlikely(dc == NULL)) {
        spin_unlock_bh(&men->buffer_heads_lock);
        return -EINVAL;
    }

    spin_lock_irqsave(&dc->listlock, flags);
    switch (data.mode) {
    case SEL_ACT_IMAGE:
        DEV_DBG_IOCTL(&men->dev, "Trying to get current image.\n");
        sb = men_last_blocked(men, dc);
        break;
    case SEL_NEXT_IMAGE:
        DEV_DBG_IOCTL(&men->dev, "Trying to get oldest image.\n");
        sb = men_next_blocked(men, dc);
        break;
    default:
        BUG();
    }

    spin_unlock_irqrestore(&dc->listlock, flags);
    spin_unlock_bh(&men->buffer_heads_lock);

    if (unlikely(sb == NULL)) {
        DEV_DBG_IOCTL(&men->dev, "Failed. No buffer avaiable.\n");
    } else {
        DEV_DBG_IOCTL(&men->dev, "Got oldest buffer. Buffer no.: %lu, frame no.: %llu.\n", sb->index, sb->frame_number);
    }

    data.frame = (sb != NULL) ? sb->index : -12; // -12 = no frame available
    if (copy_to_user((void __user *)arg,
        &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

static long men_ioctl_get_device_status(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct men_get_device_status_o_v8 status;
    int i;

    if (unlikely(_IOC_SIZE(cmd) < sizeof(struct men_get_device_status_o_v6))) {
        warn_wrong_iosize(men, cmd, sizeof(struct men_get_device_status_o_v6));
        return -EINVAL;
    }

    /* Version 6 */

    if (copy_from_user(&status, (const void __user *) arg, sizeof(struct men_get_device_status_o_v6))) {
        return -EFAULT;
    }

    if (SisoBoardIsMe6(men->pci_device_id)) {
        dev_dbg(&men->dev, "Updating board status before delivering to runtime.\n");
        if (me6_update_board_status(men) != 0) {
            dev_err(&men->dev, "Failed to update board status.\n");
            return -EFAULT;
        }
    }

    status.status = men->_state;
    status.config = men->config;
    status.config_ext = men->config_ex;
    status.dma_count = 0;
    for (i = 0; i < men->active_fpgas; ++i)
        status.dma_count += men->dmacnt[i];
    status.uiq_count = men->num_active_uiqs;
    status.node_number = 0;
    status.group_affinity = 0;
    status.affinity_high = 0;
    status.affinity_low = 0;

    status.pcie_dsn_high = men->pcie_dsn_high;
    status.pcie_dsn_low = men->pcie_dsn_low;
    status.pci_vendor_device_id = men->pci_vendor_id | (((u32) men->pci_device_id) << 16);
    status.pci_subsys_vendor_device_id = men->pci_subsys_vendor_id | (((u32) men->pci_subsys_device_id) << 16);
    status.pcie_payload_mode = ((men->pcie_device_ctrl & GENMASK(14, 12)) >> 14)
            | ((men->pcie_device_ctrl & GENMASK(7, 5)) >> (5 - 3))
            | ((men->pcie_device_caps & GENMASK(2, 0)) << 6);
    status.pcie_link_training = (men->pcie_link_stat & GENMASK(9, 0))
            | ((men->pcie_link_caps & GENMASK(9, 0)) << 10);

    unsigned int output_buffer_size = _IOC_SIZE(cmd);

    if (output_buffer_size < sizeof(struct men_get_device_status_o_v7)) {
        status._version = 6;
        status._size = sizeof(struct men_get_device_status_o_v6);

    } else if (output_buffer_size < sizeof(struct men_get_device_status_o_v8)) {
        status._version = 7;
        status._size = sizeof(struct men_get_device_status_o_v7);

    } else {
        status._version = 8;
        status._size = sizeof(struct men_get_device_status_o_v8);

    }

    switch (status._version) {
    case 8:
        status.va_event_offset = men->first_va_event_uiq_idx;
        /* fallthrough*/
    case 7:
        for (i = 0; i < 3; ++i) {
            status.fpga_dna[i] = men->fpga_dna[i];
        }
        /* fallthrough */
    default:
        break;

    }

    if (copy_to_user((void __user *) arg, &status, status._size)) {
        return -EFAULT;
    }

    return 0;
}

static long men_ioctl_data_transfer(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct transaction_header th;

    if (unlikely(_IOC_SIZE(cmd) < sizeof(th))) {
        warn_wrong_iosize(men, cmd, sizeof(th));
        return -EINVAL;
    }

    if (copy_from_user(&th, (void __user *)arg, sizeof(th)) != 0) {
        return -EFAULT;
    }

    return process_transaction(men, &th);
}

static long men_ioctl_camera_control(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    if (_IOC_SIZE(cmd) != sizeof(union camera_control_io)) {
        warn_wrong_iosize(men, cmd, sizeof(union camera_control_io));
        return -EINVAL;
    }

    return do_camera_control(men, (void __user *)arg);
}

static long men_ioctl_subbuf_impl(struct siso_menable * men, unsigned int cmd,
                                  unsigned long out_buf_address, int mem_head_idx, long subbuf_idx) {
    struct menable_dmabuf * sb = me_get_sub_buf(men, mem_head_idx, subbuf_idx);
    if (unlikely(sb == NULL)) {
        return -EINVAL;
    }

    int ret = 0;

    unsigned int ioctl_code = _IOC_NR(cmd);
    switch (ioctl_code) {
    case IOCTL_DMA_TAG:
        DEV_DBG_IOCTL(&men->dev, "DMA tag of buffer %ld is %d.\n", subbuf_idx, ret);
        copy_to_user((uint32_t __user *)out_buf_address, &sb->dma_tag, sizeof(sb->dma_tag));
        break;

    case IOCTL_DMA_LENGTH:
        DEV_DBG_IOCTL(&men->dev, "Transmission length of buffer %ld is %d.\n", subbuf_idx, ret);
        copy_to_user((uint64_t __user *)out_buf_address, &sb->dma_length, sizeof(sb->dma_length));
        break;

    case IOCTL_DMA_FRAME_NUMBER:
        ret = sb->frame_number;
        /* TODO: [RKN] Check size of output buffer? */
        copy_to_user((uint64_t __user *)out_buf_address, &sb->frame_number, sizeof(sb->frame_number));
        DEV_DBG_IOCTL(&men->dev, "Frame number of buffer %ld is %d.\n", subbuf_idx, ret);
        break;

    default:
        dev_warn(&men->dev, "[IOCTL] Invalid Subbuf IOCTL number %u.\n", ioctl_code);
        ret = -ENOTTY;
        break;
    }

    /* The lock is acquired in me_get_sub_buf */
    /* TODO: [RKN] This locking mechanism smells... */
    spin_unlock_bh(&men->buffer_heads_lock);

    return ret;
}

static long men_ioctl_subbuf(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct men_io_bufidx binfo;
    CHECK_AND_COPY_INPUT_BUFFER(men, cmd, arg, binfo);

    return men_ioctl_subbuf_impl(men, cmd, arg, binfo.headnr, binfo.index);
}

static long men_compat_ioctl_subbuf(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct men_io_bufidx32 binfo;
    CHECK_AND_COPY_INPUT_BUFFER(men, cmd, arg, binfo);

    return men_ioctl_subbuf_impl(men, cmd, arg, binfo.headnr, binfo.index);
}

static long men_compat_ioctl_dma_time_stamp(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct dma_timestamp32 ts;
    struct menable_dmabuf *sb;
    menable_ioctl_timespec_t tmp;
    int ret;

    if (unlikely(_IOC_SIZE(cmd) != sizeof(ts))) {
        warn_wrong_iosize(men, cmd, sizeof(ts));
        return -EINVAL;
    }

    if (copy_from_user(&ts, (void __user *) arg, sizeof(ts)))
        return -EFAULT;

    sb = me_get_sub_buf(men, ts.head, ts.buf);
    if (unlikely(sb == NULL))
        return -EINVAL;
    tmp.tv_sec = sb->timestamp.tv_sec & 0x7fffffffUL;
	tmp.tv_nsec = sb->timestamp.tv_nsec;
    spin_unlock_bh(&men->buffer_heads_lock);

    ret = copy_to_user(((void __user *) arg) +
        offsetof(typeof(ts), stamp),
        &tmp, sizeof(tmp));
    return ret ? -EFAULT : 0;
}

long menable_ioctl(struct file *file,
                   unsigned int cmd, unsigned long arg)
{
    struct siso_menable *men = file->private_data;

    unsigned int ioctl_code;

    if (unlikely(men == NULL)) {
        WARN_ON(!men);
        return -EFAULT;
    }

    if (unlikely(_IOC_TYPE(cmd) != 'm'))
        return -EINVAL;

    ioctl_code = _IOC_NR(cmd);
    DBG1(dev_dbg(&men->dev, "Received IOCTL Code %u (%s)\n", ioctl_code, get_ioctl_name(ioctl_code)));

    if (!men_is_ioctl_allowed(men_get_state(men), ioctl_code)) {
        dev_dbg(&men->dev, "IOCTL %u not allowed in state %d\n", ioctl_code, men_get_state(men));
        return -EINVAL;
    }

    switch (ioctl_code) {
    case IOCTL_ALLOCATE_VIRT_BUFFER:
        return men_ioctl_allocate_virt_buffer(men, cmd, arg);

    case IOCTL_ADD_VIRT_USER_BUFFER:
        return men_ioctl_add_virt_user_buffer(men, cmd, arg);

    case IOCTL_DEL_VIRT_USER_BUFFER:
        return men_ioctl_del_virt_user_buffer(men, cmd, arg);

    case IOCTL_FREE_VIRT_BUFFER:
        return men_ioctl_free_virt_buffer(men, cmd, arg);

    case IOCTL_DMA_LENGTH:
    case IOCTL_DMA_TAG:
    case IOCTL_DMA_FRAME_NUMBER:
        return men_ioctl_subbuf(men, cmd, arg);

    case IOCTL_DMA_TIME_STAMP:
        return men_ioctl_dma_time_stamp(men, cmd, arg);

    case IOCTL_FG_WAIT_FOR_SUBBUF:
        return men_ioctl_fg_wait_for_subbuf(men, cmd, arg);

    case IOCTL_FG_STOP_CMD:
        return men_ioctl_fg_stop_cmd(men, cmd, arg);

    case IOCTL_FG_START_TRANSFER:
        return men_ioctl_fg_start_transfer(men, cmd, arg);

    case IOCTL_GET_BUFFER_STATUS:
        return men_ioctl_get_buffer_status(men, cmd, arg);

    case IOCTL_QUEUE_BUFFER:
        return men_ioctl_queue_buffer(men, cmd, arg);

    case IOCTL_UNLOCK_BUFFER_NR:
        return men_ioctl_unlock_buffer_number(men, cmd, arg);

    case IOCTL_GET_HANDSHAKE_DMA_BUFFER:
        return men_ioctl_get_handshake_dma_buffer(men, cmd, arg);

    case IOCTL_EX_GET_DEVICE_STATUS:
        return men_ioctl_get_device_status(men, cmd, arg);

    case IOCTL_EX_DATA_TRANSFER:
        if (!SisoBoardIsMe6(men->pci_device_id)) {
            return -ENOTTY;
        }
        return men_ioctl_data_transfer(men, cmd, arg);

    case IOCTL_EX_CAMERA_CONTROL:
        return men_ioctl_camera_control(men, cmd, arg);

    default:
        return men->ioctl(men, _IOC_NR(cmd), _IOC_SIZE(cmd), arg);
    }
}

static long men_compat_ioctl_allocate_virt_buffer32(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct mm_create_s32 mm_cmd;

    if (_IOC_SIZE(cmd) != sizeof(mm_cmd)) {
        warn_wrong_iosize(men, cmd, sizeof(mm_cmd));
        return -EINVAL;
    }

    if (copy_from_user(&mm_cmd,
        (void __user *) arg,
        sizeof(mm_cmd)))
        return -EFAULT;

    if (mm_cmd.maxsize > 0xffffffffULL)
        return -EINVAL;

    return men_create_buf_head(men,
        mm_cmd.maxsize,
        mm_cmd.subbufs);
}

static long men_compat_ioctl_add_virt_user_buffer32(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct men_io_range32 range;
    struct men_io_range range64;

    if (_IOC_SIZE(cmd) != sizeof(range)) {
        warn_wrong_iosize(men, cmd, sizeof(range));
        return -EINVAL;
    }

    if (copy_from_user(&range,
        (void __user *) arg,
        sizeof(range)))
        return -EFAULT;
    if (range.length == 0)
        return -EFAULT;

    range64.start = range.start;
    range64.length = range.length;
    range64.subnr = range.subnr;
    range64.headnr = range.headnr;
    return men_create_userbuf(men, &range64);
}

static long men_compat_ioctl_del_virt_user_buffer(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct men_io_bufidx32 num;
    struct menable_dmahead *dh;
    int r;

    if (_IOC_SIZE(cmd) != sizeof(num)) {
        warn_wrong_iosize(men, cmd, sizeof(num));
        return -EINVAL;
    }

    if (copy_from_user(&num,
        (void __user *) arg,
        sizeof(num)))
        return -EFAULT;

    dh = me_get_buf_head(men, num.headnr);

    if (dh == NULL) {
        r = -EINVAL;
    } else {
        struct menable_dmabuf *sb = me_get_sub_buf_by_head(dh, num.index);
        r = men_free_userbuf(men, dh, num.index);
        spin_unlock_bh(&men->buffer_heads_lock);
        if (r == 0)
            men_destroy_sb(men, sb);
    }
    return r;
}

static long men_compat_ioctl_wait_for_subbuf32(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct men_io_bufwait32 ctrl;
    struct menable_dmachan *db;
    int ret;
    uint64_t foundframe;

    if (unlikely(_IOC_SIZE(cmd) != sizeof(ctrl))) {
        warn_wrong_iosize(men, cmd, sizeof(ctrl));
        return -EINVAL;
    }

    if (copy_from_user(&ctrl, (void __user *) arg, sizeof(ctrl)))
        return -EFAULT;

    // Don't allow ridiculously high negative numbers
    if (unlikely(ctrl.index < INT_MIN / 2))
        return -EINVAL;

    db = men_dma_channel(men, ctrl.dmachan);
    if (unlikely(db == NULL))
        return -ECHRNG;

    ret = men_wait_dmaimg(db, ctrl.index, 1000 * ctrl.timeout, &foundframe, true);

    if (ret < 0)
        return ret;

    // casting uint64_t to int -> make sure to get a positive value
    ctrl.index = foundframe & INT_MAX;

    if (copy_to_user((void __user *) arg, &ctrl, sizeof(ctrl)))
        return -EFAULT;

    return 0;
}

static long men_compat_ioctl_unlock_buffer_nr(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct men_io_bufidx32 data;

    if (unlikely(_IOC_SIZE(cmd) != sizeof(data))) {
        warn_wrong_iosize(men, cmd, sizeof(data));
        return -EINVAL;
    }

    if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
        return -EFAULT;

    return men_ioctl_unlock_buffer(men, data.headnr, data.index);
}

static long men_compat_ioctl_get_device_status(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct men_get_device_status_o_v7 status;
    int i;

    if (unlikely(_IOC_SIZE(cmd) < sizeof(struct men_get_device_status_o_v6))) {
        warn_wrong_iosize(men, cmd, sizeof(struct men_get_device_status_o_v6));
        return -EINVAL;
    }

    /* Version 6 */

    if (copy_from_user(&status, (const void __user *) arg, sizeof(struct men_get_device_status_o_v6))) {
        return -EFAULT;
    }

    status.status = men->_state;
    status.config = men->config;
    status.config_ext = men->config_ex;
    status.dma_count = 0;
    for (i = 0; i < men->active_fpgas; ++i)
        status.dma_count += men->dmacnt[i];
    status.uiq_count = men->num_active_uiqs;
    status.node_number = 0;
    status.group_affinity = 0;
    status.affinity_high = 0;
    status.affinity_low = 0;

    status.pcie_dsn_high = men->pcie_dsn_high;
    status.pcie_dsn_low = men->pcie_dsn_low;
    status.pci_vendor_device_id = men->pci_vendor_id | (((u32) men->pci_device_id) << 16);
    status.pci_subsys_vendor_device_id = men->pci_subsys_vendor_id | (((u32) men->pci_subsys_device_id) << 16);
    status.pcie_payload_mode = ((men->pcie_device_ctrl & GENMASK(14, 12)) >> 14)
            | ((men->pcie_device_ctrl & GENMASK(7, 5)) >> (5 - 3))
            | ((men->pcie_device_caps & GENMASK(2, 0)) << 6);
    status.pcie_link_training = (men->pcie_link_stat & GENMASK(9, 0))
            | ((men->pcie_link_caps & GENMASK(9, 0)) << 10);

    if (_IOC_SIZE(cmd) < sizeof(struct men_get_device_status_o_v7)) {

        /* Version 6 */

        status._size = sizeof(struct men_get_device_status_o_v6);
        status._version = 6;

        if (copy_to_user((void __user *) arg, &status, sizeof(struct men_get_device_status_o_v6))) {
            return -EFAULT;
        }

    } else {

        /* Version 7 */

        for (i = 0; i < 3; ++i) status.fpga_dna[i] = men->fpga_dna[i];

        status._size = sizeof(struct men_get_device_status_o_v7);
        status._version = 7;

        if (copy_to_user((void __user *) arg, &status, sizeof(struct men_get_device_status_o_v7))) {
            return -EFAULT;
        }
    }

    return 0;
}

static long men_compat_ioctl_data_transfer(struct siso_menable * men, unsigned int cmd, unsigned long arg) {
    struct transaction_header th;
    if (copy_from_user(&th, (void __user *)arg, sizeof(th))) {
        return -EFAULT;
    }

    return process_transaction(men, &th);
}

long menable_compat_ioctl(struct file *file,
                          unsigned int cmd, unsigned long arg)
{
    unsigned int ioctl_code;
    struct siso_menable *men = file->private_data;

    if (unlikely(men == NULL)) {
        WARN_ON(!men);
        return -EFAULT;
    }

    if (unlikely(_IOC_TYPE(cmd) != 'm'))
        return -EINVAL;

    ioctl_code = _IOC_NR(cmd);
    DBG_STMT(dev_dbg(&men->dev, "Received IOCTL Code %u (%s)\n", ioctl_code, get_ioctl_name(ioctl_code)));

    if (!men_is_ioctl_allowed(men_get_state(men), ioctl_code)) {
        dev_warn(&men->dev, "IOCTL %u not allowed in state %d\n", ioctl_code, men_get_state(men));
        return -EINVAL;
    }

    switch (ioctl_code) {
    case IOCTL_ALLOCATE_VIRT_BUFFER32:
        return men_compat_ioctl_allocate_virt_buffer32(men, cmd, arg);

    case IOCTL_ADD_VIRT_USER_BUFFER32:
        return men_compat_ioctl_add_virt_user_buffer32(men, cmd, arg);

    case IOCTL_DEL_VIRT_USER_BUFFER:
        return men_compat_ioctl_del_virt_user_buffer(men, cmd, arg);

    case IOCTL_FG_START_TRANSFER32:
        return men_ioctl_fgstart(men, cmd, arg);

    case IOCTL_DMA_LENGTH:
    case IOCTL_DMA_TAG:
    case IOCTL_DMA_FRAME_NUMBER:
        return men_compat_ioctl_subbuf(men, cmd, arg);

    case IOCTL_DMA_TIME_STAMP:
        return men_compat_ioctl_dma_time_stamp(men, cmd, arg);

    case IOCTL_FG_WAIT_FOR_SUBBUF32:
        return men_compat_ioctl_wait_for_subbuf32(men, cmd, arg);

    case IOCTL_UNLOCK_BUFFER_NR:
        return men_compat_ioctl_unlock_buffer_nr(men, cmd, arg);

    case IOCTL_EX_GET_DEVICE_STATUS:
        return men_compat_ioctl_get_device_status(men, cmd, arg);

    case IOCTL_EX_DATA_TRANSFER:
        return men_compat_ioctl_data_transfer(men, cmd, arg);
        
    case IOCTL_EX_CAMERA_CONTROL:
        return men_ioctl_camera_control(men, cmd, arg);

    default:
        if (men->compat_ioctl)
            return men->compat_ioctl(men, _IOC_NR(cmd), _IOC_SIZE(cmd), arg);
        else
            return -ENOIOCTLCMD;
    }
}
