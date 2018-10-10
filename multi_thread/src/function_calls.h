#ifndef __FUNCTION_CALLS 
#define __FUNCTION_CALLS 

#include <stdbool.h>
#include "../Module/function_macro.h"

#define FILE_NAME  "/dev/fiber"
#define STACK_SIZE (4096*2)
 
void* ConvertThreadToFiber();

void* CreateFiber(unsigned long dwStackSize, void* lpStartAddress,void* lpParameter);

void SwitchToFiber(void* lpFiber);

long FlsAlloc();

bool FlsFree(long dwFlsIndex);

void* FlsGetValue(long dwFlsIndex);

void FlsSetValue(long dwFlsIndex, void* lpFlsData);

#endif
