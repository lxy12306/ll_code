#ifndef _EAL_MEMOEY_PRIVATE_H_
#define _EAL_MEMOEY_PRIVATE_H_

int
eal_dynmem_memseg_lists_init(void);

int
eal_legacy_hugepage_init(void);

static inline int
eal_dynmem_hugepage_init(void) {
    return 0;
}

#endif //_EAL_MEMOEY_PRIVATE_H_