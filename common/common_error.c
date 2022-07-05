#undef _GNU_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <ll_errno.h>

#ifdef LL_EXEC_ENV_WINDOWS
#define strerror_r(errnum, buf, buflen) strerror_s(buf, buflen, errnum)
#endif

LL_DEFINE_PER_THREAD(int, _ll_errno);

const char *
ll_strerror(int errnum)
{
	/* BSD puts a colon in the "unknown error" messages, Linux doesn't */
#ifdef LL_EXEC_ENV_FREEBSD
	static const char *sep = ":";
#else
	static const char *sep = "";
#endif
#define RETVAL_SZ 256
	static LL_DEFINE_PER_THREAD(char[RETVAL_SZ], retval);
	char *ret = LL_PER_THREAD(retval);

	/* since some implementations of strerror_r throw an error
	 * themselves if errnum is too big, we handle that case here */
	if (errnum >= LL_MAX_ERRNO)
#ifdef LL_EXEC_ENV_WINDOWS
		snprintf(ret, RETVAL_SZ, "Unknown error");
#else
		snprintf(ret, RETVAL_SZ, "Unknown error%s %d", sep, errnum);
#endif
	else
		switch (errnum){
		case E_LL_SECONDARY:
			return "Invalid call in secondary process";
		case E_LL_NO_CONFIG:
			return "Missing rte_config structure";
		default:
			if (strerror_r(errnum, ret, RETVAL_SZ) != 0)
				snprintf(ret, RETVAL_SZ, "Unknown error%s %d",
						sep, errnum);
		}

	return ret;
}
