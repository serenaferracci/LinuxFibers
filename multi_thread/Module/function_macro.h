#ifndef FUNCTION_MACRO
#define FUNCTION_MACRO

#include <linux/ioctl.h>

typedef struct
{
    void* dwStackPointer;
    void* lpStartAddress;
    void* lpParameter;
} create_arg_t;

typedef struct
{
    long dwFlsIndex;
    unsigned long lpFlsData;
} fls_set_arg_t;

#define CONVERTTOFIBER       _IO('q', 1)
#define CREATEFIBER 		 _IO('q', 2)
#define SWITCHTOFIBER 		 _IO('q', 3)
#define FLSALLOC 			 _IO('q', 4)
#define FLSFREE				 _IO('q', 5)
#define FLSGETVALUE			 _IO('q', 6)
#define FLSSETVALUE			 _IO('q', 7)

#endif