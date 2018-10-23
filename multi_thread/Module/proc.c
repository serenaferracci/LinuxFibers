#include <linux/ftrace.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs_struct.h>
#include <linux/dcache.h>
#include <linux/pid.h>
#include <linux/seq_file.h>
#include "proc.h"
#include "fiber_ioctl.h"

static inline struct proc_inode *PROC_I(const struct inode *inode)
{
	return container_of(inode, struct proc_inode, vfs_inode);
}

static inline struct pid *proc_pid(struct inode *inode)
{
	return PROC_I(inode)->pid;
}

static inline struct task_struct *get_proc_task(struct inode *inode)
{
	return get_pid_task(proc_pid(inode), PIDTYPE_PID);
}

struct inode_operations fibers_iop;
struct file_operations fibers_fop;
struct file_operations file_fop;

int show_file(struct seq_file *m, void *v){
	void* data = m->private;
	fiber_arg_t* fiber = (fiber_arg_t*)data;
	if(fiber->running) seq_printf(m, "The fiber is currently running\n");
	else seq_printf(m, "The fiber is not currently running\n");
	seq_printf(m, "Entry point: %p\n", (void*)fiber->starting_point);
	seq_printf(m, "Thread id: %lu\n", fiber->thread_id);
	seq_printf(m, "Number of activations: %lu\n", fiber->activations);
	seq_printf(m, "Number of  falied activations: %lu\n", fiber->failed_activations);
	seq_printf(m, "Execution time: %lu\n", fiber->total_time);
	/*
– total execution time in that Fiber context*/
	return 0;
}

int open_file (struct inode *inode, struct file *filp){
	pid_t pid_process;
	struct task_struct *task;
	process_arg_t *process;
	fiber_arg_t *fiber;
	unsigned long fiber_id;
	task = get_proc_task(file_inode(filp));
	pid_process = task->tgid;
	kstrtoul(filp->f_path.dentry->d_name.name, 10, &fiber_id);
	process = search_process(pid_process);
	fiber = search_fiber(fiber_id, process);
	return single_open(filp, show_file, fiber);
}

int fibers_readdir (struct file * file, struct dir_context * ctx){
	pid_t pid_process;
	struct task_struct *task;
	process_arg_t *process;
	struct pid_entry *ents_fiber;
	unsigned long flags;
	int i;
	fiber_arg_t *fiber;
	long num_fibers;
	proc_pident_readdir_t readdir;
	readdir = (proc_pident_readdir_t)kallsyms_lookup_name("proc_pident_readdir");
	
	
	task = get_proc_task(file_inode(file));
	if(task == NULL) return 0;
	pid_process = task->tgid;
	process = search_process(pid_process);

	if(process != NULL){
		spin_lock_irqsave(&lock_fiber, flags);
		num_fibers = process->fiber_id;
		ents_fiber = (struct pid_entry *) kzalloc (sizeof(struct pid_entry)*(num_fibers), GFP_KERNEL);
		spin_unlock_irqrestore(&lock_fiber, flags);
		hash_for_each_rcu(process->fibers, i, fiber, f_list){
			unsigned long id = fiber->index;
			ents_fiber[id].name = fiber->name;
			ents_fiber[id].len = strnlen(fiber->name, 8);
			ents_fiber[id].mode = S_IFREG|S_IRUGO|S_IXUGO;
			ents_fiber[id].iop = NULL;
			file_fop.open = open_file;
			file_fop.read = seq_read;
			file_fop.llseek	= seq_lseek;
			file_fop.release = single_release;
			ents_fiber[id].fop = &file_fop;
		}
		return readdir(file, ctx, ents_fiber, num_fibers);
	}
	return readdir(file, ctx, NULL, 0);
}

struct dentry* fibers_lookup (struct inode *dir, struct dentry *dentry, unsigned int flags){
	pid_t pid_process;
	struct task_struct *task;
	process_arg_t *process;
	struct pid_entry *ents_fiber;
	unsigned long flags_lock;
	int i;
	fiber_arg_t *fiber;
	long num_fibers;
	proc_pident_lookup_t real_lookup;

	task = get_proc_task(dir);
	pid_process = task->tgid;
	if(task == NULL) return NULL;
	process = search_process(pid_process);
	spin_lock_irqsave(&lock_fiber, flags_lock);
	num_fibers = process->fiber_id;
	ents_fiber = (struct pid_entry *) kzalloc (sizeof(struct pid_entry)*(num_fibers), GFP_KERNEL);
	spin_unlock_irqrestore(&lock_fiber, flags_lock);
	hash_for_each_rcu(process->fibers, i, fiber, f_list){
		unsigned long id = fiber->index;
		ents_fiber[id].name = fiber->name;
		ents_fiber[id].len = strnlen(fiber->name, 8);
		ents_fiber[id].mode = S_IFREG|S_IRUGO|S_IXUGO;
		ents_fiber[id].iop = NULL;
		file_fop.open = open_file;
		file_fop.read = seq_read;
		file_fop.llseek	= seq_lseek;
		file_fop.release = single_release;
		ents_fiber[id].fop = &file_fop;
	}
	real_lookup = (proc_pident_lookup_t) kallsyms_lookup_name("proc_pident_lookup");
	return real_lookup(dir, dentry, ents_fiber, num_fibers);
}

static int fh_resolve_hook_address(struct ftrace_hook *hook)
{
	hook->address = kallsyms_lookup_name(hook->name);

	if (!hook->address) {
		pr_debug("unresolved symbol: %s\n", hook->name);
		return -ENOENT;
	}
	*((unsigned long*) hook->original) = hook->address;
	return 0;
}

static void notrace fh_ftrace_thunk(unsigned long ip, unsigned long parent_ip,
		struct ftrace_ops *ops, struct pt_regs *regs)
{
	struct ftrace_hook *hook = container_of(ops, struct ftrace_hook, ops);
	if (!within_module(parent_ip, THIS_MODULE))
		regs->ip = (unsigned long) hook->function;
}

int fh_install_hook(struct ftrace_hook *hook)
{
	int err;

	err = fh_resolve_hook_address(hook);
	if (err) return err;

	hook->ops.func = fh_ftrace_thunk;
	hook->ops.flags = FTRACE_OPS_FL_SAVE_REGS
	                | FTRACE_OPS_FL_RECURSION_SAFE
	                | FTRACE_OPS_FL_IPMODIFY;

	err = ftrace_set_filter_ip(&hook->ops, hook->address, 0, 0);
	if (err) {
		pr_debug("ftrace_set_filter_ip() failed: %d\n", err);
		return err;
	}

	err = register_ftrace_function(&hook->ops);
	if (err) {
		pr_debug("register_ftrace_function() failed: %d\n", err);
		ftrace_set_filter_ip(&hook->ops, hook->address, 1, 0);
		return err;
	}
	return 0;
}

void fh_remove_hook(struct ftrace_hook *hook)
{
	int err;
	err = unregister_ftrace_function(&hook->ops);
	if (err) pr_debug("unregister_ftrace_function() failed: %d\n", err);
	err = ftrace_set_filter_ip(&hook->ops, hook->address, 1, 0);
	if (err) pr_debug("ftrace_set_filter_ip() failed: %d\n", err);
}


int fh_install_hooks(struct ftrace_hook *hooks, size_t count)
{
	int err;
	size_t i;

	for (i = 0; i < count; i++) {
		err = fh_install_hook(&hooks[i]);
		if (err){
			while (i != 0) fh_remove_hook(&hooks[--i]);
			return err;
		}
	}
	return 0;
}

void fh_remove_hooks(struct ftrace_hook *hooks, size_t count)
{
	size_t i;
	for (i = 0; i < count; i++)
		fh_remove_hook(&hooks[i]);
}

static asmlinkage struct dentry* (*real_lookup)(struct inode *dir, struct dentry *dentry,
					 const struct pid_entry *ents, unsigned int nents);

static asmlinkage  struct dentry* fh_lookup(struct inode *dir, struct dentry *dentry,
					 const struct pid_entry *ents, unsigned int nents)
{
	struct dentry* ret = NULL;
	pid_t pid_process;
	struct pid_entry fiber;
	struct task_struct *task;
	process_arg_t *process;
	const struct pid_entry *ents_fiber;
	struct pid_entry *temp = (struct pid_entry *) kzalloc (sizeof(struct pid_entry)*(nents+1), GFP_KERNEL);
	
	task = get_proc_task(dir);
	pid_process = task->tgid;
	process = search_process(pid_process);
	
	if(process != NULL){
		fibers_fop.iterate_shared = fibers_readdir;
		fibers_fop.read = generic_read_dir;
		fibers_fop.llseek = generic_file_llseek;
		fibers_iop.lookup = fibers_lookup;
		fibers_iop.getattr = (pid_getattr_t) kallsyms_lookup_name("pid_getattr");
		fibers_iop.setattr = (proc_setattr_t) kallsyms_lookup_name("proc_setattr");
		memcpy((void*)temp, ents,sizeof(struct pid_entry)*nents);
		fiber.name = "fiber";
		fiber.len  = sizeof("fiber") - 1;
		fiber.mode = S_IFDIR|S_IRUGO|S_IXUGO;
		fiber.iop  = &fibers_iop;
		fiber.fop  = &fibers_fop;
		temp[nents] = fiber;
		ents_fiber = (struct pid_entry *) kzalloc (sizeof(struct pid_entry)*(nents+1), GFP_KERNEL);
		memcpy((void*)ents_fiber, temp ,sizeof(struct pid_entry)*(nents+1));
		ret = real_lookup(dir, dentry, ents_fiber, nents+1);
		kfree(ents_fiber);
	}
	else{
		ret = real_lookup(dir, dentry, ents, nents);
	}
	kfree(temp);
	return ret;
}



static asmlinkage int (*real_readdir)(struct file *file, struct dir_context *ctx,
		const struct pid_entry *ents, unsigned int nents);

static asmlinkage int fh_readdir(struct file *file, struct dir_context *ctx,
		const struct pid_entry *ents, unsigned int nents)
{
	int ret = 0;
	pid_t pid_process;
	struct pid_entry fiber;
	struct task_struct *task;
	process_arg_t *process;
	const struct pid_entry *ents_fiber;
	struct pid_entry *temp = (struct pid_entry *) kzalloc (sizeof(struct pid_entry)*(nents+1), GFP_KERNEL);
	task = get_proc_task(file_inode(file));
	pid_process = task->tgid;
	process = search_process(pid_process);
	if(process != NULL && strncmp(file->f_path.dentry->d_name.name, "fiber", strlen("fiber")) != 0){
		fibers_fop.iterate_shared = fibers_readdir;
		fibers_fop.read = generic_read_dir;
		fibers_fop.llseek = generic_file_llseek;
		fibers_iop.lookup = fibers_lookup;
		fibers_iop.getattr = (pid_getattr_t) kallsyms_lookup_name("pid_getattr");
		fibers_iop.setattr = (proc_setattr_t) kallsyms_lookup_name("proc_setattr");
		memcpy((void*)temp, ents,sizeof(struct pid_entry)*nents);
		fiber.name = "fiber";
		fiber.len  = sizeof("fiber") - 1;
		fiber.mode = S_IFDIR|S_IRUGO|S_IXUGO;
		fiber.iop  = &fibers_iop;
		fiber.fop  = &fibers_fop;
		temp[nents] = fiber;
		ents_fiber = (struct pid_entry *) kzalloc (sizeof(struct pid_entry)*(nents+1), GFP_KERNEL);
		memcpy((void*)ents_fiber, temp ,sizeof(struct pid_entry)*(nents+1));
		ret = real_readdir(file, ctx,ents_fiber, nents+1);
		kfree(ents_fiber);
	}
	else{
		ret = real_readdir(file, ctx, ents, nents);
	}
	kfree(temp);
	return ret;
}


#define HOOK(_name, _function, _original)	\
	{					\
		.name = (_name),		\
		.function = (_function),	\
		.original = (_original),	\
	}

static struct ftrace_hook demo_hooks[] = {
	HOOK("proc_pident_readdir",  fh_readdir,  &real_readdir),
	HOOK("proc_pident_lookup", fh_lookup, &real_lookup),
};

int fh_init(void)
{
	int err;
	err = fh_install_hooks(demo_hooks, ARRAY_SIZE(demo_hooks));
	if (err) return err;
	pr_info("module loaded\n");
	return 0;
}

void fh_exit(void)
{
	fh_remove_hooks(demo_hooks, ARRAY_SIZE(demo_hooks));
	pr_info("module unloaded\n");
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Serena Ferracci");
MODULE_DESCRIPTION("Fiber ioctl() Char Driver");
