#ifndef PROC_H
#define PROC_H

#include <linux/proc_fs.h>

struct ftrace_hook {
	const char *name;
	void *function;
	void *original;

	unsigned long address;
	struct ftrace_ops ops;
};

union proc_op {
	int (*proc_get_link)(struct dentry *, struct path *);
	int (*proc_show)(struct seq_file *m,
		struct pid_namespace *ns, struct pid *pid,
		struct task_struct *task);
};

struct pid_entry {
	const char *name;
	unsigned int len;
	umode_t mode;
	const struct inode_operations *iop;
	const struct file_operations *fop;
	union proc_op op;
};

struct proc_inode {
	struct pid *pid;
	unsigned int fd;
	union proc_op op;
	struct proc_dir_entry *pde;
	struct ctl_table_header *sysctl;
	struct ctl_table *sysctl_entry;
	struct hlist_node sysctl_inodes;
	const struct proc_ns_operations *ns_ops;
	struct inode vfs_inode;
} __randomize_layout;



int fibers_readdir (struct file *, struct dir_context *);

typedef struct dentry * (*proc_pident_lookup_t)(struct inode *dir, 
					 struct dentry *dentry,
					 const struct pid_entry *ents,
					 unsigned int nents);

typedef int (*proc_pident_readdir_t)(struct file *file, struct dir_context *ctx,
					 const struct pid_entry *ents,
					 unsigned int nents);

typedef int (*proc_setattr_t)(struct dentry *dentry, struct iattr *attr);

typedef int (*pid_getattr_t)(const struct path *path, struct kstat *stat,
		u32 request_mask, unsigned int query_flags);

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


extern spinlock_t lock_fiber;
int switch_handler(struct kretprobe_instance *ri, struct pt_regs *regs);
int fh_install_hook (struct ftrace_hook *hook);
void fh_remove_hook (struct ftrace_hook *hook);
int fh_init(void);
void fh_exit(void);

#endif