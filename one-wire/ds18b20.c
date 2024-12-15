#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#define SET_RATIO _IOW('A', 0, int)
#define SET_RATIO_9 9
#define SET_RATIO_10 10
#define SET_RATIO_11 11
#define SET_RATIO_12 12

struct ds18b20_struct
{
    dev_t dev_num;
    struct cdev ds18b20_cdev;
    struct class *class;
    struct device *device;
    struct gpio_desc *ds18b20_gpio;
    int major;
    int minor;
    char kbuf[64];
};

struct ds18b20_struct *ds18b20;

static void ds18b20_reset(void)
{
    gpiod_direction_output(ds18b20->ds18b20_gpio, 1);
    gpiod_set_value(ds18b20->ds18b20_gpio, 0);
    udelay(600);
    gpiod_set_value(ds18b20->ds18b20_gpio, 1);
    // udelay(20);

    gpiod_direction_input(ds18b20->ds18b20_gpio);
    while (gpiod_get_value(ds18b20->ds18b20_gpio))
        ; //?
    while (!gpiod_get_value(ds18b20->ds18b20_gpio))
        ;
    udelay(480);
}

static void ds18b20_writebit(unsigned char bit)
{
    gpiod_direction_output(ds18b20->ds18b20_gpio, 1);
    gpiod_set_value(ds18b20->ds18b20_gpio, 0);
    if (bit)
    {
        udelay(5); // 主机拉低总线1us-15us
        gpiod_direction_output(ds18b20->ds18b20_gpio, 1);
    }
    udelay(70);                                       // 主机拉低总线保持60-120us,写0
    gpiod_direction_output(ds18b20->ds18b20_gpio, 1); // 改为input变成电阻上拉
    udelay(3);                                        // 两次写操作之间的间隔至少为1us
}

static void ds18b20_writebyte(int byte) // 低位先行
{
    int i = 0;
    for (i = 0; i < 8; i++)
    {
        ds18b20_writebit(byte & (0x01 << i));
    }
}

static unsigned char ds18b20_readbit(void)
{
    unsigned char bit;
    gpiod_direction_output(ds18b20->ds18b20_gpio, 1);
    gpiod_set_value(ds18b20->ds18b20_gpio, 0);
    udelay(2);

    gpiod_direction_input(ds18b20->ds18b20_gpio);
    udelay(10);

    bit = gpiod_get_value(ds18b20->ds18b20_gpio);
    udelay(70);
    return bit;
}

static int ds18b20_readbyte(void)
{
    int byte;
    int i = 0;
    for (i = 0; i < 8; i++)
    {
        byte |= ds18b20_readbit() << i; // 低位先行
    }
    return byte;
}

static int ds18b20_readtemp(void)
{
    int tmpH, tmpL, temp;
    ds18b20_reset();
    ds18b20_writebyte(0xcc);
    ds18b20_writebyte(0x44);
    mdelay(750);

    ds18b20_reset();
    ds18b20_writebyte(0xcc);
    ds18b20_writebyte(0xbe);

    tmpL = ds18b20_readbyte();
    tmpH = ds18b20_readbyte();

    temp = tmpH << 8;
    temp |= tmpL;

    return temp;
}

static int ds18b20_open(struct inode *inode, struct file *filp)
{
    return 0;
}
static int ds18b20_release(struct inode *inode, struct file *flip)
{
    return 0;
}
static ssize_t ds18b20_read(struct file *flip, char __user *buf, size_t size, loff_t *off)
{
    int ds18b20_temp;
    // float realtemp;  //非必要尽量不要在驱动中进行浮点数操作
    ds18b20_temp = ds18b20_readtemp();
    // if ((ds18b20_temp >> 11) & 0x01) // if it is negative
    // {
    //     ds18b20_temp = ~ds18b20_temp + 1;
    //     ds18b20_temp &= (0xf8 << 8); // 符号位清零
    // }
    if (copy_to_user(buf, &ds18b20_temp, sizeof(ds18b20_temp)))
    {
        printk("ds18b20 read temp error\n");
        return -1;
    }

    return 0;
}

static long ds18b20_ioctl(struct file *flip, unsigned int cmd, unsigned long args)
{
    int arg;
    if (cmd == SET_RATIO)
    {
        if (copy_from_user(&arg, (int *)args, sizeof(int)) != 0)
        {
            printk("set ratio failed due to copy_from_user!\n");
            return -1;
        }
        if (arg >= SET_RATIO_9 && arg <= SET_RATIO_12)
        {
            ds18b20_reset();
            ds18b20_writebyte(0xcc);
            ds18b20_writebyte(0x4E);

            ds18b20_writebyte(60);
            ds18b20_writebyte(0); // 温度报警上下阈值
            switch (arg)
            {
            case SET_RATIO_9:
                ds18b20_writebyte(0x1f);
                break;
            case SET_RATIO_10:
                ds18b20_writebyte(0x3f);
                break;
            case SET_RATIO_11:
                ds18b20_writebyte(0x5f);
                break;
            case SET_RATIO_12:
                ds18b20_writebyte(0x7f);
                break;
            default:
                break;
            }

            /*检查设置是否正确*/
            ds18b20_reset();
            ds18b20_writebyte(0xcc);
            ds18b20_writebyte(0xBE);

            ds18b20_readbyte();
            ds18b20_readbyte();
            ds18b20_readbyte();
            ds18b20_readbyte();

            switch (arg)
            {
            case SET_RATIO_9:
                if (ds18b20_readbyte() != 0x1f)
                {
                    printk("set rartio 9 failed\n");
                    return -1;
                }
                break;
            case SET_RATIO_10:
                if (ds18b20_readbyte() != 0x3f)
                {
                    printk("set rartio 10 failed\n");
                    return -1;
                }
                break;
            case SET_RATIO_11:
                if (ds18b20_readbyte() != 0x5f)
                {
                    printk("set rartio 11 failed\n");
                    return -1;
                }
                break;
            case SET_RATIO_12:
                if (ds18b20_readbyte() != 0x7f)
                {
                    printk("set rartio 12 failed\n");
                    return -1;
                }
                break;
            default:
                break;
            }
        }
        else
        {
            printk("set ratio illeagle\n");
            return -2;
        }
    }
    return 0;
}

const struct file_operations ds18b20_fops = {
    .owner = THIS_MODULE,
    .open = ds18b20_open,
    .release = ds18b20_release,
    .read = ds18b20_read,
    .unlocked_ioctl = ds18b20_ioctl,
};

static int
ds18b20_probe(struct platform_device *dev)
{
    int ret = 0;
    printk("ds18b20_probe function\n");

    ds18b20 = kzalloc(sizeof(*ds18b20), GFP_KERNEL);

    if (ds18b20 == NULL)
    {
        ret = -ENOMEM;
        goto err_kzalloc;
    }
    printk("kzalloc func success!\n");

    ret = alloc_chrdev_region(&ds18b20->dev_num, 0, 1, "ds18b20");
    if (ret != 0)
        goto err_alloc_chrdev_region;
    printk("alloc_chrdev_region func success!\n");

    cdev_init(&ds18b20->ds18b20_cdev, &ds18b20_fops);

    ds18b20->ds18b20_cdev.owner = THIS_MODULE; // 不加上卸载驱动程序可能不成功

    ret = cdev_add(&ds18b20->ds18b20_cdev, ds18b20->dev_num, 1);
    if (ret < 0)
        goto err_cdev_add;
    printk("cdev_add func success!\n");

    ds18b20->class = class_create(THIS_MODULE, "onw-wire");
    if (IS_ERR(ds18b20->class))
    {
        ret = PTR_ERR(ds18b20->class);
        goto err_class_create;
    }
    printk("class_create func success!\n");

    ds18b20->device = device_create(ds18b20->class, NULL,
                                    ds18b20->dev_num, NULL, "ds18b20_device");
    if (IS_ERR(ds18b20->device))
    {
        ret = PTR_ERR(ds18b20->device);
        goto err_device_create;
    }
    printk("device_create func success!\n");

    ds18b20->ds18b20_gpio = gpiod_get_optional(&dev->dev, "ds18b20", GPIOD_ASIS);
    if (ds18b20->ds18b20_gpio == NULL)
    {
        ret = -EBUSY;
        goto err_gpiod_get_optional;
    }
    gpiod_direction_output(ds18b20->ds18b20_gpio, 1);

    return 0;
err_gpiod_get_optional:
    device_destroy(ds18b20->class, ds18b20->dev_num);
err_device_create:
    class_destroy(ds18b20->class);
    printk("device_create func err!\n");
err_class_create:
    cdev_del(&ds18b20->ds18b20_cdev);
    printk("class_create func err!\n");
err_cdev_add:
    unregister_chrdev_region(ds18b20->dev_num, 1);
    printk("cdev_add func err!\n");
err_alloc_chrdev_region:
    kfree(ds18b20);
    printk("alloc_chrdev_region func err!\n");
err_kzalloc:
    printk("kzalloc func err!\n");
    return ret;
}

static int ds18b20_remove(struct platform_device *dev)
{
    printk("ds18b20_remove  function\n");
    return 0;
}

const struct of_device_id of_match_table_ds18b20[] = {
    {
        .compatible = "ds18b20",
    },
    {},
};

static struct platform_driver ds18b20_driver = {
    .probe = ds18b20_probe,
    .remove = ds18b20_remove,
    .driver = {
        .name = "ds18b20",
        .owner = THIS_MODULE,
        .of_match_table = of_match_table_ds18b20,
    },
};

static int __init ds18b20_driver_init(void)
{
    int ret = platform_driver_register(&ds18b20_driver);
    if (ret < 0)
    {
        printk("ds18b20_driver_init err\n");
        return -1;
    }
    printk("ds18b20_driver_init success\n");

    return 0;
}

static void __exit ds18b20_driver_exit(void)
{
    gpiod_put(ds18b20->ds18b20_gpio);
    device_destroy(ds18b20->class, ds18b20->dev_num);
    class_destroy(ds18b20->class);
    cdev_del(&ds18b20->ds18b20_cdev);
    unregister_chrdev_region(ds18b20->dev_num, 1);
    kfree(ds18b20);
    platform_driver_unregister(&ds18b20_driver);
    printk("ds18b20_driver_exit success\n");
}

module_init(ds18b20_driver_init);
module_exit(ds18b20_driver_exit);
MODULE_LICENSE("GPL");