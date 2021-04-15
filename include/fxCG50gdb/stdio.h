#ifndef STDIO_H
#define STDIO_H

#include <stdio.h>

#define fxCG50gdb_printf(...) printf("[fxCG50gdb] " __VA_ARGS__)

#if defined(_MSC_VER)
typedef signed int ssize_t;  // We should only build on 32-bit anyway
#endif

#endif
