#ifndef FIBER_IOCTL_H
#define FIBER_IOCTL_H

#include <linux/hashtable.h>

typedef struct
{
	bool running;
	int index;
	void* param;
	struct pt_regs* regs;
    struct hlist_node f_list;
	
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

DECLARE_HASHTABLE(list_process, 10);
DEFINE_SPINLOCK(lock_fls);

#define MAX_FLS 4096
#define MAX_FIBER 4096
 
#endif
