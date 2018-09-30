#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../Module/fiber_ioctl.h"
#include "function_calls.h"

 
int ConvertThreadToFiber()
{	
	int fd = open(FILE_NAME, O_RDWR);
    if (fd == -1)
    {
        perror("convert to fiber open");
        return -1;
    }
    int ret = ioctl(fd, CONVERTTOFIBER);
    if (ret == -1)
    {
        perror("convert to fiber ioctl get");
        close (fd);
        return -1;
    }
    
    close (fd);
    return ret;
}

int CreateFiber(unsigned long dwStackSize, void* lpStartAddress,void* lpParameter)
{
	create_arg_t* args = (create_arg_t*) malloc(sizeof(create_arg_t));
	
	args->dwStackSize = dwStackSize;
	args->lpStartAddress = lpStartAddress;
	args->lpParameter = lpParameter;
	
	int fd = open(FILE_NAME, O_RDWR);
    if (fd == -1)
    {
        perror("create fiber open");
        return -1;
    }
    int ret = ioctl(fd, CREATEFIBER, args);
    if (ret == -1)
    {
        perror("create fiber ioctl get");
        close (fd);
        return -1;
    }
    
    close (fd);
    return ret;
}

int SwitchToFiber(int lpFiber)
{
	int fd = open(FILE_NAME, O_RDWR);
    if (fd == -1)
    {
        perror("switch fiber open");
        return -1;
    }

    if (ioctl(fd, SWITCHTOFIBER, &lpFiber) == -1)
    {
        perror("switch fiber ioctl get");
        close (fd);
        return -1;
    }
    
    close (fd);
    return 1;
}

long FlsAlloc()
{
	int fd = open(FILE_NAME, O_RDWR);
    if (fd == -1)
    {
        perror("alloc fiber open");
        return -1;
    }
    long ret = ioctl(fd, FLSALLOC);
    if (ret == -1)
    {
        perror("alloc fiber ioctl get");
        close (fd);
        return -1;
    }
    
    close (fd);
    return ret;
}

bool FlsFree(long dwFlsIndex)
{
	int fd = open(FILE_NAME, O_RDWR);
    if (fd == -1)
    {
        perror("free fiber open");
        return false;
    }

    if (ioctl(fd, FLSFREE) == -1)
    {
        perror("free fiber ioctl get");
        close (fd);
        return false;
    }
    
    close (fd);
    return true;
}


long long FlsGetValue(long dwFlsIndex)
{
	int fd = open(FILE_NAME, O_RDWR);
    if (fd == -1)
    {
        perror("get fiber open");
        return -1;
    }

    long long ret = ioctl(fd, FLSALLOC, &dwFlsIndex);

    if (ret == -1)
    {
        perror("get fiber ioctl get");
        close (fd);
        return -1;
    }
    
    close (fd);
    return ret;
}

void FlsSetValue(long dwFlsIndex, long long lpFlsData)
{
	fls_set_arg_t* args = (fls_set_arg_t*)malloc(sizeof(fls_set_arg_t));
	
	args->dwFlsIndex = dwFlsIndex;
	args->lpFlsData = lpFlsData;
	
	int fd = open(FILE_NAME, O_RDWR);
    if (fd == -1)
    {
        perror("set fiber open");
        return;
    }

    if (ioctl(fd, FLSALLOC, &args) == -1)
    {
        perror("set fiber ioctl get");
    }
    
    close (fd);
    return;
}
