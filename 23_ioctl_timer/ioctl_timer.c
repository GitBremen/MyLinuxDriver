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

#define TIMER_CLOSE_CMD     _IO('T',0)
#define TIMER_OPEN_CMD      _IO('T',1)
#define TIMER_SET_CMD       _IOW('T',2,int)


#define BUFSIZE 1024
static char mem[BUFSIZE]={0};

static void timer_func(struct timer_list *newtimer);

struct device_stc{
    dev_t dev_num;
    struct cdev mycdev;
    struct class* class;
    struct device* device;
    int major;
    int minor;
    char kbuf[64];
    int timeset;
};

struct device_stc dev1;

DEFINE_TIMER(newtimer,timer_func);

static spinlock_t spinlock_v;

static void timer_func(struct timer_list *newtimer)
{
    printk("timer function\n");
    mod_timer(newtimer,jiffies_64 + msecs_to_jiffies(dev1.timeset));
}

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
    loff_t p=*off;          //相对于文件开头的偏移
    size_t count = size;    //读取的大小
    int i=0;

    if(p>BUFSIZE)return 0;
    if(count+p>BUFSIZE)count = BUFSIZE-p;
    if(copy_to_user(buf,mem+p,count)){
        printk("copy_to_user error\n");
        return -1;
    }

    for(i=0;i<20;++i)
        printk("buf(%d) is %c\n",i,mem[i]);

    printk("mem is %s ,p is %llu,count is %ld\n",mem+p,p,count);
    
    *off += count;
    return count;
}

static ssize_t cdev_test_write (struct file *file, const char __user *buf, size_t size, loff_t *off)
{
    loff_t p=*off;          //相对于文件开头的偏移
    size_t count = size;    //读取的大小

    if(p>BUFSIZE)return 0;
    if(count+p>BUFSIZE)count = BUFSIZE-p;

    if(copy_from_user(mem+p,buf,count)){
        printk("copy_from_user error\n");
        return -1;
    }

    printk("mem is %s ,p is %llu\n",mem+p,p);

    *off += count;
    return count;
}

loff_t cdev_test_llseek (struct file *file, loff_t offset, int whence)
{
    //struct device_stc *dev_s = (struct device_stc *)file -> private_data;
    loff_t new_offset;
    switch(whence){
        case SEEK_SET:
            if(offset<0){
                return -EINVAL;
                break;
            }
            if(offset > BUFSIZE){
                return -EINVAL;
                break;
            }
            new_offset = offset;
            break;
        case SEEK_CUR:
            if((file->f_pos+offset)<0){
                return -EINVAL;
                break;
            }
            if((file->f_pos+offset) > BUFSIZE){
                return -EINVAL;
                break;
            }
            new_offset = file->f_pos + offset;
            break;
        case SEEK_END:
            if((file->f_pos+offset)<0){
                return -EINVAL;
                break;
            }
            new_offset = BUFSIZE+offset;
            break;
        default:
            return -EINVAL;
    }
    file->f_pos = new_offset;
    return new_offset;
}

long cdev_test_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{  
    struct device_stc *dev_s = (struct device_stc *)file -> private_data;
    switch(cmd){
        case TIMER_CLOSE_CMD:
            printk("执行用户定义的操作 TIMER_CLOSE_CMD\n");
            del_timer(&newtimer);
            break;
        case TIMER_OPEN_CMD:
            printk("执行用户定义的操作 TIMER_OPEN_CMD\n");
            
            add_timer(&newtimer);
            break;
        case TIMER_SET_CMD:
            printk("执行用户定义的操作 TIMER_SET_CMD\n");
            spin_lock(&spinlock_v);
            dev_s->timeset = arg;
            newtimer.expires = jiffies_64 + msecs_to_jiffies(dev_s->timeset);
            spin_unlock(&spinlock_v);
            break;
        default:
            break;
    }
    return 0;
}

const struct file_operations mycdev_fops = {
    .owner = THIS_MODULE,
    .open=cdev_test_open,
    .read=cdev_test_read,
    .write=cdev_test_write,
    .release=cdev_test_release,
    .llseek=cdev_test_llseek,
    .unlocked_ioctl=cdev_test_ioctl,
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
    del_timer(&newtimer);
    printk("unregister chrdev success\n");
}

module_init(modulecdev_init);
module_exit(modulecdev_exit);
MODULE_LICENSE("GPL");
