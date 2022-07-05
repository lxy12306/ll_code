#include <limits.h>
#include <string.h>

#include <ll_config.h>
#include <ll_mem_config.h>
#include <ll_errno.h>


//early configuration structure, when memory config is not mmaped
static struct ll_mem_config early_mem_config;

// address of global and public configuration
static struct ll_config ll_config = {
	.mem_config = &early_mem_config,
};

/* platform-specific runtime dir */
static char runtime_dir[PATH_MAX];

/* internal configuration */
static struct internal_config internal_config = {
	.match_allocations = 0,
};

LL_INIT_PRIO(cfg_init,CFG) {
	ll_config.iova_mode = LL_IOVA_PA;
}

LL_INIT_PRIO(mcfg_init,MCFG) {
	ll_rwlock_init(&(early_mem_config.mplock));
}



/* Return a pointer to the configuration structure */
struct ll_config *
ll_eal_get_configuration(void)
{
	return &ll_config;
}

/* Return a pointer to the internal configuration structure */
struct internal_config *
ll_eal_get_internal_configuration(void)
{
	return &internal_config;
}

const char *
ll_eal_get_runtime_dir(void) {
	return runtime_dir;
}

enum ll_proc_type_t
ll_eal_process_type(void)
{
	return ll_config.process_type;
}

int
ll_eal_has_hugepages(void)
{
	return 0;
}


enum ll_iova_mode
ll_eal_iova_mode(void)
{
	return ll_eal_get_configuration()->iova_mode;
}

/*** lcore ***/

unsigned int
ll_socket_count(void) {
    const struct ll_config *config = ll_eal_get_configuration();
    return config->numa_node_count;
}

int
ll_socket_id_by_idx(unsigned int idx)
{
	const struct ll_config *config = ll_eal_get_configuration();
	if (idx >= config->numa_node_count) {
		ll_errno = EINVAL;
		return -1;
	}
	return config->numa_nodes[idx];
}


LL_DEFINE_PER_LCORE(unsigned, _lcore_id) = LL_LCORE_ID_ANY;
LL_DEFINE_PER_LCORE(int, _thread_id) = -1;