#ifndef _LL_MALLOC_H_
#define _LL_MALLOC_H_

struct ll_memseg_list;

bool
eal_memalloc_is_contig(const struct ll_memseg_list *msl, void *start, size_t len);


void *
malloc_socket(const char *type, size_t size, unsigned align,
    int socket_arg, const bool trace_ena);

#endif //_LL_MALLOC_H_