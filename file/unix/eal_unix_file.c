
#include <unistd.h>
#include <sys/file.h>

#include <ll_errno.h>
#include <ll_log.h>

#include "../private/eal_private.h"

int
eal_file_lock(int fd, enum eal_flock_op op, enum eal_flock_mode mode)
{
	int sys_flags = 0;
	int ret;

	if (mode == EAL_FLOCK_RETURN)
		sys_flags |= LOCK_NB;

	switch (op) {
	case EAL_FLOCK_EXCLUSIVE:
		sys_flags |= LOCK_EX;
		break;
	case EAL_FLOCK_SHARED:
		sys_flags |= LOCK_SH;
		break;
	case EAL_FLOCK_UNLOCK:
		sys_flags |= LOCK_UN;
		break;
	}

	ret = flock(fd, sys_flags);
	if (ret)
		ll_errno = errno;

	return ret;
}

int
ll_file_exists(const char *f) {
	return (access(f, F_OK) != -1);
}


ssize_t ll_file_read_from_fd(int fd, uint8_t *buf, size_t file_sz) {
	size_t read_sz = 0;
    while (read_sz < file_sz) {
        ssize_t sz = read(fd, &buf[read_sz], file_sz - read_sz);
        if (sz == 0) {
            break;
        }
        if (sz < 0) {
            return -1;
        }
        read_sz += sz;
    }
    return (ssize_t)read_sz;
}


ssize_t ll_file_read_from_file(const char* file_name, uint8_t* buf, size_t file_max_sz) {
    int fd = open(file_name, O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
		LL_LOG(DEBUG, FILE, "Can not open file %s", file_name);
        return -1;
    }

    ssize_t read_sz = ll_file_read_from_fd(fd, buf, file_max_sz);
    if (read_sz < 0) {
		LL_LOG(DEBUG, FILE, "Can not read data from %s", file_name);
    }

    close(fd);

    LL_LOG(DEBUG, FILE, "Read %zu bytes (%zu requested) from '%s'", (size_t)read_sz, file_max_sz, file_name);
    return read_sz;
}


