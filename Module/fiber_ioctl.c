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
#include <linux/uaccess.h>
#include <asm/fpu/internal.h>
 
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

process_arg_t* search_process(pid_t proc_id){
	process_arg_t* node, *process;
	int exist = 0;
	hash_for_each_possible_rcu(list_process, node, p_list, proc_id){
		if(node->pid == proc_id) {
			exist = 1;
			process = node;
		}
	}
	if(exist == 1) return process;
	return NULL;
}

thread_arg_t* search_thread(pid_t thr_id, process_arg_t* process){
	thread_arg_t* node, *thread;
	int exist = 0;
	hash_for_each_possible_rcu(process->threads, node, t_list, thr_id){
		if(node->pid == thr_id) {
			exist = 1;
			thread = node;
		}
	}
	if(exist == 1) return thread;
	return NULL;
}


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int my_ioctl(struct inode *i, struct file *f, unsigned int cmd, unsigned long arg)
#else
static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
#endif
{
	if (cmd == CONVERTTOFIBER){

		pid_t pro_id, thr_id;
		process_arg_t* process;
		struct pt_regs *regs = task_pt_regs(current);
		fiber_arg_t* fiber = (fiber_arg_t*) kmalloc(sizeof(fiber_arg_t), GFP_KERNEL);
		thread_arg_t* thread;

		pro_id = current->tgid;
		thr_id = current->pid;

		process = search_process(pro_id);
		if(process == NULL){
			process = (process_arg_t*) kmalloc(sizeof(process_arg_t), GFP_KERNEL);
			process->fiber_id = 0;
			process->pid = pro_id;
			hash_add_rcu(list_process, &process->p_list, process->pid);
			hash_init(process->threads);
			hash_init(process->fibers);
		}

		thread = search_thread(thr_id, process);
		if(thread == NULL){
			thread = (thread_arg_t*) kmalloc(sizeof(thread_arg_t), GFP_KERNEL);
			thread->pid = thr_id;
			hash_add_rcu(process->threads, &thread->t_list, thread->pid);

			fiber->regs = (struct pt_regs*) kmalloc (sizeof(struct pt_regs), GFP_KERNEL);
			if(!fiber) return -1;

			fiber->index = __sync_fetch_and_add(&process->fiber_id, 1);
			memcpy(fiber->regs, regs, sizeof(struct pt_regs));

			thread->active_fiber = fiber;
			fiber->running = true;
			fiber->fpu_reg = (struct fpu *) kmalloc (sizeof(struct fpu), GFP_KERNEL); 
			copy_fxregs_to_kernel(fiber->fpu_reg);
			hash_add_rcu(process->fibers, &fiber->f_list, fiber->index);
			return fiber->index;
		}
		return -1;
	}
	
	else if (cmd == CREATEFIBER){
		pid_t pro_id, thr_id;
		create_arg_t* args = (create_arg_t*) kmalloc (sizeof(create_arg_t), GFP_KERNEL);
		fiber_arg_t* fiber = (fiber_arg_t*) kmalloc(sizeof(fiber_arg_t), GFP_KERNEL);
		process_arg_t* process;
		thread_arg_t* thread;

		//sanity check access_ok
		copy_from_user(args, (void *)arg, sizeof(create_arg_t));

		pro_id = current->tgid;
		thr_id = current->pid;

		process = search_process(pro_id);
		if(process == NULL)return -1;

		thread = search_thread(thr_id, process);
		if(thread != NULL){
			fiber->running = false;
			fiber->index = __sync_fetch_and_add(&process->fiber_id, 1);
			
			fiber->regs = (struct pt_regs*)kmalloc(sizeof(struct pt_regs), GFP_KERNEL);
			memcpy(fiber->regs, task_pt_regs(current), sizeof(struct pt_regs));

			fiber->regs->sp = (unsigned long)args->dwStackPointer;
			fiber->regs->ip = (unsigned long)args->lpStartAddress;
			fiber->regs->di = (unsigned long)args->lpParameter;
			fiber->regs->bp = fiber->regs->sp;
			hash_add_rcu(process->fibers, &fiber->f_list, fiber->index);
			fiber->fpu_reg = (struct fpu *) kmalloc (sizeof(struct fpu), GFP_KERNEL); 
			copy_fxregs_to_kernel(fiber->fpu_reg);
			return fiber->index;
		}
		return -1;
	}
	
	else if (cmd == SWITCHTOFIBER){
		int i, pro_id, thr_id, ind = (int)arg;
		int exist = 0;
		process_arg_t* node, *proc;
		thread_arg_t* cursor, *thre;
		fiber_arg_t* temp, *fibe;

		pro_id = current->tgid;
		thr_id = current->pid;

		//check if the current process is presente in the hashtable
		hash_for_each(list_process, i, node , p_list){
			if(node->pid == pro_id){
				exist = 1;
				proc = node;
			}
		}
		if(exist == 0) return -1;

		exist = 0;
		hash_for_each(proc->fibers, i, temp , f_list){
			if(temp->index == ind && !temp->running){
				exist = 1;
				fibe = temp;
			}
		}
		if(exist == 0) return -1;

		exist = 0;

		//check if the current thread is present in the hastable, 
		//if it is we know that the thread has already compute ConverteToThread
		hash_for_each(proc->threads, i, cursor , t_list){
			if(cursor->pid == thr_id){
				exist = 1;
				thre = cursor;
			}
		}
		if(exist == 1){
			//swicth the context to the found fiber
			//struct pt_regs *regs = task_pt_regs(current);
			//modifica direttamente da qui, switch anche fpu
			struct pt_regs* reg = task_pt_regs(current);
			memcpy(thre->active_fiber->regs, reg, sizeof(struct pt_regs));
			fibe->fpu_reg = (struct fpu *) kmalloc (sizeof(struct fpu), GFP_KERNEL); 
			preempt_disable();
			thre->active_fiber->regs->r15 = reg->r15;
			thre->active_fiber->regs->r14 = reg->r14;
			thre->active_fiber->regs->r13 = reg->r13;
			thre->active_fiber->regs->r12 = reg->r12;
			thre->active_fiber->regs->r11 = reg->r11;
			thre->active_fiber->regs->r10 = reg->r10;
			thre->active_fiber->regs->r9 = reg->r9;
			thre->active_fiber->regs->r8 = reg->r8;
			thre->active_fiber->regs->ax = reg->ax;
			thre->active_fiber->regs->bx = reg->bx;
			thre->active_fiber->regs->cx = reg->cx;
			thre->active_fiber->regs->dx = reg->dx;
			thre->active_fiber->regs->si = reg->si;
			thre->active_fiber->regs->di = reg->di;
			thre->active_fiber->regs->sp = reg->sp;
			thre->active_fiber->regs->ip = reg->ip;
			thre->active_fiber->regs->bp = reg->bp;
			thre->active_fiber->regs->flags = reg->flags;
			copy_fxregs_to_kernel(thre->active_fiber->fpu_reg);

			reg->r15 = fibe->regs->r15;
			reg->r14 = fibe->regs->r14;
			reg->r13 = fibe->regs->r13;
			reg->r12 = fibe->regs->r12;
			reg->r11 = fibe->regs->r11;
			reg->r10 = fibe->regs->r10;
			reg->r9 = fibe->regs->r9;
			reg->r8 = fibe->regs->r8;
			reg->ax = fibe->regs->ax;
			reg->bx = fibe->regs->bx;
			reg->cx = fibe->regs->cx;
			reg->dx = fibe->regs->dx;
			reg->si = fibe->regs->si;
			reg->di = fibe->regs->di;
			reg->sp = fibe->regs->sp;
			reg->ip = fibe->regs->ip;
			reg->bp = fibe->regs->bp;
			reg->flags = fibe->regs->flags;
			copy_kernel_to_fxregs(&(fibe->fpu_reg->state.fxsave));
			preempt_enable();
		}


		return -1;
	}
	
	else if (cmd == FLSALLOC){
		process_arg_t *process;
		thread_arg_t *thread;
		int pro_id, thr_id;

		pro_id = current->tgid;
		thr_id = current->pid;

		process = search_process(pro_id);
		if(process == NULL)return -1;

		thread = search_thread(thr_id, process);
		if(thread != NULL){
			fiber_arg_t* fiber = thread->active_fiber;
			long ret = __sync_fetch_and_add (&fiber->fls_in, 1);
			if(ret >= MAX_FLS)
				return -1;
			return ret;
		}
		return -1;
	}
	
	else if (cmd == FLSFREE){
		return true;
	}
	
	else if (cmd == FLSGETVALUE){
		//returnt the value in the fls slot using the index passed as input 
		long long ret;
		long ind = (long) arg;

		if(ind < 0 || ind >= fls_id) return -1;

		spin_lock_irq(&lock_fls);
		ret = fls_array[ind];
		spin_unlock_irq(&lock_fls);
		return ret;
	}
	
	else if (cmd == FLSSETVALUE){
		int rc;
		long ind;

		fls_set_arg_t* args = (fls_set_arg_t*) kmalloc (sizeof(fls_set_arg_t), GFP_KERNEL);
		rc = copy_from_user(args, (void *)arg, sizeof(fls_set_arg_t));

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
