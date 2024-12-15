#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>

#define SET_RATIO _IOW('A', 0, int)

void ds18b20_get_temp(int value)
{
    char sig;
    float temp;
    if ((value >> 11) & 0x01)
    {
        sig = '-';
        value = ~value + 1;
        value &= ~(0xf0 << 8); // 清除符号位
    }
    else
    {
        sig = '+';
    }
    temp = value * 0.0625;
    printf("temp is %c%.4f\r\n", sig, temp);
}

int main(int argc, char *argv[])
{
    int fd;
    int data;
    int ratio;
    fd = open("/dev/ds18b20_device", O_RDWR);
    if (fd < 0)
    {
        printf("open ds18b20 device failed!\n");
        return -EINVAL;
    }

    ratio = atoi(argv[1]);
    printf("ratio is %d\n", ratio);
    if (ratio < 9 || ratio > 12)
    {
        printf("ratio arg is illeagle\n");
        return -2;
    }
    ioctl(fd, SET_RATIO, ratio);

    while (1)
    {
        read(fd, &data, sizeof(data));
        ds18b20_get_temp(data);
        sleep(1);
    }
    return 0;
}