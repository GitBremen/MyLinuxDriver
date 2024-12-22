#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>

int main(int argc ,char *argv[])
{
    int fd;
    int ret;
    struct pollfd pll;
    char buf1[64] = {0};
    char buf2[64] = "hello world!";  

    fd = open("/dev/device_node_test",O_RDWR | O_NONBLOCK);
    if(fd < 0){
        perror("open device failed|\n");
        return fd;
    }

    pll.fd = fd;
    pll.events = POLLIN;

    printf("read start\n");
    while(1){
        ret = poll(&pll,1,5000);
        if(!ret){
            printf("time out!\n");
        }else if(pll.revents == POLLIN){
            read(fd,buf1,64);
            printf("buf is %s\n",buf1);
            sleep(1);
        }  
    }
    printf("read end\n");

    close(fd);

    return 0;
}