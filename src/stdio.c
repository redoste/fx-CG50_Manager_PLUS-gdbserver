#include <stdarg.h>
#include <stdio.h>

void fxCG50gdb_printf(const char* format, ...) {
	va_list args;
	va_start(args, format);
	printf("[fxCG50gdb] ");
	vprintf(format, args);
	va_end(args);
}
