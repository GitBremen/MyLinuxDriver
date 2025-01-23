#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
/*gpio子系统
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
*/
#include <linux/spi/spi.h>

#define CNF1 0X2A
#define CNF2 0X29
#define CNF3 0X28
#define RXB0CTRL 0X60
#define CANINTE 0X2B
#define CANCTRL 0X0F
#define CANSTAT 0X0E
#define TXB0CTRL 0X30
#define CANINTF 0X2C

struct mcp2515
{
    dev_t dev_num;
    struct cdev mcp2515_cdev;
    struct class *mcp2515_class;
    struct device *mcp2515_device;
};

static struct mcp2515 mcp2515_dev;
struct spi_device *spi_mcp2515;

static void mcp2515_reset(void)
{
    int ret;
    const uint8_t reset = 0xc0;
    ret = spi_write(spi_mcp2515, &reset, 1);
    if (ret < 0)
    {
        printk("mcp2515 reset error!\n");
        return;
    }
}

static uint8_t mcp2515_read_reg(uint8_t addr)
{
    int ret;
    uint8_t val = 0;
    uint8_t instruction[] = {0x03, addr};
    ret = spi_write_then_read(spi_mcp2515, instruction, sizeof(instruction), &val, 1);
    if (ret < 0)
    {
        printk("get mcp2515 status error!\n");
        return ret;
    }
    return val;
}

static int mcp2515_write_reg(uint8_t addr, uint8_t val)
{
    int ret;
    const uint8_t reg[] = {0x02, addr, val};
    ret = spi_write(spi_mcp2515, reg, sizeof(reg));
    if (ret < 0)
    {
        printk("mcp2515_write_reg error!\n");
        return ret;
    }
    return 0;
}

static int mcp2515_bit_modify(uint8_t addr, uint8_t mask, uint8_t val)
{
    int ret;
    const uint8_t reg[] = {0x05, addr, mask, val}; // 0x05表示位修改操作
    ret = spi_write(spi_mcp2515, reg, sizeof(reg));
    if (ret < 0)
    {
        printk("mcp2515_bit_modify error!\n");
        return ret;
    }
    return 0;
}

static int mcp2515_change_mode(uint8_t mode)
{
    int ret;
    // uint8_t reg;
    // reg = mcp2515_read_reg(CANCTRL);
    // reg &= 0xe0;
    // reg |= mode;
    // ret = mcp2515_write_reg(CANCTRL, reg);
    ret = mcp2515_bit_modify(CANCTRL, 0xe0, mode);
    if (ret < 0)
    {
        printk("mcp2515_change_mode error!\n");
        return ret;
    }
    return 0;
}

static int mcp2515_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int mcp2515_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t mcp2515_read(struct file *flip, char __user *buf,
                            size_t size, loff_t *offset)
{
    int ret;
    int i;
    uint8_t kbuf[13] = {0};

    while (!(mcp2515_read_reg(CANINTF) && (1 << 0)))
        ; // 等待接收到数据

    for (i = 0; i < sizeof(kbuf); ++i)
    {
        kbuf[i] = mcp2515_read_reg(0x61 + i);
    }

    mcp2515_bit_modify(CANINTF, 0X01, 0X00); // 清除接收中断标志位
    if ((ret = copy_to_user(buf, kbuf, size)) < 0)
    {
        printk("copy_to_user error!\n");
        return ret;
    }

    return size;
}

static ssize_t mcp2515_write(struct file *flip, const char __user *buf,
                             size_t size, loff_t *offset)
{
    int ret;
    int i;
    uint8_t kbuf[13] = {0};
    mcp2515_bit_modify(TXB0CTRL, 0x03, 0x03); // 设置发送请求,优先级最高
    ret = copy_from_user(kbuf, buf, size);
    if (ret)
    {
        printk("copy_from_user error!\n");
        return ret;
    }

    for (i = 0; i < sizeof(kbuf); ++i)
    {
        ret = mcp2515_write_reg(0x31 + i, kbuf[i]);
        if (ret < 0)
        {
            printk("mcp2515_write_reg %x error!\n", 0x31 + i);
            return ret;
        }
    }

    mcp2515_bit_modify(TXB0CTRL, 0x08, 0x08); // 启动相应缓冲器的报文发送
    while (!(mcp2515_read_reg(CANINTF) && (1 << 2)))
        ;                                    // 等待发送完成
    mcp2515_bit_modify(CANINTF, 0X04, 0X00); // 清除发送完成中断标志位

    return size;
}

const struct file_operations mcp2515_fops = {
    .open = mcp2515_open,
    .write = mcp2515_write,
    .read = mcp2515_read,
    .release = mcp2515_release,
    .owner = THIS_MODULE,
};

static int mcp2515_remove(struct spi_device *spi)
{
    printk("mcp2515_remove  function\n");
    return 0;
}

static int mcp2515_probe(struct spi_device *spi)
{
    int ret;
    uint8_t value;
    printk("mcp2515_probe  function\n");

    spi_mcp2515 = spi;

    if (0 > (ret = alloc_chrdev_region(&mcp2515_dev.dev_num, 0, 1, "mcp2515")))
    {
        printk("alloc_chrdev_region error!\n");
        goto err_alloc_chrdev_region;
    }

    cdev_init(&mcp2515_dev.mcp2515_cdev, &mcp2515_fops);
    mcp2515_dev.mcp2515_cdev.owner = THIS_MODULE;

    if (0 > (ret = cdev_add(&mcp2515_dev.mcp2515_cdev, mcp2515_dev.dev_num, 1)))
    {
        printk("cdev_add error!\n");
        goto err_cdev_add;
    }

    mcp2515_dev.mcp2515_class = class_create(THIS_MODULE, "SPI_TO_CAN");
    if (IS_ERR(mcp2515_dev.mcp2515_class))
    {
        printk("class_create error!\n");
        ret = PTR_ERR(mcp2515_dev.mcp2515_class);
        goto err_class_create;
    }

    mcp2515_dev.mcp2515_device = device_create(mcp2515_dev.mcp2515_class, NULL,
                                               mcp2515_dev.dev_num, NULL, "mcp2515");
    if (IS_ERR(mcp2515_dev.mcp2515_device))
    {
        printk("device_create error!\n");
        ret = PTR_ERR(mcp2515_dev.mcp2515_device);
        goto err_device_create;
    }

    mcp2515_reset();
    value = mcp2515_read_reg(CANSTAT);
    printk("value is %x\n", value);

    mcp2515_write_reg(CNF1, 0X01);
    mcp2515_write_reg(CNF2, 0XB1);
    mcp2515_write_reg(CNF3, 0X05);

    mcp2515_bit_modify(RXB0CTRL, 0X64, 0X60); // 接收缓冲器0接收任何标准帧
    mcp2515_write_reg(CANINTE, 0X05);         // 使能接收中断

    mcp2515_change_mode(0x40); // 设置为回环模式
    value = mcp2515_read_reg(CANSTAT);
    printk("value is %x\n", value);

    return 0;

err_device_create:
    class_destroy(mcp2515_dev.mcp2515_class);
    return ret;
err_class_create:
    cdev_del(&mcp2515_dev.mcp2515_cdev);
    return ret;
err_cdev_add:
    unregister_chrdev_region(mcp2515_dev.dev_num, 1);
    return ret;
err_alloc_chrdev_region:
    return ret;
}

const struct of_device_id mcp2515_of_match_table[] = {
    {.compatible = "my-mcp2515"},
    {}};

const struct spi_device_id mcp2515_id_table[] = {
    {.name = "my-mcp2515"},
    {}};

static struct spi_driver mcp2515_driver = {
    .remove = mcp2515_remove,
    .probe = mcp2515_probe,
    .driver = {
        .owner = THIS_MODULE,
        .name = "my-mcp2515",
        .of_match_table = mcp2515_of_match_table,
    },
    .id_table = mcp2515_id_table,
};

static int __init mcp2515_driver_init(void)
{
    int ret = spi_register_driver(&mcp2515_driver);
    if (ret < 0)
    {
        printk("spi_register_driver failed!\n");
        return ret;
    }
    return 0;
}

static void __exit mcp2515_driver_exit(void)
{
    device_destroy(mcp2515_dev.mcp2515_class, mcp2515_dev.dev_num);
    class_destroy(mcp2515_dev.mcp2515_class);
    cdev_del(&mcp2515_dev.mcp2515_cdev);
    unregister_chrdev_region(mcp2515_dev.dev_num, 1);
    spi_unregister_driver(&mcp2515_driver);
}

module_init(mcp2515_driver_init);
module_exit(mcp2515_driver_exit);
MODULE_LICENSE("GPL");
