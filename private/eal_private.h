
#ifndef _EAL_PRIVATE_H_
#define _EAL_PRIVATE_H_

/** File locking operation. */
enum eal_flock_op {
	EAL_FLOCK_SHARED,    /**< Acquire a shared lock. */
	EAL_FLOCK_EXCLUSIVE, /**< Acquire an exclusive lock. */
	EAL_FLOCK_UNLOCK     /**< Release a previously taken lock. */
};

/** Behavior on file locking conflict. */
enum eal_flock_mode {
	EAL_FLOCK_WAIT,  /**< Wait until the file gets unlocked to lock it. */
	EAL_FLOCK_RETURN /**< Return immediately if the file is locked. */
};

/**
 * Lock or unlock the file.
 *
 * On failure @code rte_errno @endcode is set to the error code
 * specified by POSIX flock(3) description.
 *
 * @param fd
 *  Opened file descriptor.
 * @param op
 *  Operation to perform.
 * @param mode
 *  Behavior on conflict.
 * @return
 *  0 on success, (-1) on failure.
 */
int
eal_file_lock(int fd, enum eal_flock_op op, enum eal_flock_mode mode);

#endif