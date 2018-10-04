#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdlib.h>
 
#include "function_calls.h"

#define num_fibers 5
 
 void hello(){
    printf("Hello call\n");
    SwitchToFiber(0);
    exit(0);
}

 void hello1(){
    printf("Hello call2\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    /*void* fiber = ConvertThreadToFiber();
    printf("%ld\n", (unsigned long)fiber);*/

	void* fibers = CreateFiber(8*2048, hello1, NULL);
    printf("%ld\n", (unsigned long)fibers);

    void* fibers2 = CreateFiber(8*2048, hello, NULL);
    printf("%ld\n", (unsigned long)fibers2);

    SwitchToFiber(fibers2);
    
    long fibers1 = FlsAlloc();
    printf("First alloc: %ld\n", fibers1);

    bool ret = FlsFree(fibers1);
    printf("Free : %d\n", ret);

    long fibers3 = FlsAlloc();
    printf("Second alloc: %ld\n", fibers3);

    FlsSetValue(fibers1, 3);

    unsigned long result = FlsGetValue(fibers1);
    if((long)result == -1) printf("Error\n");
    else printf("Return get value: %ld\n", result);
    return 0;
}
