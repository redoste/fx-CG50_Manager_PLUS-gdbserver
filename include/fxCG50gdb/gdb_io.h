#ifndef GDB_IO_H
#define GDB_IO_H

#include <stdint.h>

#include <fxCG50gdb/stdio.h>

enum gdb_io_read_mode {
	READ_WAIT_ALL,
	READ_PEEK,
};

void gdb_io_init(void);
int gdb_io_accept(void);
ssize_t gdb_io_recv(char* buffer, size_t buffer_size, enum gdb_io_read_mode read_mode);
ssize_t gdb_io_send(const char* buffer, size_t buffer_size);

#define GDB_IO_DEFAULT_INTERFACE "tcp"
#define GDB_IO_DEFAULT_INTERFACE_INDEX 0
#define GDB_IO_DEFAULT_INTERFACE_OPTION L"31188"

#endif
