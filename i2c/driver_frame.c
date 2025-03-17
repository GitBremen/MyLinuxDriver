#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>

// 宏定义
#define SMPLRT_DIV 0x19
#define CONFIG 0x1A
#define GYRO_CONFIG 0x1B
#define ACCEL_CONFIG 0x1C
#define ACCEL_XOUT_H 0x3B
#define ACCEL_XOUT_L 0x3C
#define ACCEL_YOUT_H 0x3D
#define ACCEL_YOUT_L 0x3E
#define ACCEL_ZOUT_H 0x3F
#define ACCEL_ZOUT_L 0x40
#define TEMP_OUT_H 0x41
#define TEMP_OUT_L 0x42
#define GYRO_XOUT_H 0x43
#define GYRO_XOUT_L 0x44
#define GYRO_YOUT_H 0x45
#define GYRO_YOUT_L 0x46
#define GYRO_ZOUT_H 0x47
#define GYRO_ZOUT_L 0x48
#define PWR_MGMT_1 0x6B
#define WHO_AM_I 0x75
#define SlaveAddress 0xD0
#define Address 0x68 // MPU6050地址
#define I2C_RETRIES 0x0701
#define I2C_TIMEOUT 0x0702
#define I2C_SLAVE 0x0703 // IIC从器件的地址设置
#define I2C_BUS_MODE 0x0780

struct mpu6050
{
    dev_t dev_num;
    struct cdev mpu6050_cdev;
    struct class *mpu6050_class;
    struct device *mpu6050_device;
    struct i2c_client *mpu6050_client; // 保存mpu6050设备对应的i2c_client结构体，匹配成功后由.prob函数带回。
};

struct mpu6050 mpu6050_dev;

static int i2c_write_mpu6050(struct i2c_client *mpu6050_client, uint8_t address, uint8_t data)
{
    int error = 0;
    struct i2c_msg send_msg;
    uint8_t write_buf[2] = {0};
    write_buf[0] = address;
    write_buf[1] = data;

    // send_msg.addr = write_buf[0];
    // send_msg.buf = &write_buf[1];
    send_msg.addr = mpu6050_client->addr;
    send_msg.buf = write_buf;
    send_msg.len = 2;
    send_msg.flags = 0;

    /* 执行发送 */
    error = i2c_transfer(mpu6050_client->adapter, &send_msg, 1);
    if (error != 1)
    {
        printk(KERN_ALERT "\n i2c_transfer error \n");
        return -1;
    }
    return 0;
}
static int i2c_read_mpu6050(struct i2c_client *mpu6050_client, uint8_t address, void *data, uint32_t length)
{
    int error = 0;
    struct i2c_msg mpu6050_msg[2];
    uint8_t addr_data = address;
    mpu6050_msg[0].addr = mpu6050_client->addr; // mpu6050 在 iic 总线上的地址
    mpu6050_msg[0].buf = &addr_data;            // 写入的首地址
    mpu6050_msg[0].flags = 0;                   // 标记为发送数据
    mpu6050_msg[0].len = 1;                     // 写入长度

    mpu6050_msg[1].addr = mpu6050_client->addr; // mpu6050 在 iic 总线上的地址
    mpu6050_msg[1].buf = data;                  // 写入的首地址
    mpu6050_msg[1].flags = I2C_M_RD;            // 标记为发送数据
    mpu6050_msg[1].len = length;                // 写入长度

    /* 执行发送 */
    error = i2c_transfer(mpu6050_client->adapter, mpu6050_msg, 2);
    if (error != 2)
    {
        printk(KERN_DEBUG "\n i2c_read_mpu6050 error \n");
        return -1;
    }
    return 0;
}
static int mpu6050_init(void)
{
    /* 配置 mpu6050*/
    int error = 0;
    error += i2c_write_mpu6050(mpu6050_dev.mpu6050_client, PWR_MGMT_1, 0X00);
    error += i2c_write_mpu6050(mpu6050_dev.mpu6050_client, SMPLRT_DIV, 0X07);
    error += i2c_write_mpu6050(mpu6050_dev.mpu6050_client, CONFIG, 0X06);
    error += i2c_write_mpu6050(mpu6050_dev.mpu6050_client, ACCEL_CONFIG, 0X01);
    if (error < 0)
    {
        /* 初始化错误 */
        printk(KERN_DEBUG "\n mpu6050_init error \n");
        return -1;
    }
    return 0;
}

static int mpu6050_open(struct inode *inode, struct file *file)
{
    /* 向 mpu6050 发送配置数据，让 mpu6050 处于正常工作状态 */
    mpu6050_init();
    return 0;
}

static int mpu6050_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t mpu6050_read(struct file *file, char __user *buf, size_t size, loff_t *offset)
{
    char data_H;
    char data_L;
    int error;
    short mpu6050_result[6]; // 保存 mpu6050 转换得到的原始数据

    i2c_read_mpu6050(mpu6050_dev.mpu6050_client, ACCEL_XOUT_H, &data_H, 1);
    i2c_read_mpu6050(mpu6050_dev.mpu6050_client, ACCEL_XOUT_L, &data_L, 1);
    mpu6050_result[0] = data_H << 8;
    mpu6050_result[0] += data_L;

    i2c_read_mpu6050(mpu6050_dev.mpu6050_client, ACCEL_YOUT_H, &data_H, 1);
    i2c_read_mpu6050(mpu6050_dev.mpu6050_client, ACCEL_YOUT_L, &data_L, 1);
    mpu6050_result[1] = data_H << 8;
    mpu6050_result[1] += data_L;

    i2c_read_mpu6050(mpu6050_dev.mpu6050_client, ACCEL_ZOUT_H, &data_H, 1);
    i2c_read_mpu6050(mpu6050_dev.mpu6050_client, ACCEL_ZOUT_L, &data_L, 1);
    mpu6050_result[2] = data_H << 8;
    mpu6050_result[2] += data_L;

    i2c_read_mpu6050(mpu6050_dev.mpu6050_client, GYRO_XOUT_H, &data_H, 1);
    i2c_read_mpu6050(mpu6050_dev.mpu6050_client, GYRO_XOUT_L, &data_L, 1);
    mpu6050_result[3] = data_H << 8;
    mpu6050_result[3] += data_L;

    i2c_read_mpu6050(mpu6050_dev.mpu6050_client, GYRO_YOUT_H, &data_H, 1);
    i2c_read_mpu6050(mpu6050_dev.mpu6050_client, GYRO_YOUT_L, &data_L, 1);
    mpu6050_result[4] = data_H << 8;
    mpu6050_result[4] += data_L;

    i2c_read_mpu6050(mpu6050_dev.mpu6050_client, GYRO_ZOUT_H, &data_H, 1);
    i2c_read_mpu6050(mpu6050_dev.mpu6050_client, GYRO_ZOUT_L, &data_L, 1);
    mpu6050_result[5] = data_H << 8;
    mpu6050_result[5] += data_L;

    /* 将读取得到的数据拷贝到用户空间 */
    error = copy_to_user(buf, mpu6050_result, size);

    if (error != 0)
    {
        printk("copy_to_user error!");
        return -1;
    }
    return 0;
}

static ssize_t mpu6050_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
    return 0;
}

static struct file_operations mpu6050_fops = {
    .owner = THIS_MODULE,
    .open = mpu6050_open,
    .release = mpu6050_release,
    .write = mpu6050_write,
    .read = mpu6050_read,
};

static int mpu6050_remove(struct i2c_client *client)
{
    printk("mpu6050_remove  function\n");
    device_destroy(mpu6050_dev.mpu6050_class, mpu6050_dev.dev_num);
    class_destroy(mpu6050_dev.mpu6050_class);
    cdev_del(&mpu6050_dev.mpu6050_cdev);
    unregister_chrdev_region(mpu6050_dev.dev_num, 1);
    return 0;
}

static int mpu6050_probe(struct i2c_client *client, const struct i2c_device_id *device_id)
{

    int ret = -1;
    printk(KERN_INFO "mpu6050_probe  function\n");

    ret = alloc_chrdev_region(&(mpu6050_dev.dev_num), 0, 1, "mpu6050");
    if (ret < 0)
    {
        printk(KERN_ALERT "alloc_chrdev_region failed\n");
        goto alloc_chrdev_region_err;
    }

    cdev_init(&mpu6050_dev.mpu6050_cdev, &mpu6050_fops);
    mpu6050_dev.mpu6050_cdev.owner = THIS_MODULE;

    ret = cdev_add(&mpu6050_dev.mpu6050_cdev, mpu6050_dev.dev_num, 1);
    if (ret < 0)
    {
        printk(KERN_ALERT "cdev_add failed\n");
        goto cdev_add_err;
    }

    mpu6050_dev.mpu6050_class = class_create(THIS_MODULE, "IIC");
    if (IS_ERR(mpu6050_dev.mpu6050_class))
    {
        printk(KERN_ALERT "class_create failed\n");
        goto class_create_err;
    }

    mpu6050_dev.mpu6050_device = device_create(mpu6050_dev.mpu6050_class, NULL,
                                               mpu6050_dev.dev_num, NULL, "mpu6050");
    if (IS_ERR(mpu6050_dev.mpu6050_device))
    {
        printk(KERN_ALERT "device_create failed\n");
        goto device_create_err;
    }
    mpu6050_dev.mpu6050_client = client;
    return 0;
device_create_err:
    class_destroy(mpu6050_dev.mpu6050_class);
class_create_err:
    cdev_del(&mpu6050_dev.mpu6050_cdev);
cdev_add_err:
    unregister_chrdev_region(mpu6050_dev.dev_num, 1);
alloc_chrdev_region_err:
    return ret;
}

static const struct of_device_id mpu6050_of_match_table[] = {
    {.compatible = "invensense,mpu6050"}, {}};

static struct i2c_driver mpu6050_driver = {
    .remove = mpu6050_remove,
    .probe = mpu6050_probe,
    .driver = {
        .owner = THIS_MODULE,
        .name = "my-mpu6050",
        .of_match_table = mpu6050_of_match_table,
    },
};

static int __init mpu6050_driver_init(void)
{
    int ret = i2c_add_driver(&mpu6050_driver);
    if (ret < 0)
    {
        printk("i2c_add_driver failed!\n");
        return ret;
    }
    return 0;
}

static void __exit mpu6050_driver_exit(void)
{
    i2c_del_driver(&mpu6050_driver);
}

module_init(mpu6050_driver_init);
module_exit(mpu6050_driver_exit);
MODULE_LICENSE("GPL");