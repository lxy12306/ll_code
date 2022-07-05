#include <ll_log.h>

/* for legacy log test */
#define LL_LOGTYPE_TESTAPP1 LL_LOGTYPE_USER1
#define LL_LOGTYPE_TESTAPP2 LL_LOGTYPE_USER2


static int
test_legacy_logs(void)
{
	printf("== static log types\n");

	/* set logtype level low to so we can test global level */
	ll_log_set_level(LL_LOGTYPE_TESTAPP1, LL_LOG_DEBUG);
	ll_log_set_level(LL_LOGTYPE_TESTAPP2, LL_LOG_DEBUG);

	/* log in error level */
	ll_log_set_global_level(LL_LOG_DEBUG);
	LL_LOG(ERR, TESTAPP1, "error message\n");
	LL_LOG(CRIT, TESTAPP1, "critical message\n");

	/* log in critical level */
	ll_log_set_global_level(LL_LOG_CRIT);
	LL_LOG(ERR, TESTAPP2, "error message (not displayed)\n");
	LL_LOG(CRIT, TESTAPP2, "critical message\n");

	/* bump up single log type level above global to test it */
	ll_log_set_level(LL_LOGTYPE_TESTAPP2, LL_LOG_EMERG);

	/* log in error level */
	ll_log_set_global_level(LL_LOG_ERR);
	LL_LOG(ERR, TESTAPP1, "error message\n");
	LL_LOG(ERR, TESTAPP2, "error message (not displayed)\n");

	return 0;
}

int main() {
	test_legacy_logs();
	return 0;
}