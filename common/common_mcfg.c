#include <ll_common.h>
#include <ll_config.h>
#include <ll_mem_config.h>
#include <ll_rwlock.h>

struct ll_mem_config *
ll_get_mem_config(void) {
	return ll_eal_get_configuration()->mem_config;
}

void
ll_mcfg_mem_read_lock(void) {
	struct ll_mem_config * mcfg = ll_get_mem_config();
	ll_rwlock_read_lock(&mcfg->memory_hotplug_lock);
	return;
}

void
ll_mcfg_mem_read_unlock(void) {
	struct ll_mem_config * mcfg = ll_get_mem_config();
	ll_rwlock_read_unlock(&mcfg->memory_hotplug_lock);
	return;
}

void
ll_mcfg_mem_write_lock(void) {
	struct ll_mem_config * mcfg = ll_get_mem_config();
	ll_rwlock_write_lock(&mcfg->memory_hotplug_lock);
	return;
}

void
ll_mcfg_mem_write_unlock(void) {
	struct ll_mem_config * mcfg = ll_get_mem_config();
	ll_rwlock_write_unlock(&mcfg->memory_hotplug_lock);
	return;
}

void
ll_mcfg_mempool_read_lock(void) {
	struct ll_mem_config *mcfg = ll_get_mem_config();
	ll_rwlock_read_lock(&mcfg->mplock);
}


void
ll_mcfg_mempool_read_unlock(void) {
	struct ll_mem_config *mcfg = ll_get_mem_config();
	ll_rwlock_read_unlock(&mcfg->mplock);
}

void
ll_mcfg_mempool_write_lock(void) {
	struct ll_mem_config *mcfg = ll_get_mem_config();
	ll_rwlock_write_lock(&mcfg->mplock);
}

void
ll_mcfg_mempool_write_unlock(void) {
	struct ll_mem_config *mcfg = ll_get_mem_config();
	ll_rwlock_write_unlock(&mcfg->mplock);
}

enum ll_iova_mode
ll_mcfg_iova_mode(void)
{
	return ll_eal_iova_mode();
}