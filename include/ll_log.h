#ifndef _LL_LOG_H_
#define _LL_LOG_H_

/**
 * @file
 *
 * LL Logs API
 *
 * This file provides a log API to RTE applications.
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#include <ll_common.h>

/*SDK log type */
#define LL_LOGTYPE_EAL 0 /*sdk type log */
#define LL_LOGTYPE_RING 1 /*Log related to ring*/
#define LL_LOGTYPE_MEMPOOL 2 /*Log related to mempool*/
#define LL_LOGTYPE_FILE 3 /*Log related to file*/
#define LL_LOGTYPE_PERF 4 /*Log related to perf*/
#define LL_LOGTYPE_PROC 5 /*Log related to proc*/

/* these log types can be used in an application */
#define LL_LOGTYPE_USER1     24 /**< User-defined log type 1. */
#define LL_LOGTYPE_USER2     25 /**< User-defined log type 2. */
#define LL_LOGTYPE_USER3     26 /**< User-defined log type 3. */
#define LL_LOGTYPE_USER4     27 /**< User-defined log type 4. */
#define LL_LOGTYPE_USER5     28 /**< User-defined log type 5. */
#define LL_LOGTYPE_USER6     29 /**< User-defined log type 6. */
#define LL_LOGTYPE_USER7     30 /**< User-defined log type 7. */
#define LL_LOGTYPE_USER8     31 /**< User-defined log type 8. */

/** First identifier for extended logs */
#define LL_LOGTYPE_FIRST_EXT_ID 32

/* Can't use 0, as it gives compiler warnings */
#define LL_LOG_EMERG    1U  /**< System is unusable.               */
#define LL_LOG_ALERT    2U  /**< Action must be taken immediately. */
#define LL_LOG_CRIT     3U  /**< Critical conditions.              */
#define LL_LOG_ERR      4U  /**< Error conditions.                 */
#define LL_LOG_WARNING  5U  /**< Warning conditions.               */
#define LL_LOG_NOTICE   6U  /**< Normal but significant condition. */
#define LL_LOG_INFO     7U  /**< Informational.                    */
#define LL_LOG_DEBUG    8U  /**< Debug-level messages.             */
#define LL_LOG_MAX LL_LOG_DEBUG /**< Most detailed log level.     */

int ll_log(uint32_t level, uint32_t logtype, const char *format, ...)
#ifdef __GUNC__
	__attribute__ (( format(printf, 3, 4) ));

#else
	;
#endif

/**
 * Change the stream that will be used by the logging system.
 *
 * This can be done at any time. The f argument represents the stream
 * to be used to send the logs. If f is NULL, the default output is
 * used (stderr).
 *
 * @param f
 *   Pointer to the stream.
 * @return
 *   - 0 on success.
 *   - Negative on error.
 */
int ll_openlog_stream(FILE *f);


/**
 * Set the global log level.
 *
 * After this call, logs with a level lower or equal than the level
 * passed as argument will be displayed.
 *
 * @param level
 *   Log level. A value between LL_LOG_EMERG (1) and LL_LOG_DEBUG (8).
 */
void ll_log_set_global_level(uint32_t level);


/**
 * Set the log level for a given type.
 *
 * @param logtype
 *   The log type identifier.
 * @param level
 *   The level to be set.
 * @return
 *   0 on success, a negative value if logtype or level is invalid.
 */
int ll_log_set_level(uint32_t logtype, uint32_t level);

/**
 * Generates a log message.
 *
 * The RTE_LOG() is a helper that prefixes the string with the log level
 * and type, and call rte_log().
 *
 * @param l
 *   Log level. A value between EMERG (1) and DEBUG (8). The short name is
 *   expanded by the macro, so it cannot be an integer value.
 * @param t
 *   The log type, for example, EAL. The short name is expanded by the
 *   macro, so it cannot be an integer value.
 * @param ...
 *   The fmt string, as in printf(3), followed by the variable arguments
 *   required by the format.
 * @return
 *   - 0: Success.
 *   - Negative on error.
 */
#define LL_LOG(l, t, ...)					\
	 ll_log(LL_LOG_ ## l,					\
		 LL_LOGTYPE_ ## t, # t ": " __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif //_LL_LOG_H_
