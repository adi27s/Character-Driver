#include<linux/module.h>

static int __init helloworld_init(void){
    pr_info("Hello World !\n");
    // printk("Hello World !\n");
    return 0;
}

static void __exit helloworld_cleanup(void){
    pr_info("Bye World !\n");
    // printk("Bye World !\n");
}

module_init(helloworld_init);
module_exit(helloworld_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ADITYA");
MODULE_DESCRIPTION("A simple kernel module");
MODULE_INFO(board,"BBB REV C");