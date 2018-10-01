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
#include "function_macro.h"
 
#define FIRST_MINOR 0
#define MINOR_CNT 1
#define STACKSIZE (8 * 1024 * 1024)
#define ALIGN_SIZE 16

 
static dev_t dev;
static struct cdev c_dev;
static struct class *cl;
static int Device_Open = 0;

static int fls_id = 0;

static unsigned long fls_array[MAX_FLS];

 
static int my_open(struct inode *i, struct file *f)
{
	/* 
	* We don't want to talk to two processes at the same time 
	*/
	if (Device_Open)
		return -EBUSY;

	Device_Open++;

	try_module_get(THIS_MODULE);
	return 0;
}
static int my_close(struct inode *i, struct file *f)
{
	/* 
	* We're now ready for our next caller 
	*/
	Device_Open--;

	module_put(THIS_MODULE);
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
		//return the id
		int i, exist = 0;
		pid_t pro_id, thr_id;
		void* param;
		process_arg_t* node;
		process_arg_t* proc = (process_arg_t*) kmalloc(sizeof(process_arg_t), GFP_KERNEL);
		struct pt_regs *regs = task_pt_regs(current);
		fiber_arg_t* fiber = (fiber_arg_t*) kmalloc(sizeof(fiber_arg_t), GFP_KERNEL);
		thread_arg_t* cursor;
		thread_arg_t* thread = (thread_arg_t*) kmalloc(sizeof(thread_arg_t), GFP_KERNEL);

		pro_id = current->tgid;
		thr_id = current->pid;

		hash_for_each(list_process, i, node , p_list){
			if(node->pid == pro_id) {
				exist = 1;
				proc = node;
			}
		}
		if(exist == 0){
			proc->fiber_id = 0;
			proc->pid = pro_id;
			hash_add_rcu(list_process, &proc->p_list, proc->pid);
			hash_init(proc->threads);
			hash_init(proc->fibers);
		}

		exist = 0;

		hash_for_each(proc->threads, i, cursor , t_list){
			if(cursor->pid == thr_id) exist = 1;
		}
		if(exist == 0){
			thread->pid = thr_id;
			hash_add_rcu(proc->threads, &thread->t_list, thread->pid);

			fiber->regs = (struct pt_regs*) kmalloc(sizeof(struct pt_regs), GFP_KERNEL);
			if(!fiber) return -1;

			param = (void *)arg;

			fiber->param = param;
			fiber->index = __sync_fetch_and_add(&proc->fiber_id, 1);
			memcpy(fiber->regs, regs, sizeof(struct pt_regs));

			return fiber->index;
		}
		return -1;
	}
	
	else if (cmd == CREATEFIBER){
		unsigned long stack_size;
		size_t reminder;
		void *stack;
		int rc, i, exist = 0;
		pid_t pro_id, thr_id;
		create_arg_t* args = (create_arg_t*) kmalloc (sizeof(create_arg_t), GFP_KERNEL);
		fiber_arg_t* new_fiber = (fiber_arg_t*) kmalloc(sizeof(fiber_arg_t), GFP_KERNEL);
		process_arg_t* node, *proc;
		thread_arg_t* cursor, *thr;

		rc = raw_copy_from_user(args, (void *)arg, sizeof(create_arg_t));

		//alloc a stack (size of the stack = dwStackSize, allign to 16 bytes), if 0 use the default stack
		//set up the execution to begin at the specific adress (lpStartAddress), used to schedule this fiber
		//save parameters
		//assing an id to the fiber
		//return the id

		pro_id = current->tgid;
		thr_id = current->pid;

		//check if the current process is presente in the hashtable
		hash_for_each(list_process, i, node , p_list){
			if(node->pid == pro_id) {
				exist = 1;
				proc = node;
			}
		}
		if(exist == 0){
			return -1;
		}

		exist = 0;
		//check if the current thread is present in the hastable, 
		//if it is we know that the thread has already compute ConverteToThread
		hash_for_each(proc->threads, i, cursor , t_list){
			if(cursor->pid == thr_id) {
				exist = 1;
				thr = cursor;
			}
		}

		//in questo modo un thread può creare molti fibers
		//exist == 0 non può creare fibers
		//togliere l'inserimento nel convert?
		if(exist == 1){
			stack_size = args->dwStackSize;
			if(stack_size == 0) stack_size = STACKSIZE;

			// Align the size
			reminder = stack_size % ALIGN_SIZE;
			if (reminder != 0) {
				stack_size = (stack_size / ALIGN_SIZE + 1) * ALIGN_SIZE;
			}

			stack = (void *) kzalloc(stack_size, GFP_KERNEL);
			//new_fiber->param = args->lpParameter;
			new_fiber->index = __sync_fetch_and_add(&proc->fiber_id, 1);
			new_fiber->regs = (struct pt_regs*)kzalloc(sizeof(struct pt_regs), GFP_KERNEL);

			new_fiber->regs->sp = (unsigned long)stack + stack_size - 8;
			new_fiber->regs->ip = (unsigned long)args->lpStartAddress;
			new_fiber->regs->di = (unsigned long)args->lpParameter;
			new_fiber->regs->bp = new_fiber->regs->sp;
			return new_fiber->index;
		}
		return -1;
	}
	
	else if (cmd == SWITCHTOFIBER){
		//schedules a fiber
		//it has as parameter the address of the fiber to be scheduled
		//use an hashmap to easy retrive the fiber given the address
		//does not return

		int i, pro_id, ind = (int)arg;
		int exist = 0;
		process_arg_t* node;
		thread_arg_t* cursor;

		pro_id = current->tgid;

		//check if the current process is presente in the hashtable
		hash_for_each(list_process, i, node , p_list){
			if(node->pid == pro_id) exist = 1;
		}
		if(exist == 0){
			return -1;
		}

		exist = 0;

		//check if the current thread is present in the hastable, 
		//if it is we know that the thread has already compute ConverteToThread
		hash_for_each(node->threads, i, cursor , t_list){
			if(cursor->pid == ind) exist = 1;
		}
		if(exist == 1){
		}


		return -1;
	}
	
	else if (cmd == FLSALLOC){
		//allocate a fiber local storage 
		//return the fls index
		long ret = __sync_fetch_and_add (&fls_id, 1);
		if(ret >= MAX_FLS)
			return -1;
		return ret;
	}
	
	else if (cmd == FLSFREE){
		return true;
	}
	
	else if (cmd == FLSGETVALUE){
		//returnt the value in the fls slot using the index passed as input 
		long long ret;
		long ind = (long) arg;

		if(ind >= fls_id) return -1;

		spin_lock_irq(&lock_fls);
		ret = fls_array[ind];
		spin_unlock_irq(&lock_fls);
		return ret;
	}
	
	else if (cmd == FLSSETVALUE){
		int rc;
		long ind;

		fls_set_arg_t* args = (fls_set_arg_t*) kmalloc (sizeof(fls_set_arg_t), GFP_KERNEL);
		rc = raw_copy_from_user(args, (void *)arg, sizeof(fls_set_arg_t));

		//set the given value in the slot identified by the index
		ind = args->dwFlsIndex;

		spin_lock_irq(&lock_fls);
		fls_array[ind] = args->lpFlsData;
		spin_unlock_irq(&lock_fls);
	}

    return 0;
}
 
static struct file_operations fiber_fops =
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
 
    if ((ret = alloc_chrdev_region(&dev, FIRST_MINOR, MINOR_CNT, "fiber_ioctl")) < 0)
    {
        return ret;
    }
 
    cdev_init(&c_dev, &fiber_fops);
 
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
    if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, "fiber")))
    {
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(dev_ret);
    }

	hash_init(list_process);
 
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
MODULE_DESCRIPTION("Fiber ioctl() Char Driver");
