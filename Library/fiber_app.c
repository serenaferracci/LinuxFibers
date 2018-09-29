#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdlib.h>
 
#include "function_calls.h"

#define num_fibers 5
 
int main(int argc, char *argv[])
{
	long fibers = FlsAlloc();
    printf("%ld\n", fibers);
    
    long fibers1 = FlsAlloc();
    printf("%ld\n", fibers1);
    return 0;
}
