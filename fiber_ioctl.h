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
 
#define CONVERTTOFIBER       _IO('q', 1)
#define CreateFiber 		 _IO('q', 2)
#define SwitchToFiber 		 _IO('q', 3)
#define FlsAlloc 			 _IO('q', 4)
#define FlsFree				 _IO('q', 5)
#define FlsGetValue			 _IO('q', 6)
#define FlsSetValue			 _IO('q', 7)
 
#endif
