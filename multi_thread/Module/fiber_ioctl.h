#ifndef FIBER_IOCTL_H
#define FIBER_IOCTL_H

#include <linux/hashtable.h>
#include <linux/bitmap.h>
#include <linux/ftrace.h>

#define MAX_FLS 4096
static struct kprobe probe_proc;

typedef struct
{
	bool running;
	unsigned long index;
	struct pt_regs regs;
    struct hlist_node f_list;
    long fls_in;
    DECLARE_BITMAP(set_bitmap, sizeof(MAX_FLS));
    DECLARE_BITMAP(fls_bitmap, sizeof(MAX_FLS));
    unsigned long fls_array[MAX_FLS];
    struct fpu fpu_reg;
} fiber_arg_t;

typedef struct
{
	pid_t pid;
	fiber_arg_t* active_fiber;
    struct hlist_node t_list;
	
} thread_arg_t;

typedef struct
{
    pid_t pid;
    struct hlist_node p_list;
    DECLARE_HASHTABLE(fibers, 10);
    DECLARE_HASHTABLE(threads, 10);
    long fiber_id;
} process_arg_t;

struct ftrace_hook {
        const char *name;
        void *function;
        void *original;
 
        unsigned long address;
        struct ftrace_ops ops;
};

DECLARE_HASHTABLE(list_process, 10);
DEFINE_SPINLOCK(lock_fiber);
 
#endif
