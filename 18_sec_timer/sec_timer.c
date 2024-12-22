#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/atomic.h>
#include <asm/atomic.h>
#include <linux/types.h>


static void timerfunc(struct timer_list*timertest);
DEFINE_TIMER(timertest, timerfunc);

struct device_stc{
    dev_t dev_num;
    struct cdev mycdev;
    struct class* class;
    struct device* device;
    int major;
    int minor;
    char kernelbuf[64];
    char usrbuf[64];
    atomic64_t count;
};

struct device_stc dev1;

static void timerfunc(struct timer_list*timertest)
{
    printk("timer function\n");
    mod_timer(timertest,jiffies_64 + msecs_to_jiffies(3000));
    atomic64_inc(&dev1.count);
}

static int cdev_test_open (struct inode *inode, struct file *file)
{
    file->private_data = &dev1;
    printk("cdev_test_open\n");

    timertest.expires = jiffies_64 + msecs_to_jiffies(3000);
    add_timer(&timertest);
    atomic64_set(&dev1.count,0);

    return 0;
}

static int cdev_test_release (struct inode *inode, struct file *file)
{
    del_timer(&timertest);
    printk("cdev_test_release\n");
    return 0;
}

static ssize_t cdev_test_read (struct file *file, char __user *buf, size_t size, loff_t *off)
{
    unsigned int cnt=0;
    struct device_stc *dev_s = (struct device_stc *)file -> private_data;

    //char kernelbuf[64] = "This is cdev_test_read";
    int ret;
    printk("cdev_test_read\n");
 
    cnt = atomic64_read(&dev_s->count);

    ret = copy_to_user(buf,&cnt,sizeof(cnt));
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
    printk("cdev_test_write\n");

    ret = copy_from_user(dev_s->usrbuf,buf,size);
    if(ret != 0){
        printk("write data to kernel failed!\n");
        return -1;
    }

    printk("usrbuf is %s\n",dev_s->usrbuf);

    return 0;
}

const struct file_operations mycdev_fops = {
    .owner = THIS_MODULE,
    .open=cdev_test_open,
    .read=cdev_test_read,
    .write=cdev_test_write,
    .release=cdev_test_release,
};

static int modulecdev_init(void)
{
    int ret=0;

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
    if(ret != 0)   
        goto err_cdev_add;
    
    /*3.创建设备节点*/
    dev1.class = class_create(THIS_MODULE,"device_node_test");
    if(IS_ERR(dev1.class)){
        ret = PTR_ERR(dev1.class);
        goto err_class_create;
    }
        
    dev1.device = device_create(dev1.class,NULL,dev1.dev_num,NULL,"device_node_test");
    if(IS_ERR(dev1.device)){
        ret = PTR_ERR(dev1.device);
        goto err_device_create;
    }

    return 0;
    err_device_create:
        class_destroy(dev1.class);
    err_class_create:
        cdev_del(&dev1.mycdev);
    err_cdev_add:
        unregister_chrdev_region(dev1.dev_num,1);
    err_chrdev:
        return ret;
}

static void modulecdev_exit(void)
{   
    device_destroy(dev1.class,dev1.dev_num);
    class_destroy(dev1.class);
    cdev_del(&dev1.mycdev);
    unregister_chrdev_region(dev1.dev_num,1);
    del_timer(&timertest);
    printk("unregister chrdev success\n");
}

module_init(modulecdev_init);
module_exit(modulecdev_exit);
MODULE_LICENSE("GPL");
