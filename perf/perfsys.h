#ifndef _PERF_SYS_H
#define _PERF_SYS_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>

struct perf_event_attr;

static inline int
sys_perf_event_open(struct perf_event_attr *attr,
		      pid_t pid, int cpu, int group_fd,
		      unsigned long flags)
{
	return syscall(__NR_perf_event_open, attr, pid, cpu,
		       group_fd, flags);
}

static inline int
sys_perf_event_enable(int fd) {
	return ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
}

static inline int
sys_perf_event_disable(int fd) {
	return ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
}



#endif /* _PERF_SYS_H */