#ifndef FIBER_IOCTL_H
#define FIBER_IOCTL_H
#include <linux/ioctl.h>

typedef struct
{
    unsigned long dwStackSize;
    void* lpStartAddress;
    void* lpParameter;
} create_arg_t;

typedef struct
{
    long dwFlsIndex;
    long long lpFlsData;
} fls_set_arg_t;


typedef struct
{
	bool running;
	int index;
	void* param;
	struct pt_regs* regs;
    struct task_struct* task;
    struct list_head f_list ;
	
} fiber_arg_t;

LIST_HEAD(listStart);

#define DEFINE_SPINLOCK(x) spinlock_t x = __SPIN_LOCK_UNLOCKED(x)

DEFINE_SPINLOCK(init_lock);

#define MAX_FLS 4096
#define MAX_FIBER 4096
 
#define CONVERTTOFIBER       _IO('q', 1)
#define CREATEFIBER 		 _IO('q', 2)
#define SWITCHTOFIBER 		 _IO('q', 3)
#define FLSALLOC 			 _IO('q', 4)
#define FLSFREE				 _IO('q', 5)
#define FLSGETVALUE			 _IO('q', 6)
#define FLSSETVALUE			 _IO('q', 7)
 
#endif
