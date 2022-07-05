#include <stdarg.h>
#include <ll_log.h>
#include <ll_debug.h>


void
__ll_panic(const char *funcname, const char *format, ...)
{
	va_list ap;

	ll_log(LL_LOG_CRIT, LL_LOGTYPE_EAL, "PANIC in %s():\n", funcname);
	va_start(ap, format);
	ll_log(LL_LOG_CRIT, LL_LOGTYPE_EAL, format, ap);
	va_end(ap);
	//ll_dump_stack();
	abort(); /* generate a coredump if enabled */
}