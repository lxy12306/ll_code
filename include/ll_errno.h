#ifndef _LL_ERRNO_H_
#define _LL_ERRNO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <ll_thread_lcore.h>

	LL_DECLARE_PER_THREAD(int, _ll_errno); /**< Per thread error number. */

	/**
	 * Error number value, stored per-thread, which can be queried after
	 * calls to certain functions to determine why those functions failed.
	 *
	 * Uses standard values from errno.h wherever possible, with a small number
	 * of additional possible values for RTE-specific conditions.
	 */
#define ll_errno LL_PER_THREAD(_ll_errno)

	 /**
	  * Function which returns a printable string describing a particular
	  * error code. For non-RTE-specific error codes, this function returns
	  * the value from the libc strerror function.
	  *
	  * @param errnum
	  *   The error number to be looked up - generally the value of ll_errno
	  * @return
	  *   A pointer to a thread-local string containing the text describing
	  *   the error.
	  */
	const char* ll_strerror(int errnum);

#ifndef __ELASTERROR
	/**
	 * Check if we have a defined value for the max system-defined errno values.
	 * if no max defined, start from 1000 to prevent overlap with standard values
	 */
#define __ELASTERROR 1000
#endif

	 /** Error types */
	enum {
		LL_MIN_ERRNO = __ELASTERROR, /**< Start numbering above std errno vals */

		E_LL_SECONDARY, /**< Operation not allowed in secondary processes */
		E_LL_NO_CONFIG, /**< Missing rte_config */

		LL_MAX_ERRNO    /**< Max RTE error number */
	};

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ERRNO_H_ */
