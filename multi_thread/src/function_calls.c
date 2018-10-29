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
#ifndef USERSPACE

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
    return (void *)ret;
}

void* CreateFiber(unsigned long dwStackSize, void* lpStartAddress,void* lpParameter)
{
	create_arg_t* args = (create_arg_t*) malloc(sizeof(create_arg_t));
	void* stack;
    void* stack_pointer;
    int err = posix_memalign(&stack, 16, dwStackSize);
    if(err) return (void*)-1;
    stack_pointer = (void *)((unsigned long)stack + dwStackSize - 8);
	args->dwStackPointer = stack_pointer;
	args->lpStartAddress = lpStartAddress;
	args->lpParameter = lpParameter;
	
	if (done == 0) init_file();
    unsigned long ret = (unsigned long)ioctl(fd, CREATEFIBER, args);
    free(args);
    return (void *)ret;
}

void SwitchToFiber(void* lpFiber)
{   
	if (done == 0) init_file();
    ioctl(fd, SWITCHTOFIBER, (unsigned long) lpFiber);
    return;
}

long FlsAlloc()
{   
	if (done == 0) init_file();
    long ret = ioctl(fd, FLSALLOC);
    return ret;
}

bool FlsFree(long dwFlsIndex)
{   
	if (done == 0) init_file();
    if (ioctl(fd, FLSFREE, dwFlsIndex) == -1){
        return false;
    }
    return true;
}


void* FlsGetValue(long dwFlsIndex)
{   
	if (done == 0) init_file();

    fls_arg_t* args = (fls_arg_t*)malloc(sizeof(fls_arg_t));
    args->dwFlsIndex = dwFlsIndex;
    int ret = ioctl(fd, FLSGETVALUE, args);
    if (ret == -1){
        return NULL;
    }
    if(args->lpFlsData == 0) return NULL;
    return (void *)args->lpFlsData;
}

void FlsSetValue(long dwFlsIndex, void* lpFlsData)
{   
	fls_arg_t* args = (fls_arg_t*)malloc(sizeof(fls_arg_t));
	
	args->dwFlsIndex = dwFlsIndex;
	args->lpFlsData = (unsigned long)lpFlsData;
	
	if (done == 0) init_file();

    ioctl(fd, FLSSETVALUE, args);
    free(args);
    return;
}

#endif