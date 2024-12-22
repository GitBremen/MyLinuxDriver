#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc ,char *argv[])
{
    int fd;

    char buf1[64] = {0};
    char buf2[64] = "hello world!";

    fd = open("/dev/device_node_test",O_RDWR | O_NONBLOCK);
    if(fd < 0){
        perror("open device failed|\n");
        return fd;
    }

    printf("write start\n");
    write(fd,buf2,strlen(buf2));
    printf("write end\n");

    close(fd);

    return 0;
}