#ifndef __FUNCTION_CALLS 
#define __FUNCTION_CALLS 

#include <stdbool.h>
#include "../Module/fiber_ioctl.h"

#define FILE_NAME  "/dev/fiber"
 
int ConvertThreadToFiber();

int CreateFiber(unsigned long dwStackSize, void* lpStartAddress,void* lpParameter);

int SwitchToFiber(int lpFiber);

long FlsAlloc();

bool FlsFree(long dwFlsIndex);

long long FlsGetValue(long dwFlsIndex);

void FlsSetValue(long dwFlsIndex, long long lpFlsData);

#endif
