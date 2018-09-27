#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/task_stack.h>
#include <linux/types.h>
#include <asm/uaccess.h>
 
#include "fiber_ioctl.h"
 
#define FIRST_MINOR 0
#define MINOR_CNT 1
#define STACKSIZE (8 * 1024 * 1024)
#define ALIGN_SIZE 16
 
static dev_t dev;
static struct cdev c_dev;
static struct class *cl;

static int fiber_id = 0;
 
static int my_open(struct inode *i, struct file *f)
{
    return 0;
}
static int my_close(struct inode *i, struct file *f)
{
    return 0;
}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int my_ioctl(struct inode *i, struct file *f, unsigned int cmd, unsigned long arg)
#else
static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
#endif
{
	if (cmd == CONVERTTOFIBER){
		//save the context execution of the current thread
		//assing an id to the fiber
		//use an array or a bitmap to retrive a free id
		//return the id
		void* param;
		struct pt_regs *regs;
		fiber_arg_t* new_fiber =(fiber_arg_t*) kmalloc(sizeof(fiber_arg_t), GFP_KERNEL);
		if(!new_fiber) return -1;

		param = (void *)arg;

		new_fiber->param = param;
		
		new_fiber->index = __sync_fetch_and_add(&fiber_id, 1);
		
		regs = task_pt_regs(current);
		memcpy(new_fiber->regs, regs, sizeof(struct pt_regs));
		
		return new_fiber->index;
	}
	
	else if (cmd == CREATEFIBER){
		unsigned long stack_size;
		size_t reminder;
		void *stack;

		create_arg_t* args = (create_arg_t*) arg;
		fiber_arg_t* new_fiber =(fiber_arg_t*) kmalloc(sizeof(fiber_arg_t), GFP_KERNEL);
		if(!new_fiber) return -1;
		//alloc a stack (size of the stack = dwStackSize, allign to 16 bytes), if 0 use the default stack
		//set up the execution to begin at the specific adress (lpStartAddress), used to schedule this fiber
		//save parameters
		//assing an id to the fiber
		//return the id

		stack_size = args->dwStackSize;
		if(stack_size == 0) stack_size = STACKSIZE;

		// Align the size
		reminder = stack_size % ALIGN_SIZE;
		if (reminder != 0) {
			stack_size = (stack_size / ALIGN_SIZE + 1) * ALIGN_SIZE;
		}

		stack = (void *) kzalloc(stack_size, GFP_KERNEL);
		
		new_fiber->param = args->lpParameter;

		new_fiber->index = __sync_fetch_and_add(&fiber_id, 1);

		new_fiber->regs = (pt_regs*)kzalloc(sizeof(pt_regs));

		return new_fiber->index;
	}
	
	else if (cmd == SWITCHTOFIBER){
		//schedules a fiber
		//it has as parameter the address of the fiber to be scheduled
		//use an hashmap to easy retrive the fiber given the address
		//does not return
		return 0;
	}
	
	else if (cmd == FLSALLOC){
		//allocate a fiber local storage 
		//return the fls index
		return 0;
	}
	
	else if (cmd == FLSFREE){
		//make the fls available for reuse
		return 0;
	}
	
	else if (cmd == FLSGETVALUE){
		//returnt the value in the fls slot using the index passed as input 
		return 0;
	}
	
	else if (cmd == FLSSETVALUE){
		//fls_set_arg_t* args = (fls_set_arg_t*) arg;
		//set the given value in the slot identified by the index
		return 0;
	}

    return 0;
}
 
static struct file_operations query_fops =
{
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_close,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
    .ioctl = my_ioctl
#else
    .unlocked_ioctl = my_ioctl
#endif
};
 
static int __init fiber_ioctl_init(void)
{
    int ret;
    struct device *dev_ret;
 
 
    if ((ret = alloc_chrdev_region(&dev, FIRST_MINOR, MINOR_CNT, "query_ioctl")) < 0)
    {
        return ret;
    }
 
    cdev_init(&c_dev, &query_fops);
 
    if ((ret = cdev_add(&c_dev, dev, MINOR_CNT)) < 0)
    {
        return ret;
    }
     
    if (IS_ERR(cl = class_create(THIS_MODULE, "char")))
    {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(cl);
    }
    if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, "query")))
    {
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(dev_ret);
    }
 
    return 0;
}
 
static void __exit fiber_ioctl_exit(void)
{
    device_destroy(cl, dev);
    class_destroy(cl);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev, MINOR_CNT);
}
 
module_init(fiber_ioctl_init);
module_exit(fiber_ioctl_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia <email_at_sarika-pugs_dot_com>");
MODULE_DESCRIPTION("Query ioctl() Char Driver");
