#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

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
    fd = open("/dev/ds18b20_device", O_RDWR);
    if (fd < 0)
    {
        printf("open ds18b20 device failed!\n");
        return -EINVAL;
    }

    while (1)
    {
        read(fd, &data, sizeof(data));
        ds18b20_get_temp(data);
    }
    return 0;
}