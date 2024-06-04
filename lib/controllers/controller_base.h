/************************************************************************
 * Copyright 2006-2020 Silicon Software GmbH, 2021-2024 Basler AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#ifndef DATA_TRANSPORT_INTERFACE_H_
#define DATA_TRANSPORT_INTERFACE_H_

#include "../os/types.h"
#include "../fpga/register_interface.h"
#include "../ioctl_interface/transaction.h"

#ifdef __cplusplus 
extern "C" {
#endif

/**
 * Base type for data transport interfaces.
 * A data transport interface takes care of reading from and writing to a target (usually some device).
 *
 * This base implementation provides a general mechanism to write / read data in bursts.
 * The protocol- and controller specifics must be provided by the user via the functions required
 * by data_transport_interface_init.
 */
struct controller_base {
    struct register_interface * register_interface;
    user_mode_lock * lock;

    /* functions provided by data_transport_interface */

    /**
     * Begin a transaction.
     * This function calls handle_begin_transaction internally.
     *
     * @param self self pointer
     * @return STATUS_OK on success, STATUS_ERR... on failure.
     */
    int (*begin_transaction)(struct controller_base * self);

    /**
     * End a transaction.
     * This function calls handle_end_transaction internally.
     *
     * @param self self pointer
     */
    void (*end_transaction)(struct controller_base * self);

    /**
     * Reads data from the target.
     * This function calls prepare_read and read_shot internally.
     *
     * @attention
     * This function relies on the target to actually write data. Usually a preceding
     * write_burst is required to request the data (i.e. sending a read command and the address to read from).
     *
     * @param self self pointer
     * @param bh burst_header that specifies the burst details
     * @param buf buffer to be filled with the data
     * @param size the number of bytes to read
     * @return STATUS_OK on success, STATUS_ERR... on failure.
     */
    int (*read_burst)(struct controller_base * self, struct burst_header * bh, uint8_t * buf, size_t size);

    /**
     * Writes data to the target.
     * This function calls write_shot internally.
     *
     * @param self self pointer
     * @param bh burst_header that specifies the burst details
     * @param buf buffer to be filled with the data
     * @param size the number of bytes to read
     * @return STATUS_OK on success, STATUS_ERR... on failure.
     */
    int (*write_burst)(struct controller_base * self, struct burst_header * bh, const uint8_t * buf, size_t size);

    /*
     * Performs a state change only by processing all flags.
     *
     * @param self self pointer
     * @param bh burst_header that specifies the burst details
     * @return STATUS_OK on success, STATUS_ERR... on failure.
     */
    int(*state_change_burst)(struct controller_base * self, struct burst_header * bh);

    /**
     * Executes a command.
     * 
     * @param self self pointer
     * @param burst_header header with the detail information for the burst
     * @param buf buffer that starts with a command_burst_header, optionally followed by command specific data
     * @param size size of the buffer in bytes
     *
     * @return STATUS_OK on success, error code (STATUS_ERR_...) on failure.
     */
    int (*command_execution_burst)(struct controller_base * self, struct burst_header *burst_header, uint8_t * buf, size_t size);

    /**
     * Checks if certain flags are set for the current burst.
     *
     * @param self self pointer
     * @param flags the flags to check
     * @return true if all flags are set, otherwise false.
     */
    bool (*are_burst_flags_set)(struct controller_base * self, uint8_t flags);

    /**
     * Destroys the controller instance (without releasing memory for the controller itself).
     * This function calls cleanup internally.
     *
     * @param self self pointer
     */
    void (*destroy)(struct controller_base * self);

    /*
     * Functions to be provided by user (pure virtual functions). Do not call directly!
     *
     * Implementation remarks:
     * These function are called internally by the data_transport_interface
     * and must therefore be provided in any case when using a data_transport_interface.
     * If a specific operation is not required in an implementation, provide a no-op function.
     */

    /**
     * Handles all implementation specific actions that need to be handled before
     * beginning to process the transaction.
     *
     * @param self self pointer
     * @return STATUS_OK if all flags have been processed properly, STATUS_ERR... otherwise.
     */
    int (*handle_begin_transaction)(struct controller_base * self);

    /**
     * Handles all implementation specific actions that need to be handled after
     * processing the transaction.
     *
     * @param self self pointer
     */
    void (*handle_end_transaction)(struct controller_base * self);

    /**
     * Handles all implementation specific burst flags that need to be handled before
     * beginning to process the burst.
     *
     * @param self self pointer
     * @param flags the burst flags
     * @return STATUS_OK if all flags have been processed properly, STATUS_ERR... otherwise.
     */
    /* TODO Rename to 'prepare_burst' so that other preparations would fit here as wall
     *      Remove the flags argument, since the dti has a burst_flags member that
     *      can be accessed by derived types.
     */
    int (*handle_pre_burst_flags)(struct controller_base * self, uint32_t burst_flags);

    /**
     * Handles all implementation specific burst flags that need to be handled after
     * the processing the burst is finished.
     *
     * @param self self pointer
     * @param flags the burst flags
     * @return STATUS_OK if all flags have been processed properly, STATUS_ERR... otherwise.
     */
    /* TODO Rename to 'finalize_burst' so other finalizations would fit here as well.
     *      Remove the flags argument, since the dti has a burst_flags member that
     *      can be accessed by derived types.
     */
    int (*handle_post_burst_flags)(struct controller_base * self, uint32_t flags);

    /**
     * Writes num_bytes bytes to the target in one go.
     *
     * @remark
     * The implementation may rely on num_bytes being <= max_bytes_per_write.
     *
     * @param self self pointer
     * @param buf the bytes to write
     * @param num_bytes the number of bytes to write
     * @return STATUS_OK if writing the bytes succeeded, STATUS_ERR... otherwise
     */
    int (*write_shot)(struct controller_base * self, const uint8_t * buf, int num_bytes);

    /**
     * Sends a request to the target to send num_bytes for reading.
     *
     * @remark
     * The implementation may rely on num_bytes being <= max_bytes_per_read
     *
     * @param self self pointer
     * @param num_bytes The number of bytes to request
     * @return STATUS_OK if the request was issued properly, STATUS_ERR... otherwise.
     */
    int (*request_read)(struct controller_base * self, size_t num_bytes);

    /**
     * Reads num_bytes from the target.
     *
     * @remark
     * The implementation may rely on num_bytes being <= max_bytes_per_read
     *
     * @attention
     * If request_read returns an error, the burst is aborted. The implementation
     * is responsible for handling errors in the communication procedure.
     *
     * @param self self pointer
     * @param buffer buffer to fill with the bytes read
     * @param num_bytes number of bytes to read
     * @return STATUS_OK if the request was issued properly, STATUS_ERR... otherwise.
     */
    int (*read_shot)(struct controller_base * self, uint8_t * buffer, size_t num_bytes);

    /**
     * Executes a command.
     * 
     * @param self self pointer
     * @param command_id the id of the command to execute. The id is controller specific.
     * @param command_data a buffer that contains the data required by the command. Usually this will be a struct that
     *                     contains the arguments and/or provides a member for the return value.
     * @param command_data_size size of the command data buffer
     *
     * @return STATUS_OK if the command could be executed, STATUS_ERR_... otherwise. Note that this value
     *         only provides information about whether or not the command execution was possible and not
     *         about the command itself. The command's return value will usually be provided in the 
     *         command_data.
     */
    int(*execute_command)(struct controller_base * self, command_burst_header * header, uint8_t * command_data, size_t command_data_size);

    /**
     * Waits until it is safe enqueue write `write_queue_size` write operations.
     *
     * @remark
     * This function is called by the data_tranport_interface when it is not certain, if write
     * operations can be issued.
     *
     * @param self self pointer
     * @return STATUS_OK if it is safe to issue `write_queue_size` write operations.
     *         If an error occurs (i.e. timeout elapsed), STATUS_ERR... is returned.
     */
    /* TODO Rename to sth. more general (i.e. wait_for_write_possible)
     */
    int (*wait_for_write_queue_empty)(struct controller_base * self);

    /**
     * Implementation specific cleanup after a burst has been aborted.
     * @param self self pointer.
     */
    void (*burst_aborted)(struct controller_base * self);

    /**
     * Implementation specific cleanup when the driver is unloaded.
     * @param self self pointer.
     */
    void (*cleanup)(struct controller_base * self);

    /* data to be provided by the user / implmentation */

    /**
     * Number of read operations that can be queued.
     * If there is no read queue, set this to 1.
     */
    size_t read_queue_size;

    /**
     * Number of bytes per read operation.
     */
    size_t max_bytes_per_read;

    /**
     * Number of write operations that can be queued.
     * If there is no write queue, set this to 1.
     */
    size_t write_queue_size;

    /**
     * Number of bytes per write operation.
     */
    size_t max_bytes_per_write;

    /* data provided by the data_transport_interface */

    /**
     * The flags for the current burst as provided by
     * caller of `[read|write]_burst` via the burst_header.
     */
    uint8_t current_burst_flags;

    /**
     * Indicates if the current write|read shot is the first one
     * in the current burst.
     *
     * This can be useful in the implementation of [write|read]_shot,
     * if special handling of the first shot is required that can not
     * be achieved in handle_pre_burst_flags.
     */
    bool is_first_shot;

    /**
     * Indicates if the current write|read shot is the last one
     * in the current burst.
     *
     * This can be useful in the implementation of [write|read]_shot,
     * if special handling of the last shot is required that can not
     * be achieved in handle_post_burst_flags.
     */
    bool is_last_shot;
};

/**
 * Use this function to initialize a data_transport_interface instance properly.
 * Not initializing it this way is very likely to result in an unusable instance.
 *
 * See the data_transport_interface documentation for details on the arguments.
 */
void controller_base_init(
        struct controller_base * ctrl,
        struct register_interface * ri,
        user_mode_lock * lock,
        int read_queue_size,
        int max_bytes_per_read,
        int write_queue_size,
        int max_bytes_per_write,
        int (*handle_begin_transaction)(struct controller_base * ctrl),
        void (*handle_end_transaction)(struct controller_base * ctrl),
        int (*handle_pre_burst_flags)(struct controller_base * ctrl, uint32_t flags),
        int (*handle_post_burst_flags)(struct controller_base * ctrl, uint32_t flags),
        int (*write_shot)(struct controller_base * ctrl, const uint8_t * buf, int num_bytes),
        int (*request_read)(struct controller_base * ctrl, size_t num_bytes),
        int (*read_shot)(struct controller_base * ctrl, uint8_t * buffer, size_t num_bytes),
        int (*execute_command)(struct controller_base * self, command_burst_header * header, uint8_t * command_data, size_t command_data_size),
        int (*wait_for_write_queue_empty)(struct controller_base * ctrl),
        void (*burst_aborted)(struct controller_base * self),
        void(*cleanup)(struct controller_base * self));

/**
 * Use this function to initialize an aggregate data_transport_interface instance properly,
 * which overrides the "non-virtual" functions. For aggregates, controller_base_init() still
 * must be called separately.
 * Not initializing it this way is very likely to result in an unusable instance.
 *
 * See the data_transport_interface documentation for details on the arguments.
 */
void controller_base_super_init(
    struct controller_base * ctrl,
    struct register_interface * ri,
    user_mode_lock * lock,
    int read_queue_size,
    int max_bytes_per_read,
    int write_queue_size,
    int max_bytes_per_write,
    int(*begin_transaction)(struct controller_base * self),
    void(*end_transaction)(struct controller_base * self),
    int(*read_burst)(struct controller_base * self, struct burst_header * bh, uint8_t * buf, size_t size),
    int(*write_burst)(struct controller_base * self, struct burst_header * bh, const uint8_t * buf, size_t size),
    int(*state_change_burst)(struct controller_base * self, struct burst_header * bh),
    int(*command_execution_burst)(struct controller_base *, struct burst_header *, uint8_t *, size_t),
    bool(*are_burst_flags_set)(struct controller_base * self, uint8_t flags),
    void(*destroy)(struct controller_base * self));

/**
 * A function that can be used for the execute_command function pointer in 
 * derived controllers if these don't support any commands.
 */
int controller_base_execute_command_not_supported(struct controller_base * self, command_burst_header * header, uint8_t * command_data, size_t command_data_size);

#ifdef __cplusplus 
} // extern "C"
#endif

#endif /* DATA_TRANSPORT_INTERFACE_H_ */
