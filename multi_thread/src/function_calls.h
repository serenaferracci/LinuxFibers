#ifndef __FUNCTION_CALLS 
#define __FUNCTION_CALLS 

#include <stdbool.h>
#include "../Module/function_macro.h"

#define FILE_NAME  "/dev/fiber"
#define STACK_SIZE (4096*2)

#define USERSPACE

#ifdef USERSPACE

#include "ult.h"

#define ConvertThreadToFiber() ult_convert()
#define CreateFiber(dwStackSize, lpStartAddress, lpParameter) ult_creat(dwStackSize, lpStartAddress, lpParameter)
#define SwitchToFiber(lpFiber) ult_switch_to(lpFiber)
#define FlsAlloc(lpCallback) fls_alloc()
#define FlsFree(dwFlsIndex)	fls_free(dwFlsIndex)
#define FlsGetValue(dwFlsIndex) fls_get(dwFlsIndex)
#define FlsSetValue(dwFlsIndex, lpFlsData) fls_set((dwFlsIndex), (long long)(lpFlsData))

#else
 
void* ConvertThreadToFiber();
void* CreateFiber(unsigned long dwStackSize, void* lpStartAddress,void* lpParameter);
void SwitchToFiber(void* lpFiber);
long FlsAlloc();
bool FlsFree(long dwFlsIndex);
void* FlsGetValue(long dwFlsIndex);
void FlsSetValue(long dwFlsIndex, void* lpFlsData);

#endif
#endif
