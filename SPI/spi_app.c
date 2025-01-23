#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <string.h>

int mode = SPI_MODE_0;
int fd;
int bits = 8;
int speed = 24000000;
int delay = 0;

int spi_init()
{
    int ret;
    fd = open("/dev/spidev0.0", O_RDWR); // 此处要修改设备树文件中的compatible属性
    if (fd < 0)
    {
        perror("open");
        return -1;
    }

    /*
     * spi mode
     */
    ret = ioctl(fd, SPI_IOC_WR_MODE32, &mode);
    if (ret == -1)
    {
        printf("can't set spi mode");
        return ret;
    }

    ret = ioctl(fd, SPI_IOC_RD_MODE32, &mode);
    if (ret == -1)
    {
        printf("can't get spi mode");
        return ret;
    }

    /*
     * bits per word
     */
    ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret == -1)
    {
        printf("can't set bits per word");
        return ret;
    }

    ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret == -1)
    {
        printf("can't get bits per word");
        return ret;
    }

    /*
     * max speed hz
     */
    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret == -1)
    {
        printf("can't set max speed hz");
        return ret;
    }

    ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (ret == -1)
    {
        printf("can't get max speed hz");
        return ret;
    }

    printf("spi mode: 0x%x\n", mode);
    printf("bits per word: %d\n", bits);
    printf("max speed: %d Hz (%d KHz)\n", speed, speed / 1000);

    return 0;
}

int spi_transfer(int fd, u_int8_t *tx, u_int8_t *rx, int len)
{
    int ret;
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = len,
        .delay_usecs = delay,
        .speed_hz = speed,
        .bits_per_word = bits,
    };

    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1)
    {
        printf("can't send spi message");
        return ret;
    }

    return 0;
}

#define CANSTAT 0X0E
#define READ 0X03
#define RESET 0XC0
#define CANCTRL 0X0F
#define WRITE 0X02

int main(int argc, char *argv[])
{
    u_int8_t reset_command = RESET;
    u_int8_t read_command[] = {READ, CANSTAT};
    u_int8_t canstat[4] = {0};
    u_int8_t write_command[] = {WRITE, CANCTRL, 0X00};

    if (spi_init())
    {
        printf("spi_init error!\n");
        return -1;
    }

    spi_transfer(fd, &reset_command, NULL, 1);

    spi_transfer(fd, read_command, canstat, sizeof(canstat));
    printf("canstat is %x\n", canstat[2]);

    memset(canstat, 0, sizeof(canstat));

    spi_transfer(fd, write_command, NULL, sizeof(write_command));
    spi_transfer(fd, read_command, canstat, sizeof(canstat));
    printf("canstat is %x\n", canstat[3]);

    return 0;
}