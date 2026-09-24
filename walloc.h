#ifndef WALLOC_H
#define WALLOC_H

#include <stddef.h>

void *walloc(size_t size);
void winfree(void *ptr);

#endif
