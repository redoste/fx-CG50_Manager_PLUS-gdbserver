#include <assert.h>

#include <fxCG50gdb/emulator.h>
#include <fxCG50gdb/serial.h>
#include <fxCG50gdb/stdio.h>

FILE* serial_output_file;
void (*serial_old_handler)();

void serial_init() {
	serial_output_file = stderr;

	void (**handlers)() = real_cpu_SCFTDR_handlers();
	assert(handlers[0] == handlers[1]);
	serial_old_handler = handlers[0];
	handlers[0] = serial_handler;
	handlers[1] = serial_handler;
}
