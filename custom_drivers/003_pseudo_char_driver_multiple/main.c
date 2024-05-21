#include<linux/module.h>    // Default
#include<linux/fs.h>        // alloc_chrdev_region fucntion, fileops
#include<linux/cdev.h>      // cdev_init fucntion
#include<linux/device.h>    // class,device create functions
#include<linux/kdev_t.h>    // M and m
#include<linux/uaccess.h>   // copy_to/from_user functions
#include<linux/err.h>       // error handling

#undef pr_fmt
#define pr_fmt(fmt) "%s :" fmt,__func__

#define NO_OF_DEVICES 4

#define MEM_SIZE_MAX_PCDEV1 1024
#define MEM_SIZE_MAX_PCDEV2 1024
#define MEM_SIZE_MAX_PCDEV3 1024
#define MEM_SIZE_MAX_PCDEV4 1024

/*RWS operations will be carried out using this buffer*/
char device_buffer_pcdev1[MEM_SIZE_MAX_PCDEV1];
char device_buffer_pcdev2[MEM_SIZE_MAX_PCDEV2];
char device_buffer_pcdev3[MEM_SIZE_MAX_PCDEV3];
char device_buffer_pcdev4[MEM_SIZE_MAX_PCDEV4];


/* Device private data stucture*/
struct pcdev_private_data{
    char *buffer;
    unsigned size;
    const char *serial_number;
    int perm;
    struct cdev cdev;
};


/* Driver private data structure */
struct pcdrv_private_data{
    int total_devices;
    dev_t device_number;
    struct class *class_pcd;
    struct device *device_pcd;
    struct pcdev_private_data pcdev_data[NO_OF_DEVICES];
};

struct pcdrv_private_data pcdrv_data = {
    .total_devices = NO_OF_DEVICES,
    .pcdev_data = {
        [0] = {
            .buffer = device_buffer_pcdev1,
            .size = MEM_SIZE_MAX_PCDEV1,
            .serial_number = "PCDEV1XYZ123",
            .perm = 0x1 /*RDONLY*/
        },
        [1] = {
            .buffer = device_buffer_pcdev2,
            .size = MEM_SIZE_MAX_PCDEV2,
            .serial_number = "PCDEV2XYZ123",
            .perm = 0x10 /*WRONLY*/
        },
        [2] = {
            .buffer = device_buffer_pcdev3,
            .size = MEM_SIZE_MAX_PCDEV3,
            .serial_number = "PCDEV3XYZ123",
            .perm = 0x11 /*RDWR*/
        },
        [3] = {
            .buffer = device_buffer_pcdev4,
            .size = MEM_SIZE_MAX_PCDEV4,
            .serial_number = "PCDEV4XYZ123",
            .perm = 0x11 /*RDWR*/
        }
    }
};


loff_t pcd_lseek(struct file *filp, loff_t off, int whence){
    
    struct pcdev_private_data *pcdev_data = (struct pcdev_private_data*)filp->private_data;
    int max_size = pcdev_data->size;

    loff_t temp;

    pr_info("lseek requested\n");
    pr_info("Current value of file position = %lld\n",filp->f_pos);

    switch(whence){
        case SEEK_SET:
            if(( off > max_size) || (off < 0)){
                return -EINVAL;
            }
            filp->f_pos = off;
            break;
        case SEEK_CUR:
            temp = filp->f_pos + off;
            if(( temp > max_size) || (temp < 0)){
                return -EINVAL;
            }
            filp->f_pos = temp;
            break;
        case SEEK_END:
            temp = max_size + off;
            if(( temp > max_size) || (temp < 0)){
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
    
    struct pcdev_private_data *pcdev_data = (struct pcdev_private_data*)filp->private_data;
    int max_size = pcdev_data->size;

    pr_info("Read requested for %zu bytes\n",count);
    pr_info("Current file position = %lld\n",*f_pos);
    
    /*Adjust the 'count' */
    if((*f_pos + count) > max_size){
        count = max_size - *f_pos;
    }
    /*copy to user */
    if (copy_to_user(buff,pcdev_data->buffer+(*f_pos),count)){
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
    struct pcdev_private_data *pcdev_data = (struct pcdev_private_data*)filp->private_data;
    int max_size = pcdev_data->size;
    
    pr_info("Write requested for %zu bytes\n",count);
    pr_info("Current file position = %lld\n",*f_pos);
    
    /*Adjust the 'count' */
    if((*f_pos + count) > max_size){
        count = max_size - *f_pos;
    }

    if(!count){
        return -ENOMEM;
    }

    /*copy to user */
    if (copy_from_user(pcdev_data->buffer+(*f_pos),buff,count)){
        return -EFAULT;
    }
    /*update the current file position */
    *f_pos += count;

    pr_info("Number of bytes successfully written = %zu\n",count);
    pr_info("Updated file position = %lld\n",*f_pos);

    /*return number of bytes successfully read */
    return count;
}

#define RDONLY 0x01
#define WRONLY 0x10
#define RDWR 0x11

int check_permission(int dev_perm, int acc_mode){
    
    if(dev_perm == RDWR)
        return 0;

    /*read only*/
    if((dev_perm ==RDONLY) && ((acc_mode & FMODE_READ) && !(acc_mode & FMODE_WRITE)))
        return 0;
    
    /*write only*/
    if((dev_perm ==WRONLY) && ((acc_mode & FMODE_WRITE) && !(acc_mode & FMODE_READ)))
        return 0;

    return -EPERM;
}

int pcd_open(struct inode *inode, struct file *filp){
    
    int ret;
    int minor_n;
    struct pcdev_private_data *pcdev_data;

    /*find out on which device file open was attempted by the user space*/
    minor_n = MINOR(inode->i_rdev);
    pr_info("minor access = %d\n",minor_n);

    /*get deivce's private data structure*/
    pcdev_data = container_of(inode->i_cdev,struct pcdev_private_data,cdev); // what i have created
    filp->private_data = pcdev_data; //  to su[ply private data to other methods of the driver

    /*check permission*/
    ret = check_permission(pcdev_data->perm,filp->f_mode);

    (!ret)? pr_info("open was successful\n"):pr_info("open was unsuccessful\n");

    return ret;
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
// struct file_operations pcd_fops;

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
    int i;
    ret = alloc_chrdev_region(&pcdrv_data.device_number,0,NO_OF_DEVICES,"pcd_devices");
    if (ret <0){
        pr_info("Alloc chrdev failed\n");
        goto out;
    }

    /* 4. Create device class under /sys/class/ 
    struct class *class_create(owner,class name)
    */
    // pcdrv_data.class_pcd = class_create(THIS_MODULE,"pcd_class");
    pcdrv_data.class_pcd = class_create("pcd_class");
    if (IS_ERR(pcdrv_data.class_pcd))
    {
        pr_info("Class creation failed\n");
        ret = PTR_ERR(pcdrv_data.class_pcd);
        goto unreg_chrdev;
    }


    for (i=0;i<NO_OF_DEVICES;i++){
            pr_info("Device number <major>:<minor> = %d:%d\n",MAJOR(pcdrv_data.device_number+i),MINOR(pcdrv_data.device_number+i));

            /* 2. Initialize the cdev structure with fops ++++++++++
            -- > void cdev_init(cdev,fops)
            struct cdev cdev
            struct file_operations fops
            */     
            cdev_init(&pcdrv_data.pcdev_data[i].cdev,&pcd_fops);
            /* A pointer to the module that owns this structure. It is used to 
            prevent the module from being unloaded while the structure is in use. */
            pcdrv_data.pcdev_data[i].cdev.owner = THIS_MODULE;

            /* 3. Register a device (cdev structure) with VFS +++++++++++ 
            */
            ret = cdev_add(&pcdrv_data.pcdev_data[i].cdev,pcdrv_data.device_number+i,1);
            if (ret<0){
                pr_info("Cdev add failed\n");
                goto cdev_del;
            }


            /* 5. Populate the sysfs with device information 
            device_create(class pointer,parent,device number, driver data, device name)
            */
            pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd,NULL,pcdrv_data.device_number+i,NULL,"pcdev-%d",i+1);
            if (IS_ERR(pcdrv_data.device_pcd))
            {
                pr_info("Device creation failed\n");
                ret = PTR_ERR(pcdrv_data.device_pcd);
                goto class_del;
            }

    }
    
    pr_info("Module init was successful\n");
    return 0;

cdev_del:
class_del:
    for (;i>=0;i--){
        device_destroy(pcdrv_data.class_pcd,pcdrv_data.device_number+i);
        cdev_del(&pcdrv_data.pcdev_data[i].cdev);
    }
    class_destroy(pcdrv_data.class_pcd);
unreg_chrdev:
    unregister_chrdev_region(pcdrv_data.device_number,NO_OF_DEVICES);    
out:
    pr_info("Module insertion failed\n");
    return ret;
}

// ----------------  Exit/Cleanup function  ----------------
static void __exit pcd_driver_cleanup(void){

    int i=0;
    for (i=0;i<NO_OF_DEVICES;i++){
        device_destroy(pcdrv_data.class_pcd,pcdrv_data.device_number+i);
        cdev_del(&pcdrv_data.pcdev_data[i].cdev);
    }

    class_destroy(pcdrv_data.class_pcd);
    unregister_chrdev_region(pcdrv_data.device_number,NO_OF_DEVICES);
    pr_info("module unloaded\n");
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ADITYA");
MODULE_DESCRIPTION("A pseudo character driver which handles n devices");
MODULE_INFO(board,"BBB REV C");