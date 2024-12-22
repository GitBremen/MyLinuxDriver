#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc ,char *argv[])
{
    int fd;
    unsigned int count=0;

    fd = open("/dev/device_node_test",O_RDWR);
    // if(fd != 0){
    //     perror("open device failed\n");
    //     return fd;
    // }

    while(1){
        read(fd,&count,sizeof(count));
        printf("count is %d\n",count);
        
        sleep(1);
    }
    
    close(fd);

    return 0;
}