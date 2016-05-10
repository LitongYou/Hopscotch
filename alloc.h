#ifndef ALLOC_H
#define ALLOC_H

#include <stdlib.h>

#define ALLOCATE(_size) malloc(_size)

#define DEALLOCATE(_pointer) free(_pointer)

#endif
