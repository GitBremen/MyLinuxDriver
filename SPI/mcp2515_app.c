#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int fd;
    int i;
    int ret;
    u_int8_t w_buf[13] = {0x66, 0x08, 0x22, 0x33, 0x08, 0x01, 0x02, 0x03,
                          0x04, 0x05, 0x06, 0x07, 0x08};

    u_int8_t r_buf[13] = {0};

    fd = open("/dev/mcp2515", O_RDWR);
    if (fd < 0)
    {
        perror("open");
        return -1;
    }

    ret = write(fd, w_buf, sizeof(w_buf));
    if (ret < 0)
    {
        perror("write");
        return -1;
    }
    ret = read(fd, r_buf, sizeof(r_buf));
    if (ret < 0)
    {
        perror("read");
        return -1;
    }

    for (i = 0; i < sizeof(r_buf); i++)
    {
        printf("r_buf[%d] = %x\n", i, r_buf[i]);
    }

    close(fd);

    return 0;
}
