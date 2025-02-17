#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#include "../Module/function_macro.h"
#include "function_calls.h"

int fd;
int done = 0;
void init_file(){
    fd = open(FILE_NAME, O_RDWR);
    if (fd == -1)
    {
        printf("Impossible to open the file descriptor\n");
        return;
    }
    done = 1;
}


 
void* ConvertThreadToFiber()
{	
	if (done == 0) init_file();
    unsigned long ret = (unsigned long)ioctl(fd, CONVERTTOFIBER);
    if ((unsigned long)ret == (unsigned long)-1){
        printf("Unable to convert the thread\n");
        return (void*)-1;
    }
    return (void *)ret;
}

void* CreateFiber(unsigned long dwStackSize, void* lpStartAddress,void* lpParameter)
{
	create_arg_t* args = (create_arg_t*) malloc(sizeof(create_arg_t));
	void* stack;
    void* stack_pointer;
    posix_memalign(&stack, 16, dwStackSize);
    stack_pointer = (void *)((unsigned long)stack + dwStackSize - 8);
	args->dwStackPointer = stack_pointer;
	args->lpStartAddress = lpStartAddress;
	args->lpParameter = lpParameter;
	
	if (done == 0) init_file();
    unsigned long ret = (unsigned long)ioctl(fd, CREATEFIBER, args);
    if ((unsigned long)ret == (unsigned long)-1){
        printf("Unable to create a new fiber\n");
        free(args);
        return (void *)-1;
    }
    free(args);
    return (void *)ret;
}

void SwitchToFiber(void* lpFiber)
{
	if (done == 0) init_file();
    if (ioctl(fd, SWITCHTOFIBER, (unsigned long) lpFiber) == -1)
    {
        printf("Unable to switch to the given fiber\n");
    }
    return;
}

long FlsAlloc()
{
	if (done == 0) init_file();
    long ret = ioctl(fd, FLSALLOC);
    if (ret == -1){
        printf("Impossible to alloc the fls\n");
        return -1;
    }
    return ret;
}

bool FlsFree(long dwFlsIndex)
{
	if (done == 0) init_file();

    if (ioctl(fd, FLSFREE, dwFlsIndex) == -1){
        printf("Impossible to free\n");
        return false;
    }
    return true;
}


unsigned long FlsGetValue(long dwFlsIndex)
{
	if (done == 0) init_file();

    long long ret = ioctl(fd, FLSGETVALUE, dwFlsIndex);
    if (ret == -1){
        printf("Impossible to get the value\n");  
        return -1;
    }
    return ret;
}

void FlsSetValue(long dwFlsIndex, long long lpFlsData)
{
	fls_set_arg_t* args = (fls_set_arg_t*)malloc(sizeof(fls_set_arg_t));
	
	args->dwFlsIndex = dwFlsIndex;
	args->lpFlsData = lpFlsData;
	
	if (done == 0) init_file();

    if (ioctl(fd, FLSSETVALUE, args) == -1){
        printf("Impossible to set the value\n");
    }
    free(args);
    return;
}
