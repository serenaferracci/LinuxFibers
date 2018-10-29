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
#include <linux/kprobes.h>
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>

#include "fiber_ioctl.h"
#include "function_macro.h"
#include "proc.h"
#include "fiber_ioctl.h"
 
#define FIRST_MINOR 0
#define MINOR_CNT 1
 
static dev_t dev;
static struct cdev c_dev;
static struct class *cl;
static struct kprobe probe;
long count_threads;

DECLARE_HASHTABLE(list_process, 10);

process_arg_t* search_process(pid_t proc_id){
	process_arg_t* process;
	hash_for_each_possible(list_process, process, p_list, proc_id){
		if(process->pid == proc_id) {
			return process;
		}
	}
	return NULL;
}

thread_arg_t* search_thread(pid_t thr_id, process_arg_t* process){
	unsigned long flags;
	thread_arg_t* thread;
	spin_lock_irqsave(&(process->lock_hash), flags);
	hash_for_each_possible(process->threads, thread, t_list, thr_id){
		if(thread->pid == thr_id) {
			spin_unlock_irqrestore(&(process->lock_hash), flags);
			return thread;
		}
	}
	spin_unlock_irqrestore(&(process->lock_hash), flags);
	return NULL;
}

fiber_arg_t* search_fiber(int index, process_arg_t* process){
	fiber_arg_t *fiber;
	hash_for_each_possible(process->fibers, fiber, f_list, index){
		if(fiber->index == index) {
			return fiber;
		}
	}
	return NULL;
}


int Pre_Handler(struct kprobe *p, struct pt_regs *regs){ 
	thread_arg_t* thread;
	process_arg_t* process;
	fiber_arg_t* fiber;
	pid_t pro_id, thr_id;
	unsigned long flags;
	pro_id = current->tgid;
	thr_id = current->pid;
	process = search_process(pro_id);
	if(process == NULL) return 0;
	thread = search_thread(thr_id, process);
	if(thread == NULL) return 0;
	__sync_fetch_and_sub(&count_threads, 1);
	fiber = thread->active_fiber;
	spin_lock_irqsave(&(process->lock_hash), flags);
	hash_del(&(thread->t_list));
	kfree(thread);
	spin_unlock_irqrestore(&(process->lock_hash), flags);
	hash_del(&(fiber->f_list));
	kfree(fiber);
	if(count_threads == 0){
		hash_del(&(process->p_list));
		kfree(process);
		printk("All data structures are freed\n");
	}
	return 0;
} 
 

static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	
	if (cmd == CONVERTTOFIBER){
		struct timespec ts;
		unsigned long flags;
		pid_t pro_id, thr_id;
		process_arg_t* process;
		thread_arg_t* thread;
		fiber_arg_t* fiber = (fiber_arg_t*) kzalloc(sizeof(fiber_arg_t), GFP_KERNEL);

		pro_id = current->tgid;
		printk("pid: %d\n", pro_id);
		thr_id = current->pid;

		process = search_process(pro_id);
		if(process == NULL){
			process = (process_arg_t*) kzalloc(sizeof(process_arg_t), GFP_KERNEL);
			if (!process) return -ENOMEM;
			process->fiber_id = 0;
			process->pid = pro_id;
			hash_add(list_process, &process->p_list, process->pid);
			hash_init(process->threads);
			hash_init(process->fibers);
		}
		thread = search_thread(thr_id, process);
		if(thread == NULL){
			thread = (thread_arg_t*) kzalloc(sizeof(thread_arg_t), GFP_KERNEL);
			if (!thread) return -ENOMEM;
			thread->pid = thr_id;
			spin_lock_irqsave(&(process->lock_hash), flags);
			hash_add(process->threads, &thread->t_list, thread->pid);
			spin_unlock_irqrestore(&(process->lock_hash), flags);
			fiber->index = __sync_fetch_and_add(&process->fiber_id, 1);
			fiber->thread_id = thr_id;
			fiber->starting_point = task_pt_regs(current)->ip;
			snprintf(fiber->name, 8, "%lu", fiber->index);
			memcpy(&(fiber->regs), task_pt_regs(current), sizeof(struct pt_regs));
			fiber->running = true;
			fiber->activations = 1;
			fiber->failed_activations = 0;
			bitmap_zero(fiber->fls_bitmap, MAX_FLS);
			getnstimeofday(&ts);
			fiber->starting_time = (unsigned long)ts.tv_nsec + (unsigned long)ts.tv_sec*1000000000;
			fiber->total_time = 0;
			memset((char *)&(fiber->fpu_reg), 0, sizeof(struct fpu));
			copy_fxregs_to_kernel(&(fiber->fpu_reg));
			hash_add(process->fibers, &fiber->f_list, fiber->index);
			thread->active_fiber = fiber;
			__sync_fetch_and_add(&count_threads, 1);
			return fiber->index;
		}
		return -1;
	}
	
	else if (cmd == CREATEFIBER){
		pid_t pro_id, thr_id;
		create_arg_t* args = (create_arg_t*) kzalloc (sizeof(create_arg_t), GFP_KERNEL);
		fiber_arg_t* fiber = (fiber_arg_t*) kzalloc(sizeof(fiber_arg_t), GFP_KERNEL);
		process_arg_t* process;
		thread_arg_t* thread;
		int err;

		err = access_ok(VERIFY_READ, (void *)arg, sizeof(create_arg_t));
		if(!err) return -1;
		err = copy_from_user(args, (void *)arg, sizeof(create_arg_t));
		if(err) return -1;

		pro_id = current->tgid;
		thr_id = current->pid;

		process = search_process(pro_id);
		if(process == NULL) return -1;

		thread = search_thread(thr_id, process);
		if(thread != NULL){
			fiber->running = false;
			fiber->index = __sync_fetch_and_add(&process->fiber_id, 1);
			fiber->activations = 0;
			fiber->failed_activations = 0;
			fiber->thread_id = thr_id;
			fiber->starting_point = (unsigned long)args->lpStartAddress;
			fiber->starting_time = 0;
			fiber->total_time = 0;
			bitmap_zero(fiber->fls_bitmap, MAX_FLS);			
			snprintf(fiber->name, 8, "%lu", fiber->index);
			memcpy(&(fiber->regs), task_pt_regs(current), sizeof(struct pt_regs));
			fiber->regs.sp = (unsigned long)args->dwStackPointer;
			fiber->regs.ip = (unsigned long)args->lpStartAddress;
			fiber->regs.di = (unsigned long)args->lpParameter;
			fiber->regs.bp = fiber->regs.sp;
			memset((char *)&(fiber->fpu_reg), 0, sizeof(struct fpu));
			copy_fxregs_to_kernel(&(fiber->fpu_reg));

			hash_add(process->fibers, &fiber->f_list, fiber->index);
			kfree(args);
			return fiber->index;
		}
		kfree(args);
		return -1;
	}
	
	else if (cmd == SWITCHTOFIBER){
		int pro_id, thr_id;
		unsigned long fib_id = (unsigned long)arg;
		process_arg_t* process;
		thread_arg_t* thread;
		fiber_arg_t* fiber;
		unsigned long flags;
		struct timespec ts;

		pro_id = current->tgid;
		thr_id = current->pid;
		process = search_process(pro_id);
		if(process == NULL) {
			return -1;
		}
		spin_lock_irqsave(&(process->lock_fiber), flags);
		fiber = search_fiber(fib_id, process);
		if(fiber == NULL) {
			spin_unlock_irqrestore(&(process->lock_fiber), flags);
			return -1;
		}
		if(fiber->running){
			fiber->failed_activations += 1;
			spin_unlock_irqrestore(&(process->lock_fiber), flags);
			return -1;
		}
		thread = search_thread(thr_id, process);
		if(thread != NULL){ 
			struct fpu* fpu_reg;
			
			memcpy(&(thread->active_fiber->regs), task_pt_regs(current), sizeof(struct pt_regs));
			copy_fxregs_to_kernel(&(thread->active_fiber->fpu_reg));

			memcpy(task_pt_regs(current), &(fiber->regs), sizeof(struct pt_regs));
			fpu_reg = &(fiber->fpu_reg);
			copy_kernel_to_fxregs(&(fpu_reg->state.fxsave));

			thread->active_fiber->running = false;
			if (thread->active_fiber->starting_time != 0){
				unsigned long time_fiber;
				getnstimeofday(&ts);
				time_fiber = (unsigned long)ts.tv_nsec + (unsigned long)ts.tv_sec*1000000000 - thread->active_fiber->starting_time;
				time_fiber /= 1000;
				thread->active_fiber->total_time += time_fiber;
			}
			thread->active_fiber = fiber;
			fiber->running = true;
			fiber->activations += 1;
			getnstimeofday(&ts);
			fiber->starting_time = (unsigned long)ts.tv_nsec + (unsigned long)ts.tv_sec*1000000000;
			spin_unlock_irqrestore(&(process->lock_fiber), flags);
			return 0;
		}
		spin_unlock_irqrestore(&(process->lock_fiber), flags);
		return -1;
	}
	
	else if (cmd == FLSALLOC){
		process_arg_t *process;
		thread_arg_t *thread;
		int pro_id, thr_id;

		pro_id = current->tgid;
		thr_id = current->pid;

		process = search_process(pro_id);
		if(process == NULL) {
			printk("non trova processo\n");
			return -1;
		}
		
		thread = search_thread(thr_id, process);
		if(thread != NULL){
			fiber_arg_t* fiber = thread->active_fiber;
			long index = find_first_zero_bit(fiber->fls_bitmap, MAX_FLS);
			if (index == MAX_FLS) {
			printk("non trova index\n");
			return -1;
		}
			change_bit(index, fiber->fls_bitmap);
			fiber->fls_array[index] = 0;
			return index;
		}
		printk("non trova il thread\n");
		return -1;
	}
	
	else if (cmd == FLSFREE){
		process_arg_t *process;
		thread_arg_t *thread;
		int pro_id, thr_id;

		long index = (long)arg;

		pro_id = current->tgid;
		thr_id = current->pid;

		process = search_process(pro_id);
		if(process == NULL) return false;

		thread = search_thread(thr_id, process);
		if(thread != NULL){
			fiber_arg_t* fiber = thread->active_fiber;
			if(index > 0 && index < MAX_FLS){
				clear_bit(index, fiber->fls_bitmap);
				return true;
			}
		}
		return false;
	}
	
	else if (cmd == FLSGETVALUE){
		int pro_id, thr_id, err;
		long index;
		process_arg_t *process;
		thread_arg_t *thread;
		fls_arg_t* args = (fls_arg_t*) kzalloc (sizeof(fls_arg_t), GFP_KERNEL);
		if (!args) return -ENOMEM;
		err = access_ok(VERIFY_READ, (void *)arg, sizeof(fls_arg_t));
		if(!err) return -1;
		err = copy_from_user(args, (void *)arg, sizeof(fls_arg_t));
		if(err) return -1;
		index = args->dwFlsIndex;
		pro_id = current->tgid;
		thr_id = current->pid;

		process = search_process(pro_id);
		if(process == NULL) return -1;
		thread = search_thread(thr_id, process);
		if(thread != NULL){
			fiber_arg_t* fiber = thread->active_fiber;
			if(test_bit(index, fiber->fls_bitmap)){
				int err;
				unsigned long ret = fiber->fls_array[index];
				args->lpFlsData = ret;
				err = access_ok(VERIFY_WRITE, (void *)arg, sizeof(fls_arg_t));
				if(!err) return -1;
				err = copy_to_user((void *)arg, args, sizeof(fls_arg_t));
				if(err) return -1;
				kfree(args);
				return 1;
			}
		}
		kfree(args);
		return -1;
	}
	
	else if (cmd == FLSSETVALUE){
		long index;
		process_arg_t *process;
		thread_arg_t *thread;
		int pro_id, thr_id;
		int err;

		fls_arg_t* args = (fls_arg_t*) kzalloc (sizeof(fls_arg_t), GFP_KERNEL);
		if (!args) return -ENOMEM;
		err = access_ok(VERIFY_READ, (void *)arg, sizeof(fls_arg_t));
		if(!err) return -1;
		err = copy_from_user(args, (void *)arg, sizeof(fls_arg_t));
		if(err) return -1;

		index = args->dwFlsIndex;

		pro_id = current->tgid;
		thr_id = current->pid;

		process = search_process(pro_id);
		if(process == NULL) return -1;

		thread = search_thread(thr_id, process);
		if(thread != NULL){
			fiber_arg_t* fiber = thread->active_fiber;
			if(test_bit(index, fiber->fls_bitmap)){
				fiber->fls_array[index] = args->lpFlsData;
			}
		}
		kfree(args);
	}
    return 0;
}
 
static struct file_operations fiber_fops =
{
    .owner = THIS_MODULE,
    .unlocked_ioctl = my_ioctl
};

static char *unlock_sudo(struct device *dev, umode_t *mode){
    if (mode) *mode = 0666;
    return kasprintf(GFP_KERNEL, "%s", dev_name(dev));
}
 
static int __init fiber_ioctl_init(void)
{
    int ret;
    struct device *dev_ret;
 
    if ((ret = alloc_chrdev_region(&dev, FIRST_MINOR, MINOR_CNT, "fiber_ioctl")) < 0) return ret; 
    cdev_init(&c_dev, &fiber_fops);
 
    if ((ret = cdev_add(&c_dev, dev, MINOR_CNT)) < 0) return ret;
     
    if (IS_ERR(cl = class_create(THIS_MODULE, "char"))){
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(cl);
    }
	cl->devnode = unlock_sudo;
    if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, "fiber"))){
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(dev_ret);
    }

	hash_init(list_process);
    probe.pre_handler = Pre_Handler;  
    probe.addr = (kprobe_opcode_t *)do_exit; 
    register_kprobe(&probe); 
	if ((ret = fh_init()) < 0) return -1;
	pr_info("module loaded\n");	
    return 0;
}
 
static void __exit fiber_ioctl_exit(void)
{
    device_destroy(cl, dev);
    class_destroy(cl);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev, MINOR_CNT);
	unregister_kprobe(&probe);
	fh_exit();
	pr_info("module unloaded\n");
}
 
module_init(fiber_ioctl_init);
module_exit(fiber_ioctl_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Serena Ferracci -- ferracci.1649134@studenti.uniroma1.it");
MODULE_DESCRIPTION("Fibers implementation in the Linux Kernel");