#ifndef FIBER_IOCTL_H
#define FIBER_IOCTL_H

#include <linux/hashtable.h>
#include <linux/bitmap.h>
#include <linux/ftrace.h>
#include <linux/time.h>

#define MAX_FLS 4096

typedef struct
{
    DECLARE_BITMAP(fls_bitmap, sizeof(MAX_FLS));
    struct pt_regs regs;
    struct hlist_node f_list;
    struct fpu fpu_reg;
	unsigned long index;
    unsigned long thread_id;
    unsigned long starting_point;
    unsigned long fls_array[MAX_FLS];
    unsigned long activations;
    unsigned long failed_activations;
    unsigned long starting_time;
    unsigned long total_time;
    char name[8];
    bool running;
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
thread_arg_t* search_thread(pid_t thr_id, process_arg_t* process);
fiber_arg_t* search_fiber(int index, process_arg_t* process);

#endif
