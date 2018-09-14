#ifndef QUERY_IOCTL_H
#define QUERY_IOCTL_H
#include <linux/ioctl.h>
 
typedef struct
{
    size_t dwStackSize;
    void lpStartAddress;
    void lpParameter;
    void lpFiber;
    long dwFlsIndex;
    long long value;
} fiber_arg_t;
 
#define ConvertThreadToFiber() _IO('q', 1, fiber_arg_t*)
#define CreateFiber(dwStackSize, lpStartAddress, lpParameter) _IO('q', 2, fiber_arg_t*)
#define SwitchToFiber(lpFiber) _IO('q', 3, fiber_arg_t*)
#define FlsAlloc(lpCallback) _IO('q', 4, fiber_arg_t*)
#define FlsFree(dwFlsIndex)_IO('q', 5, fiber_arg_t*)
#define FlsGetValue(dwFlsIndex) _IOR('q', 6, fiber_arg_t*)
#define FlsSetValue(dwFlsIndex, lpFlsData) _IOW('q', 7, fiber_arg_t*)
 
#endif