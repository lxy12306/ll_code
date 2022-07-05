#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>

#include "perf.h"

int main()
{

    ll_arch_perf_init(NULL);

    struct perf_event_attr attr;
    memset(&attr,0,sizeof(struct perf_event_attr));
    attr.size=sizeof(struct perf_event_attr);
    //监测硬件
    attr.type=PERF_TYPE_HARDWARE;
    //监测指令数
    attr.config=PERF_COUNT_HW_INSTRUCTIONS;
    //初始状态为禁用
    attr.disabled=1;
    //创建perf文件描述符，其中pid=0,cpu=-1表示监测当前进程，不论运行在那个cpu上
    int fd=sys_perf_event_open(&attr,0,-1,-1,0);
    if(fd<0)
    {
        perror("Cannot open perf fd!");
        return 1;
    }
    //启用（开始计数）
    ioctl(fd,PERF_EVENT_IOC_ENABLE,0);
    while(1)
    {
        uint64_t instructions;
        //读取最新的计数值
        read(fd,&instructions,sizeof(instructions));
        printf("instructions=%ld\n",instructions);
        sleep(1);
    }
}