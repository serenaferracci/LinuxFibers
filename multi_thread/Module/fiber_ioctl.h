#ifndef FIBER_IOCTL_H
#define FIBER_IOCTL_H

#include <linux/hashtable.h>
#include <linux/bitmap.h>
#include <linux/ftrace.h>
#include <linux/time.h>

#define MAX_FLS 4096

typedef struct
{
	bool running;
	unsigned long index;
    unsigned long thread_id;
    unsigned long starting_point;
    char name[8];
	struct pt_regs regs;
    struct hlist_node f_list;
    long fls_in;
    DECLARE_BITMAP(set_bitmap, sizeof(MAX_FLS));
    DECLARE_BITMAP(fls_bitmap, sizeof(MAX_FLS));
    unsigned long fls_array[MAX_FLS];
    struct fpu fpu_reg;
    unsigned long activations;
    unsigned long failed_activations;
    unsigned long starting_time;
    unsigned long total_time;
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


process_arg_t* search_process(pid_t proc_id);
fiber_arg_t* search_fiber(int index, process_arg_t* process);

#endif
