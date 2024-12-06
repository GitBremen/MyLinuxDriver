#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>

#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/io.h>

struct resource *myresource;

struct device_stc{
    dev_t dev_num;
    struct cdev mycdev;
    struct class* class;
    struct device* device;
    int major;
    int minor;
    char kernelbuf[64];
    char usrbuf[64];
    unsigned int* VIR_GPIO_DR;
};

struct device_stc dev1;

static int cdev_test_open (struct inode *inode, struct file *file)
{
    file->private_data = &dev1;
    printk("cdev_test_open\n");
    return 0;
}

static int cdev_test_release (struct inode *inode, struct file *file)
{
    printk("cdev_test_release\n");
    return 0;
}

static ssize_t cdev_test_read (struct file *file, char __user *buf, size_t size, loff_t *off)
{
    struct device_stc *dev_s = (struct device_stc *)file -> private_data;

    //char kernelbuf[64] = "This is cdev_test_read";
    int ret;
    printk("cdev_test_read\n");
 
    ret = copy_to_user(buf,dev_s->kernelbuf,strlen(dev_s->kernelbuf));
    if(ret != 0){
        printk("read data from kernel failed!\n");
        return -1;
    }

    return 0;
}

static ssize_t cdev_test_write (struct file *file, const char __user *buf, size_t size, loff_t *off)
{
    struct device_stc *dev_s = (struct device_stc *)file -> private_data;

    //char usrbuf[64];
    int ret;
    //printk("cdev_test_write\n");

    ret = copy_from_user(dev_s->usrbuf,buf,size);
    if(ret != 0){
        printk("write data to kernel failed!\n");
        return -1;
    }

    if(dev_s->usrbuf[0]==1){
        writel(0x00200061,dev_s->VIR_GPIO_DR);
    }else if(dev_s->usrbuf[0]==0){
        writel(0x00200041,dev_s->VIR_GPIO_DR);
    }

    printk("usrbuf is %d\n",dev_s->usrbuf[0]);

    return 0;
}

const struct file_operations mycdev_fops = {
    .owner = THIS_MODULE,
    .open=cdev_test_open,
    .read=cdev_test_read,
    .write=cdev_test_write,
    .release=cdev_test_release,
};

static int pltdrv_probe(struct platform_device *dev)
{   
    int ret=0;
    printk("probe function\n");

    printk("IRQ is %lld\n",dev->resource[1].start);

    myresource = platform_get_resource(dev,IORESOURCE_IRQ,0);
    printk("IRQ is %lld\n",myresource->start);

    myresource = platform_get_resource(dev,IORESOURCE_MEM,0);
    printk("MEM is %llx\n",myresource->start);
    
    /*1.动态申请设备号*/
    ret = alloc_chrdev_region(&dev1.dev_num,0,1,"allco_name");
    if(ret != 0)
        goto err_chrdev;

    printk("alloc chrdev success!\n");

    dev1.major = MAJOR(dev1.dev_num);
    dev1.minor = MINOR(dev1.dev_num);
    printk("major is %d\n",dev1.major);
    printk("minor is %d\n",dev1.minor);

    dev1.mycdev.owner = THIS_MODULE;
    //mycdev.dev = dev_num;

    /*2.注册字符类设备*/
    cdev_init(&dev1.mycdev,&mycdev_fops);
    ret = cdev_add(&dev1.mycdev,dev1.dev_num,1);
    if(ret < 0){
        goto err_cdev_add;
    }       
    
    /*3.创建设备节点*/
    dev1.class = class_create(THIS_MODULE,"led_test");
    if(IS_ERR(dev1.class)){
        ret = PTR_ERR(dev1.class);
        goto err_class_create;
    }
        
    dev1.device = device_create(dev1.class,NULL,dev1.dev_num,NULL,"led_test");
    if(IS_ERR(dev1.device)){
        ret = PTR_ERR(dev1.device);
        goto err_device_create;
    }

    dev1.VIR_GPIO_DR = ioremap(myresource->start,4);
    if(IS_ERR(dev1.VIR_GPIO_DR)){
        ret = PTR_ERR(dev1.VIR_GPIO_DR);
        goto err_ioremap;
    }

    return 0;
err_ioremap:
    iounmap(dev1.VIR_GPIO_DR);
    printk("1");
err_device_create:
    class_destroy(dev1.class);
    printk("2");
err_class_create:
    cdev_del(&dev1.mycdev);
    printk("3");
err_cdev_add:
    unregister_chrdev_region(dev1.dev_num,1);
    printk("4");
err_chrdev:
    printk("5");
    return ret;
}

static int pltdrv_remove(struct platform_device *dev)
{
    printk("remove function\n");
    return 0;
}

const struct platform_device_id myidtable = {
    .name = "pltdev",
};

static struct platform_driver pltdrv = {
    .probe = pltdrv_probe,
    .remove = pltdrv_remove,
    .driver = {
        .name = "pltdev",
        .owner = THIS_MODULE,
    },
    .id_table = &myidtable,
};

static int platform_driver_init(void)
{
    printk("platform_device_register\n");
    platform_driver_register(&pltdrv);
    return 0;
}

static void platform_driver_exit(void)
{
    iounmap(dev1.VIR_GPIO_DR);
    device_destroy(dev1.class,dev1.dev_num);
    class_destroy(dev1.class);
    cdev_del(&dev1.mycdev);
    unregister_chrdev_region(dev1.dev_num,1);
    platform_driver_unregister(&pltdrv);
    printk("platform_device_unregister\n");
}

module_init(platform_driver_init);
module_exit(platform_driver_exit);
MODULE_LICENSE("GPL");
