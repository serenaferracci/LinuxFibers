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
#include "declarations.h"
 
#define FIRST_MINOR 0
#define MINOR_CNT 1
 
static dev_t dev;
static struct cdev c_dev;
static struct class *cl;
static struct kprobe probe;


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
	thread_arg_t* thread;
	hash_for_each_possible(process->threads, thread, t_list, thr_id){
		if(thread->pid == thr_id) {
			return thread;
		}
	}
	return NULL;
}

fiber_arg_t* search_fiber(int index, process_arg_t* process){
	fiber_arg_t *fiber;
	hash_for_each_possible(process->fibers, fiber, f_list, index){
		if(fiber->index == index && !fiber->running) {
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
	pro_id = current->tgid;
	thr_id = current->pid;
	process = search_process(pro_id);
	if(process == NULL) return 0;
	thread = search_thread(thr_id, process);
	if(thread == NULL) return 0; 
	fiber = thread->active_fiber;
	hash_del(&(thread->t_list));
	hash_del(&(fiber->f_list));
	kfree(thread);
	kfree(fiber);
	return 0;
} 
 

static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	
	if (cmd == CONVERTTOFIBER){

		pid_t pro_id, thr_id;
		process_arg_t* process;
		fiber_arg_t* fiber = (fiber_arg_t*) kzalloc(sizeof(fiber_arg_t), GFP_KERNEL);
		thread_arg_t* thread;

		pro_id = current->tgid;
		printk("pid: %d\n", pro_id);
		thr_id = current->pid;

		process = search_process(pro_id);
		if(process == NULL){
			process = (process_arg_t*) kzalloc(sizeof(process_arg_t), GFP_KERNEL);
			process->fiber_id = 0;
			process->pid = pro_id;
			hash_add(list_process, &process->p_list, process->pid);
			hash_init(process->threads);
			hash_init(process->fibers);
		}

		thread = search_thread(thr_id, process);
		if(thread == NULL){
			thread = (thread_arg_t*) kzalloc(sizeof(thread_arg_t), GFP_KERNEL);
			thread->pid = thr_id;
			hash_add(process->threads, &thread->t_list, thread->pid);

			fiber->index = __sync_fetch_and_add(&process->fiber_id, 1);
			snprintf(fiber->name, 8, "%lu", fiber->index);
			printk("name: %s\n", fiber->name);
			memcpy(&(fiber->regs), task_pt_regs(current), sizeof(struct pt_regs));
			fiber->running = true;
			memset((char *)&(fiber->fpu_reg), 0, sizeof(struct fpu));
			copy_fxregs_to_kernel(&(fiber->fpu_reg));
			hash_add(process->fibers, &fiber->f_list, fiber->index);
			thread->active_fiber = fiber;
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
		spin_lock_irq(&lock_fiber);

		pro_id = current->tgid;
		thr_id = current->pid;
		process = search_process(pro_id);
		if(process == NULL) {
			spin_unlock_irq(&lock_fiber);
			return -1;
		}
		fiber = search_fiber(fib_id, process);
		if(fiber == NULL) {
			spin_unlock_irq(&lock_fiber);
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
			thread->active_fiber = fiber;
			fiber->running = true;
			spin_unlock_irq(&lock_fiber);
			return 0;
		}
		spin_unlock_irq(&lock_fiber);
		return -1;
	}
	
	else if (cmd == FLSALLOC){
		process_arg_t *process;
		thread_arg_t *thread;
		int pro_id, thr_id;

		pro_id = current->tgid;
		thr_id = current->pid;

		process = search_process(pro_id);
		if(process == NULL) return -1;

		thread = search_thread(thr_id, process);
		if(thread != NULL){
			fiber_arg_t* fiber = thread->active_fiber;
			long index = find_first_zero_bit(fiber->fls_bitmap, MAX_FLS);
			change_bit(index, fiber->fls_bitmap);
			fiber->fls_array[index] = 0;
			return index;
		}
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
			clear_bit(index, fiber->fls_bitmap);
			return true;
		}
		return false;
	}
	
	else if (cmd == FLSGETVALUE){
		int pro_id, thr_id, err;
		long index;
		process_arg_t *process;
		thread_arg_t *thread;
		fls_arg_t* args = (fls_arg_t*) kzalloc (sizeof(fls_arg_t), GFP_KERNEL);
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
    void* proc_readdir;
    void* proc_lookup;
 
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

	proc_readdir = (void*)kallsyms_lookup_name("proc_pident_readdir");
	proc_lookup = (void*)kallsyms_lookup_name("proc_pident_lookup");
    //probe.addr = (kprobe_opcode_t *)proc_readdir;
    register_kprobe(&probe); 
	fh_init();
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
}
 
module_init(fiber_ioctl_init);
module_exit(fiber_ioctl_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Serena Ferracci");
MODULE_DESCRIPTION("Fiber ioctl() Char Driver");