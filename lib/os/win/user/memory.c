#include <lib/helpers/memory.h>
#include <memory.h>

void copy_mem(void* destination, const void* source, size_t length) {
    memcpy(destination, source, length);
}

void fill_mem(void * mem, size_t length, int fill_value) {
    memset(mem, fill_value, length);
}