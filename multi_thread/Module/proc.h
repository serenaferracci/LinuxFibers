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



int fh_install_hook (struct ftrace_hook *hook);

void fh_remove_hook (struct ftrace_hook *hook);

int fh_init(void);

void fh_exit(void);

#endif