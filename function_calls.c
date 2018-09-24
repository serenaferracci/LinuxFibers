#include "fiber_ioctl.h"
 
void ConvertThreadToFiber()
{
    fiber_arg_t q;
 
    if (ioctl(fd, CONVERTTOFIBER, &q) == -1)
    {
        perror("query_apps ioctl get");
    }
}