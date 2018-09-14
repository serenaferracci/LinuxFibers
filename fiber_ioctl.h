#ifndef FIBER_IOCTL_H
#define FIBER_IOCTL_H
#include <linux/ioctl.h>
 
typedef struct
{
    size_t dwStackSize;
    void* lpStartAddress;
    void* lpParameter;
    void* lpFiber;
    long dwFlsIndex;
    long long value;
} fiber_arg_t;
 
#define ConvertThreadToFiber _IOR('q', 1, fiber_arg_t*)
#define CreateFiber 		 _IO('q', 2, fiber_arg_t*)
#define SwitchToFiber 		 _IO('q', 3, fiber_arg_t*)
#define FlsAlloc 			 _IO('q', 4, fiber_arg_t*)
#define FlsFree				 _IO('q', 5, fiber_arg_t*)
#define FlsGetValue			 _IOR('q', 6, fiber_arg_t*)
#define FlsSetValue			 _IOW('q', 7, fiber_arg_t*)
 
#endif
