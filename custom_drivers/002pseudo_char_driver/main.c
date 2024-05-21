#include<linux/module.h>    // Default
#include<linux/fs.h>        // alloc_chrdev_region fucntion, fileops
#include<linux/cdev.h>      // cdev_init fucntion
#include<linux/device.h>    // class,device create functionsd
#include<linux/kdev_t.h>    // M and m
#include<linux/uaccess.h>   // copy_to/from_user functions
#include<linux/err.h>       // error handling

#undef pr_fmt
#define pr_fmt(fmt) "%s :" fmt,__func__
#define DEV_MEM_SIZE 512

/*RWS operations will be carried out using this buffer*/
char device_buffer[DEV_MEM_SIZE];

// Holds device number
dev_t device_number;

// Cdev variable = number of devices you want to handle (here we are handling only one device so creating it once)
struct cdev pcd_cdev;

loff_t pcd_lseek(struct file *filp, loff_t off, int whence){
    
    loff_t temp;

    pr_info("lseek requested\n");
    pr_info("Current value of file position = %lld\n",filp->f_pos);

    switch(whence){
        case SEEK_SET:
            if(( off > DEV_MEM_SIZE) || (off < 0)){
                return -EINVAL;
            }
            filp->f_pos = off;
            break;
        case SEEK_CUR:
            temp = filp->f_pos + off;
            if(( temp > DEV_MEM_SIZE) || (temp < 0)){
                return -EINVAL;
            }
            filp->f_pos = temp;
            break;
        case SEEK_END:
            temp = DEV_MEM_SIZE + off;
            if(( temp > DEV_MEM_SIZE) || (temp < 0)){
                return -EINVAL;
            }
            filp->f_pos = temp;
            break;
        default:
            return -EINVAL;
    }
    pr_info("Current value of new position = %lld\n",filp->f_pos);
    return filp->f_pos;
}
ssize_t pcd_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos){
    pr_info("Read requested for %zu bytes\n",count);
    pr_info("Current file position = %lld\n",*f_pos);
    
    /*Adjust the 'count' */
    if((*f_pos + count) > DEV_MEM_SIZE){
        count = DEV_MEM_SIZE - *f_pos;
    }
    /*copy to user */
    if (copy_to_user(buff,&device_buffer[*f_pos],count)){
        return -EFAULT;
    }
    /*update the current file position */
    *f_pos += count;

    pr_info("Number of bytes successfully read = %zu\n",count);
    pr_info("Updated file position = %lld\n",*f_pos);

    /*return number of bytes successfully read */
    return count;
}
ssize_t pcd_write(struct file *filp, const char __user *buff, size_t count, loff_t *f_pos){
    pr_info("Write requested for %zu bytes\n",count);
    pr_info("Current file position = %lld\n",*f_pos);
    
    /*Adjust the 'count' */
    if((*f_pos + count) > DEV_MEM_SIZE){
        count = DEV_MEM_SIZE - *f_pos;
    }

    if(!count){
        return -ENOMEM;
    }

    /*copy to user */
    if (copy_from_user(&device_buffer[*f_pos],buff,count)){
        return -EFAULT;
    }
    /*update the current file position */
    *f_pos += count;

    pr_info("Number of bytes successfully written = %zu\n",count);
    pr_info("Updated file position = %lld\n",*f_pos);

    /*return number of bytes successfully read */
    return count;
}
int pcd_open(struct inode *inode, struct file *filp){
    pr_info("open was successful\n");
    return 0;
}
int pcd_release(struct inode *inode, struct file *filp){
    pr_info("release was successful\n");
    return 0;
}

// file operations
struct file_operations pcd_fops = 
{
    .open=pcd_open,
    .write=pcd_write,
    .read=pcd_read,
    .llseek=pcd_lseek,
    .release=pcd_release,
    .owner=THIS_MODULE
};

struct class *class_pcd;
struct device *device_pcd;

// ----------------  Entry function  ----------------
static int __init pcd_driver_init(void){

    /* 1. Dynamically allocate a device number +++++++++
    -- > int alloc_chrdev_region(major_number,baseminor,count,name) 
    dev_t major_number
    unsigned baseminor
    unsigned count
    const char name (name given to device number/ range given to device number)
    */
    int ret;
    ret = alloc_chrdev_region(&device_number,0,1,"pcd_devices");
    if (ret <0){
        pr_info("Alloc chrdev failed\n");
        goto out;
    }
    pr_info("Device number <major>:<minor> = %d:%d\n",MAJOR(device_number),MINOR(device_number));
    
    /* 2. Initialize the cdev structure with fops ++++++++++
    -- > void cdev_init(cdev,fops)
    struct cdev cdev
    struct file_operations fops
    */
    cdev_init(&pcd_cdev,&pcd_fops);
    /* A pointer to the module that owns this structure. It is used to 
    prevent the module from being unloaded while the structure is in use. */
    pcd_cdev.owner = THIS_MODULE;

    /* 3. Register a device (cdev structure) with VFS +++++++++++ 
    */
    ret = cdev_add(&pcd_cdev,device_number,1);
    if (ret<0){
        pr_info("Cdev add failed\n");
        goto unreg_chrdev;
    }
        
    /* 4. Create device class under /sys/class/ 
    struct class *class_create(owner,class name)
    */
    // class_pcd = class_create(THIS_MODULE,"pcd_class");
    class_pcd = class_create("pcd_class");
    if (IS_ERR(class_pcd))
    {
        pr_info("Class creation failed\n");
        ret = PTR_ERR(class_pcd);
        goto cdev_del;
    }


    /* 5. Populate the sysfs with device information 
    device_create(class pointer,parent,device number, driver data, device name)
    */
    device_pcd = device_create(class_pcd,NULL,device_number,NULL,"pcd");
    if (IS_ERR(device_pcd))
    {
        pr_info("Device creation failed\n");
        ret = PTR_ERR(device_pcd);
        goto class_del;
    }
    pr_info("Module init was successful\n");

    return 0;


class_del:
    class_destroy(class_pcd);
cdev_del:
    cdev_del(&pcd_cdev);
unreg_chrdev:
    unregister_chrdev_region(device_number,1);    
out:
    return ret;
}

// ----------------  Exit/Cleanup function  ----------------
static void __exit pcd_driver_cleanup(void){
    device_destroy(class_pcd,device_number);        // Removes device
    class_destroy(class_pcd);                       // Destroy struct class structure
    cdev_del(&pcd_cdev);                            // Remove cdev registeration from kernel VFS
    unregister_chrdev_region(device_number,1);      // Unregister a range of device numbers (devicenumber,minor number)
    pr_info("module unloaded\n");
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ADITYA");
MODULE_DESCRIPTION("A pseudo kernel module");
MODULE_INFO(board,"BBB REV C");