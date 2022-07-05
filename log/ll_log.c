#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <fnmatch.h>


#include <ll_log.h>

/*
 * Convert log level to string.
 */
const char *eal_log_level2str(uint32_t level);

struct ll_log_dynamic_type {
	const char *name;
	uint32_t loglevel;
};

static struct ll_logs {
	uint32_t type; 
	uint32_t level;
	FILE *file; // output file set by ll_openlog_stream, or NULL
	size_t dynamic_types_len;
	struct ll_log_dynamic_type *dynamic_types;
}ll_logs = {
	.type = UINT32_MAX,
	.level = LL_LOG_DEBUG,
};

/*stream to use for logging if ll_logs.file is NULL*/
static FILE *default_log_stream;

/*change the stream that will be used by logging system*/
int
ll_openlog_stream(FILE* f) {
	ll_logs.file = f;
	return 0;
}

FILE *
ll_log_get_stream(void) {
	FILE *f = ll_logs.file;
	
	if (f == NULL) {
		return default_log_stream ? default_log_stream: stderr;
	}
	
	return f;
}

/*Get global log level */
uint32_t
ll_log_get_global_level(void) {
	return ll_logs.level;
}

/* Set global log level */
void
ll_log_set_global_level(uint32_t level)
{
	ll_logs.level = (uint32_t)level;
}

uint32_t
ll_log_get_level(uint32_t type) {
	if (type >= ll_logs.dynamic_types_len)
		return -1;
	
	return ll_logs.dynamic_types[type].loglevel;
}

bool
ll_log_can_log(uint32_t logtype,uint32_t level) {
	int log_level;
	
	if (level > ll_log_get_global_level())
		return false;
	
	log_level = ll_log_get_level(logtype);
	if (log_level < 0)
		return false;

	if (level > (uint32_t)log_level)
		return false;

	return true;
}

static void
logtype_set_level(uint32_t type, uint32_t level) {
	
	uint32_t current = ll_logs.dynamic_types[type].loglevel;
	
	if (current != level) {
		ll_logs.dynamic_types[type].loglevel = level;
		LL_LOG(DEBUG, EAL, "%s log level changed from %s to %s\n",
			ll_logs.dynamic_types[type].name == NULL ?
				"" : ll_logs.dynamic_types[type].name,
			eal_log_level2str(current),
			eal_log_level2str(level));
	}
}

int
ll_log_set_level(uint32_t type, uint32_t level)
{
	if (type >= ll_logs.dynamic_types_len)
		return -1;
	if (level > LL_LOG_MAX)
		return -1;

	logtype_set_level(type, level);

	return 0;
}

//used name find the logtype index
static int
log_lookup(const char* name)
{
	size_t i;

	for (i = 0; i < ll_logs.dynamic_types_len; i++) {
		if (ll_logs.dynamic_types[i].name == NULL)
			continue;
		if (strcmp(name, ll_logs.dynamic_types[i].name) == 0)
			return i;
	}

	return -1;
}

static int
log_register(const char* name, uint32_t level) {
	struct ll_log_dynamic_type *new_dynamic_types;
	int id;
	
	id = log_lookup(name);
	if (id >= 0) 
		return id;
	
	new_dynamic_types = realloc(ll_logs.dynamic_types,
			sizeof(struct ll_log_dynamic_type) *
			(ll_logs.dynamic_types_len + 1));
	if(new_dynamic_types == NULL)
		return -ENOMEM;
	ll_logs.dynamic_types = new_dynamic_types;
	
	id = ll_logs.dynamic_types_len;
	memset(&ll_logs.dynamic_types[id], 0,
			sizeof(struct ll_log_dynamic_type));
	ll_logs.dynamic_types[id].name = strdup(name);
	if (ll_logs.dynamic_types[id].name == NULL)
		return -ENOMEM;
	logtype_set_level(id, level);
	
	++ll_logs.dynamic_types_len;

	return id;
}

/* set log level based on globbing pattern */
int
rte_log_set_level_pattern(const char *pattern, uint32_t level)
{
	size_t i;

	if (level > LL_LOG_MAX)
		return -1;

	for (i = 0; i < ll_logs.dynamic_types_len; i++) {
		if (ll_logs.dynamic_types[i].name == NULL)
			continue;

		if (fnmatch(pattern, ll_logs.dynamic_types[i].name, 0) == 0)
			logtype_set_level(i, level);
	}

	return 0;
}


struct logtype {
	uint32_t log_id;
	const char* logtype;
};

static const struct logtype logtype_strings[] = {
	{LL_LOGTYPE_EAL, "EAL"},
	{LL_LOGTYPE_RING, "RING"},
	{LL_LOGTYPE_FILE, "FILE"},
	{LL_LOGTYPE_PERF, "PERF"},
	{LL_LOGTYPE_USER1,      "user1"},
	{LL_LOGTYPE_USER2,      "user2"},
	{LL_LOGTYPE_USER3,      "user3"},
	{LL_LOGTYPE_USER4,      "user4"},
	{LL_LOGTYPE_USER5,      "user5"},
	{LL_LOGTYPE_USER6,      "user6"},
	{LL_LOGTYPE_USER7,      "user7"},
	{LL_LOGTYPE_USER8,      "user8"}
};

/* Logging should be first initializer (before drivers and bus) */
LL_INIT_PRIO(log_init, LOG)
{
	uint32_t i;

	ll_log_set_global_level(LL_LOG_DEBUG);

	ll_logs.dynamic_types = calloc(LL_LOGTYPE_FIRST_EXT_ID,
		sizeof(struct ll_log_dynamic_type));
	if (ll_logs.dynamic_types == NULL)
		return;

	/* register legacy log types */
	for (i = 0; i < LL_DIM(logtype_strings); i++) {
		ll_logs.dynamic_types[logtype_strings[i].log_id].name =
			strdup(logtype_strings[i].logtype);
		logtype_set_level(logtype_strings[i].log_id, LL_LOG_INFO);
	}

	ll_logs.dynamic_types_len = LL_LOGTYPE_FIRST_EXT_ID;
}


const char*
eal_log_level2str(uint32_t level)
{
	switch (level) {
	case 0: return "disabled";
	case LL_LOG_EMERG: return "emergency";
	case LL_LOG_ALERT: return "alert";
	case LL_LOG_CRIT: return "critical";
	case LL_LOG_ERR: return "error";
	case LL_LOG_WARNING: return "warning";
	case LL_LOG_NOTICE: return "notice";
	case LL_LOG_INFO: return "info";
	case LL_LOG_DEBUG: return "debug";
	default: return "unknown";
	}
}


int
ll_log(uint32_t level, uint32_t logtype, const char *format, ...) {
	
	va_list ap;
	int ret;
	FILE *f = ll_log_get_stream();

	if (logtype >= ll_logs.dynamic_types_len)
		return -1;
	if (!ll_log_can_log(logtype, level))
		return 0;

	va_start(ap, format);

	time_t t;
	time(&t);
	struct tm *tblock;
	tblock = localtime(&t);
	ret = fprintf(f,"[%4d.%02d.%02d %02d:%02d:%02d]\t",tblock->tm_year+1900,tblock->tm_mon+1,tblock->tm_mday,tblock->tm_hour,tblock->tm_min,tblock->tm_sec);
	ret += vfprintf(f, format, ap);
	fflush(f);

	va_end(ap);
	return ret;
}


