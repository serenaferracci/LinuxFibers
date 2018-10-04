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
 
#include "fiber_ioctl.h"
#include "function_macro.h"
 
#define FIRST_MINOR 0
#define MINOR_CNT 1
#define STACKSIZE (8 * 1024 * 1024)
#define ALIGN_SIZE 16

 
static dev_t dev;
static struct cdev c_dev;
static struct class *cl;
static struct kprobe probe;
 
static int my_open(struct inode *i, struct file *f){
	return 0;
}

static int my_close(struct inode *i, struct file *f){
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

fiber_arg_t* search_fiber(int index, process_arg_t* process){
	fiber_arg_t *node, *fiber;
	int exist = 0;
	hash_for_each_possible_rcu(process->fibers, node, f_list, index){
		if(node->index == index && !node->running) {
			exist = 1;
			fiber = node;
		}
	}
	if(exist == 1) return fiber;
	return NULL;
}


int Pre_Handler(struct kprobe *p, struct pt_regs *regs){ 
	printk("Pre Hendler kprob thread id: %d\n", current->pid);
	thread_arg_t* thread;
	process_arg_t* process;
	pid_t pro_id, thr_id;
	pro_id = current->tgid;
	thr_id = current->pid;

	process = search_process(pro_id);
	if(process == NULL) return 0;

	thread = search_thread(thr_id, process);
	if(thread == NULL) return 0; 
	printk("Thread is present in the hashmap");
	return 0;
} 
 
void Post_Handler(struct kprobe *p, struct pt_regs *regs, unsigned long flags){ 
} 


static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
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
		int pro_id, thr_id, fib_id = (unsigned long)arg;
		process_arg_t* process;
		thread_arg_t* thread;
		fiber_arg_t* fiber;

		pro_id = current->tgid;
		thr_id = current->pid;
		process = search_process(pro_id);
		if(process == NULL) return -1;
		fiber = search_fiber(fib_id, process);
		if(fiber == NULL) return -1;
		thread = search_thread(thr_id, process);
		if(thread != NULL){
			struct pt_regs* reg = task_pt_regs(current);
			fiber->fpu_reg = (struct fpu *) kmalloc (sizeof(struct fpu), GFP_KERNEL); 
			preempt_disable();
			thread->active_fiber->regs->r15 = reg->r15;
			thread->active_fiber->regs->r14 = reg->r14;
			thread->active_fiber->regs->r13 = reg->r13;
			thread->active_fiber->regs->r12 = reg->r12;
			thread->active_fiber->regs->r11 = reg->r11;
			thread->active_fiber->regs->r10 = reg->r10;
			thread->active_fiber->regs->r9 = reg->r9;
			thread->active_fiber->regs->r8 = reg->r8;
			thread->active_fiber->regs->ax = reg->ax;
			thread->active_fiber->regs->bx = reg->bx;
			thread->active_fiber->regs->cx = reg->cx;
			thread->active_fiber->regs->dx = reg->dx;
			thread->active_fiber->regs->si = reg->si;
			thread->active_fiber->regs->di = reg->di;
			thread->active_fiber->regs->sp = reg->sp;
			thread->active_fiber->regs->ip = reg->ip;
			thread->active_fiber->regs->bp = reg->bp;
			thread->active_fiber->regs->flags = reg->flags;
			copy_fxregs_to_kernel(thread->active_fiber->fpu_reg);

			reg->r15 = fiber->regs->r15;
			reg->r14 = fiber->regs->r14;
			reg->r13 = fiber->regs->r13;
			reg->r12 = fiber->regs->r12;
			reg->r11 = fiber->regs->r11;
			reg->r10 = fiber->regs->r10;
			reg->r9 = fiber->regs->r9;
			reg->r8 = fiber->regs->r8;
			reg->ax = fiber->regs->ax;
			reg->bx = fiber->regs->bx;
			reg->cx = fiber->regs->cx;
			reg->dx = fiber->regs->dx;
			reg->si = fiber->regs->si;
			reg->di = fiber->regs->di;
			reg->sp = fiber->regs->sp;
			reg->ip = fiber->regs->ip;
			reg->bp = fiber->regs->bp;
			reg->flags = fiber->regs->flags;
			copy_kernel_to_fxregs(&(fiber->fpu_reg->state.fxsave));
			preempt_enable();
			thread->active_fiber->running = false;
			thread->active_fiber = fiber;
			fiber->running = true;
			return 0;
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
			int index = find_first_zero_bit(fiber->fls_bitmap, MAX_FLS);
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
		if(process == NULL)return false;

		thread = search_thread(thr_id, process);
		if(thread != NULL){
			fiber_arg_t* fiber = thread->active_fiber;
			clear_bit(index, fiber->fls_bitmap);
			return true;
		}
		return false;
	}
	
	else if (cmd == FLSGETVALUE){
		process_arg_t *process;
		thread_arg_t *thread;
		int pro_id, thr_id;
		long index = (long) arg;

		pro_id = current->tgid;
		thr_id = current->pid;

		process = search_process(pro_id);
		if(process == NULL)return -1;

		thread = search_thread(thr_id, process);
		if(thread != NULL){
			fiber_arg_t* fiber = thread->active_fiber;
			if(test_bit(index, fiber->fls_bitmap)){
				return fiber->fls_array[index];
			}
		}
		return -1;
	}
	
	else if (cmd == FLSSETVALUE){
		long index;
		process_arg_t *process;
		thread_arg_t *thread;
		int pro_id, thr_id;

		fls_set_arg_t* args = (fls_set_arg_t*) kmalloc (sizeof(fls_set_arg_t), GFP_KERNEL);
		copy_from_user(args, (void *)arg, sizeof(fls_set_arg_t));

		index = args->dwFlsIndex;

		pro_id = current->tgid;
		thr_id = current->pid;

		process = search_process(pro_id);
		if(process == NULL)return -1;

		thread = search_thread(thr_id, process);
		if(thread != NULL){
			fiber_arg_t* fiber = thread->active_fiber;
			if(test_bit(index, fiber->fls_bitmap)){
				fiber->fls_array[index] = args->lpFlsData;
			}
		}
	}

    return 0;
}
 
static struct file_operations fiber_fops =
{
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_close,
    .unlocked_ioctl = my_ioctl
};
 
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
    if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, "fiber"))){
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(dev_ret);
    }
	hash_init(list_process);

    probe.pre_handler = Pre_Handler; 
    probe.post_handler = Post_Handler; 
    probe.addr = (kprobe_opcode_t *)do_exit; 
    register_kprobe(&probe); 
    return 0;
}
 
static void __exit fiber_ioctl_exit(void)
{
    device_destroy(cl, dev);
    class_destroy(cl);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev, MINOR_CNT);
	unregister_kprobe(&probe);
}
 
module_init(fiber_ioctl_init);
module_exit(fiber_ioctl_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Serena Ferracci");
MODULE_DESCRIPTION("Fiber ioctl() Char Driver");
