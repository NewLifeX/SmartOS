#ifndef __Memory_H__
#define __Memory_H__

#include "Sys.h"

#ifdef DEBUG

void* operator new(uint size);
void operator delete(void * p);
void operator delete [] (void * p);

//#define DEBUG_NEW new(__FILE__, __LINE__)
//#define new DEBUG_NEW
#endif

#endif
