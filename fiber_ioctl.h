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
#define CREATEFIBER 		 _IO('q', 2)
#define SWITCHTOFIBER 		 _IO('q', 3)
#define FLSALLOC 			 _IO('q', 4)
#define FLSFREE				 _IO('q', 5)
#define FLSGETVALUE			 _IO('q', 6)
#define FLSSETVALUE			 _IO('q', 7)
 
#endif
