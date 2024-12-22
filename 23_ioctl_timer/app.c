#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define TIMER_CLOSE_CMD     _IO('T',0)
#define TIMER_OPEN_CMD      _IO('T',1)
#define TIMER_SET_CMD       _IOW('T',2,int)


int main(int argc ,char *argv[])
{
    int fd;
   
    int val;
    
    fd = open("/dev/device_node_test",O_RDWR);

    if(fd<0){
        perror("open error\n");
        return -1;
    }
   
        // ioctl(fd,CMD_TEST1,&test);
        // sleep(1);
        // ioctl(fd,CMD_TEST2,&val);
        // printf("val is %d\n",val);
    
    ioctl(fd,TIMER_SET_CMD,1000);
    ioctl(fd,TIMER_OPEN_CMD);
    sleep(3);
    ioctl(fd,TIMER_SET_CMD,3000);
    sleep(7);
    ioctl(fd,TIMER_CLOSE_CMD);

    close(fd);

    return 0;
}