#include <unistd.h>
#include <limits.h>
#include <stdio.h>

#include <ll_common.h>
#include <ll_config.h>
#include <ll_log.h>
#include <ll_thread_lcore.h>


#define SYS_CPU_DIR "/sys/devices/system/cpu/cpu%u"
#define CORE_ID_FILE "topology/core_id"
#define NUMA_NODE_PATH "/sys/devices/system/node"

/* parse a sysfs (or other) file containing one integer value */
int eal_parse_sysfs_value(const char *filename, unsigned long *val)
{
	FILE *f;
	char buf[BUFSIZ];
	char *end = NULL;

	if ((f = fopen(filename, "r")) == NULL) {
		LL_LOG(ERR, EAL, "%s(): cannot open sysfs value %s\n",
			__func__, filename);
		return -1;
	}

	if (fgets(buf, sizeof(buf), f) == NULL) {
		LL_LOG(ERR, EAL, "%s(): cannot read sysfs value %s\n",
			__func__, filename);
		fclose(f);
		return -1;
	}
	*val = strtoul(buf, &end, 0);
	if ((buf[0] == '\0') || (end == NULL) || (*end != '\n')) {
		LL_LOG(ERR, EAL, "%s(): cannot parse sysfs value %s\n",
				__func__, filename);
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}



// Check if a cpu is present by the presence of the cpu information for it
int
eal_cpu_detected(unsigned lcore_id) {
    char path[PATH_MAX];
    int len = snprintf(path , sizeof(path), SYS_CPU_DIR "/" CORE_ID_FILE, lcore_id);

	if (len <= 0 || (unsigned)len >= sizeof(path))
		return 0;
	if (access(path, F_OK) != 0)
		return 0;

	return 1;
}

/*
 * Get CPU socket id (NUMA node) for a logical core.
 *
 * This searches each nodeX directories in /sys for the symlink for the given
 * lcore_id and returns the numa node where the lcore is found. If lcore is not
 * found on any numa node, returns zero.
 */
unsigned
eal_cpu_socket_id(unsigned lcore_id)
{
	unsigned socket;

	for (socket = 0; socket < LL_MAX_NUMA_NODES; socket++) {
		char path[PATH_MAX];

		snprintf(path, sizeof(path), "%s/node%u/cpu%u", NUMA_NODE_PATH,
				socket, lcore_id);
		if (access(path, F_OK) == 0)
			return socket;
	}
	return 0;
}

/* Get the cpu core id value from the /sys/.../cpuX core_id value */
unsigned
eal_cpu_core_id(unsigned lcore_id)
{
	char path[PATH_MAX];
	unsigned long id;

	int len = snprintf(path, sizeof(path), SYS_CPU_DIR "/%s", lcore_id, CORE_ID_FILE);
	if (len <= 0 || (unsigned)len >= sizeof(path))
		goto err;
	if (eal_parse_sysfs_value(path, &id) != 0)
		goto err;
	return (unsigned)id;

err:
	LL_LOG(ERR, EAL, "Error reading core id value from %s "
			"for lcore %u - assuming core 0\n", SYS_CPU_DIR, lcore_id);
	return 0;
}



static int
socket_id_cmp(const void *a, const void *b)
{
	const int *lcore_id_a = a;
	const int *lcore_id_b = b;

	if (*lcore_id_a < *lcore_id_b)
		return -1;
	if (*lcore_id_a > *lcore_id_b)
		return 1;
	return 0;
}


/*
 * Parse /sys/devices/system/cpu to get the number of physical and logical
 * processors on the machine. The function will fill the cpu_info
 * structure.
 */
int
ll_eal_cpu_init(void) {

    struct ll_config *config = ll_eal_get_configuration();
    unsigned lcore_id;
	unsigned count = 0;
	unsigned int socket_id, prev_socket_id;
	int lcore_to_socket_id[LL_MAX_LCORE];

    for (lcore_id = 0; lcore_id < LL_MAX_LCORE; lcore_id++) {
        lcore_config[lcore_id].core_index = count;

        //init cpuset for per lcore config
        CPU_ZERO(&lcore_config[lcore_id].cpuset);

        //find socket first
        socket_id = eal_cpu_core_id(lcore_id);
        lcore_to_socket_id[lcore_id] = socket_id;

        if (eal_cpu_detected(lcore_id) == 0) {
            config->lcore_role[lcore_id] = ROLE_OFF;
            lcore_config[lcore_id].core_index = -1;
            continue;
        }

        /* By default, lcore 1:1 map to cpu id */
		CPU_SET(lcore_id, &lcore_config[lcore_id].cpuset);

		/* By default, each detected core is enabled */
		config->lcore_role[lcore_id] = ROLE_LL;
		lcore_config[lcore_id].core_role = ROLE_LL;
		lcore_config[lcore_id].core_id = eal_cpu_core_id(lcore_id);
		lcore_config[lcore_id].socket_id = socket_id;
		LL_LOG(DEBUG, EAL, "Detected lcore %u as "
				"core %u on socket %u\n",
				lcore_id, lcore_config[lcore_id].core_id,
				lcore_config[lcore_id].socket_id);
		count++;
    }

    for (; lcore_id < CPU_SETSIZE; lcore_id++) {
		if (eal_cpu_detected(lcore_id) == 0)
			continue;
		LL_LOG(DEBUG, EAL, "Skipped lcore %u as core %u on socket %u\n",
			lcore_id, eal_cpu_core_id(lcore_id),
			eal_cpu_socket_id(lcore_id));
	}

    /* Set the count of enabled logical cores of the EAL configuration */
	config->lcore_count = count;
	LL_LOG(DEBUG, EAL,
			"Maximum logical cores by configuration: %u\n",
			LL_MAX_LCORE);
	LL_LOG(INFO, EAL, "Detected CPU lcores: %u\n", config->lcore_count);

	/* sort all socket id's in ascending order */
	qsort(lcore_to_socket_id, LL_DIM(lcore_to_socket_id),
			sizeof(lcore_to_socket_id[0]), socket_id_cmp);

	prev_socket_id = -1;
	config->numa_node_count = 0;
	for (lcore_id = 0; lcore_id < LL_MAX_LCORE; lcore_id++) {
		socket_id = lcore_to_socket_id[lcore_id];
		if (socket_id != prev_socket_id)
			config->numa_nodes[config->numa_node_count++] =
					socket_id;
		prev_socket_id = socket_id;
	}
	LL_LOG(INFO, EAL, "Detected NUMA nodes: %u\n", config->numa_node_count);

	return 0;
}
