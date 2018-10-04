#ifndef __FUNCTION_CALLS 
#define __FUNCTION_CALLS 

#include <stdbool.h>
#include "../Module/function_macro.h"

#define FILE_NAME  "/dev/fiber"
 
void* ConvertThreadToFiber();

void* CreateFiber(unsigned long dwStackSize, void* lpStartAddress,void* lpParameter);

void SwitchToFiber(void* lpFiber);

long FlsAlloc();

bool FlsFree(long dwFlsIndex);

unsigned long FlsGetValue(long dwFlsIndex);

void FlsSetValue(long dwFlsIndex, long long lpFlsData);

#endif
