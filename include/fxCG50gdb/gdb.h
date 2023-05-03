#ifndef GDB_H
#define GDB_H

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>
#include <winsock2.h>

#define GDB_SERVER_PORT 31188
#define GDB_SIGNAL_TRAP 5

// Comment this line to disable "send" and "recv" debug output
// This may speed up the session
#define GDB_SEND_RECV_DEBUG

// Comment this line to disable non-standard fxCG50gdb's features
#define GDB_NON_STANDARD_FEATURES

// We store registers in BIG-ENDIAN in this structure since they are going to be sent to GDB as is
struct gdb_registers {
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
	uint32_t r4;
	uint32_t r5;
	uint32_t r6;
	uint32_t r7;
	uint32_t r8;
	uint32_t r9;
	uint32_t r10;
	uint32_t r11;
	uint32_t r12;
	uint32_t r13;
	uint32_t r14;
	uint32_t r15;
	uint32_t pc;
	uint32_t pr;
	uint32_t gbr;
	uint32_t vbr;
	uint32_t mach;
	uint32_t macl;
	uint32_t sr;
};

void gdb_start(void);
void gdb_main(bool program_started);

extern SOCKET gdb_client_socket;
extern bool gdb_wants_step;
extern uint8_t* gdb_breakpoints[128];
extern size_t gdb_breakpoints_amount;

#endif
