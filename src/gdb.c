#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include <fxCG50gdb/bswap.h>
#include <fxCG50gdb/emulator.h>
#include <fxCG50gdb/gdb.h>
#include <fxCG50gdb/gdb_io.h>
#include <fxCG50gdb/mmu.h>
#include <fxCG50gdb/stdio.h>

static HANDLE gdb_io_mutex;
bool gdb_wants_step = false;

// This stupid lock should never be used but we keep it for safety
static bool gdb_SIGINT_thread_is_running = false;
static unsigned long __stdcall gdb_SIGINT_thread(void* lpParameter) {
	(void)lpParameter;

	gdb_SIGINT_thread_is_running = true;
	char buf;
	fxCG50gdb_printf("gdb_SIGINT_thread : started\n");

	assert(gdb_io_recv(&buf, 1, READ_PEEK) >= 1);
	if (buf == 0x03) {
		fxCG50gdb_printf("gdb_SIGINT_thread : recv SIGINT : breaking\n");
		gdb_wants_step = true;
	}

	fxCG50gdb_printf("gdb_SIGINT_thread : ended\n");
	gdb_SIGINT_thread_is_running = false;
	return 0;
}

static void gdb_start_SIGINT_thread(void) {
	if (!gdb_SIGINT_thread_is_running)
		CreateThread(NULL, 0, &gdb_SIGINT_thread, NULL, 0, NULL);
}

static unsigned long __stdcall gdb_start_thread(void* lpParameter) {
	(void)lpParameter;
	assert(WaitForSingleObject(gdb_io_mutex, INFINITE) == WAIT_OBJECT_0);

	if (gdb_io_accept() < 0) {
		ExitProcess(-1);
		return 0;
	}

	gdb_main(false);
	assert(ReleaseMutex(gdb_io_mutex));
	return 0;
}

void gdb_start(void) {
	gdb_io_mutex = CreateMutexA(NULL, FALSE, NULL);
	CreateThread(NULL, 0, &gdb_start_thread, NULL, 0, NULL);
}

uint8_t* gdb_breakpoints[128] = {NULL};
size_t gdb_breakpoints_amount = 0;
static void gdb_add_breakpoint(uint32_t address) {
	fxCG50gdb_printf("gdb_add_breakpoint : New breakpoint at 0x%08X (bp@0x%p)\n", address, (void*)gdb_breakpoints);
	address = address >> 1;
	uint8_t prefix = address >> 24;
	uint32_t suffix = address & 0xFFFFFF;
	if (gdb_breakpoints[prefix] == NULL) {
		// We use about 2MiB per region, we should probably found an other fast way to store/use breakpoints but
		// less memory intensive
		gdb_breakpoints[prefix] = malloc(0x1000000 >> 3);
		memset(gdb_breakpoints[prefix], 0, 0x1000000 >> 3);
	}
	if (!(gdb_breakpoints[prefix][suffix >> 3] & (1 << (suffix & 7)))) {
		gdb_breakpoints[prefix][suffix >> 3] |= 1 << (suffix & 7);
		gdb_breakpoints_amount += 1;
	}
	fxCG50gdb_printf("gdb_add_breakpoint : address=0x%08X p=0x%02X s=0x%06X bp@0x%p bp=0x%02X amount=%d\n", address,
			 prefix, suffix, &gdb_breakpoints[prefix][suffix >> 3], gdb_breakpoints[prefix][suffix >> 3],
			 gdb_breakpoints_amount);
}

static void gdb_del_breakpoint(uint32_t address) {
	fxCG50gdb_printf("gdb_del_breakpoint : Remove breakpoint at 0x%08X (bp@0x%p)\n", address,
			 (void*)gdb_breakpoints);
	address = address >> 1;
	uint8_t prefix = address >> 24;
	uint32_t suffix = address & 0xFFFFFF;
	if (gdb_breakpoints[prefix] != NULL) {
		if (gdb_breakpoints[prefix][suffix >> 3] & (1 << (suffix & 7))) {
			gdb_breakpoints[prefix][suffix >> 3] &= ~(1 << (suffix & 7));
			gdb_breakpoints_amount -= 1;
		}
		fxCG50gdb_printf("gdb_del_breakpoint : address=0x%08X p=0x%02X s=0x%06X bp@0x%p bp=0x%02X amount=%d\n",
				 address, prefix, suffix, &gdb_breakpoints[prefix][suffix >> 3],
				 gdb_breakpoints[prefix][suffix >> 3], gdb_breakpoints_amount);
	}
	// Currently we don't garbage collect gdb_breakpoints, if the user puts and delete a lot of breakpoints, this
	// might be heavy on memory usage
}

static int gdb_recv_packet(char* buf, size_t buf_len, size_t* packet_len) {
	char read_char;
	int read_size;
	unsigned char checksum;
	unsigned char read_checksum;
	char read_checksum_hex[3];

	// buf_len == sizeof(buf) so we substract 1 to be sure to be able to fit a \0
	buf_len -= 1;

	// Wating for packet start
	for (;;) {
		read_size = gdb_io_recv(&read_char, 1, READ_WAIT_ALL);
		if (read_size != 1) {
			fxCG50gdb_printf("Packet prelude read failed : %d\n", read_size);
			return -1;
		}
		if (read_char == '$')
			break;
	}

	checksum = 0;
	*packet_len = 0;
	for (;;) {
		read_size = gdb_io_recv(&read_char, 1, READ_WAIT_ALL);
		if (read_size != 1) {
			fxCG50gdb_printf("Packet data read failed : %d\n", read_size);
			return -1;
		}
		if (read_char == '#')
			break;
		if (*packet_len >= buf_len) {
			fxCG50gdb_printf("Packet is too big (size > %d)", buf_len);
			return -1;
		}
		buf[(*packet_len)] = read_char;
		(*packet_len)++;
		checksum += read_char;
	}

	buf[(*packet_len)] = '\0';
#ifdef GDB_SEND_RECV_DEBUG
	fxCG50gdb_printf("recv <- %s\n", buf);
#endif

	read_size = gdb_io_recv((char*)&read_checksum_hex, sizeof(read_checksum_hex) - 1, READ_WAIT_ALL);
	if (read_size != sizeof(read_checksum_hex) - 1) {
		fxCG50gdb_printf("Packet checksum read failed : %d\n", read_size);
		return -1;
	}
	read_checksum_hex[2] = '\0';
	read_checksum = (unsigned char)strtol(read_checksum_hex, NULL, 16);

	if (read_checksum != checksum) {
		fxCG50gdb_printf("Packet checksum is invalid (%hhd != %hhd)\n", read_checksum, checksum);
		read_char = '-';
		gdb_io_send(&read_char, 1);
		return -1;
	}
	read_char = '+';
	gdb_io_send(&read_char, 1);
	return 0;
}

static int gdb_send_packet(char* buf, size_t buflen) {
	char local_buf[4];
	unsigned char checksum = 0;
	bool err = false;

	local_buf[0] = '$';
	err |= gdb_io_send(local_buf, 1) != 1;
	if (buf != NULL && buflen > 0) {
		err |= gdb_io_send(buf, buflen) != (ssize_t)buflen;
		for (size_t i = 0; i < buflen; i++)
			checksum += buf[i];
#ifdef GDB_SEND_RECV_DEBUG
		fxCG50gdb_printf("send -> %s\n", buf);
#endif
	}
#ifdef GDB_SEND_RECV_DEBUG
	else {
		fxCG50gdb_printf("send -> ((empty))\n");
	}
#endif

	snprintf(local_buf, sizeof(local_buf), "#%02X", checksum);
	err |= gdb_io_send(local_buf, 3) != 3;
	return err ? -1 : 0;
}

static int gdb_send_signal(unsigned char signal) {
	char buf[4];
	snprintf(buf, sizeof(buf), "S%02X", signal);
	return gdb_send_packet(buf, 3);
}

static int gdb_handle_q_packet(char* buf) {
	// GDB can have a weird behaviour and send 2 qSupported packets making everything misaligned
	static bool qSupported_done = false;

	if (strncmp("qSupported", buf, 10) == 0) {
		if (!qSupported_done) {
			qSupported_done = true;
			return gdb_send_packet("PacketSize=255", 14);
		}
		return 0;
	}

	return gdb_send_packet(NULL, 0);
}

static int gdb_send_empty_registers(void) {
	char packet[(23 * 8) + 1];
	memset(packet, 'x', sizeof(packet));
	packet[sizeof(packet) - 1] = '\0';

	memcpy(packet + 16 * 8, "A0000000", 8);
	return gdb_send_packet(packet, sizeof(packet) - 1);
}

static int gdb_send_registers(void) {
	struct registers* emur = real_cpu_registers();
	struct gdb_registers gdbr = {
		htoel(emur->r0),   htoel(emur->r1),   htoel(emur->r2),	htoel(emur->r3),  htoel(emur->r4),
		htoel(emur->r5),   htoel(emur->r6),   htoel(emur->r7),	htoel(emur->r8),  htoel(emur->r9),
		htoel(emur->r10),  htoel(emur->r11),  htoel(emur->r12), htoel(emur->r13), htoel(emur->r14),
		htoel(emur->r15),  htoel(emur->pc),   htoel(emur->pr),	htoel(emur->gbr), htoel(emur->vbr),
		htoel(emur->mach), htoel(emur->macl), htoel(emur->sr),
	};

	char buf[(sizeof(gdbr) * 2) + 1];
	static const char hex[] = "0123456789ABCDEF";
	for (size_t i = 0; i < sizeof(gdbr); i++) {
		buf[(i * 2) + 0] = hex[(((char*)&gdbr)[i] & 0xF0) >> 4];
		buf[(i * 2) + 1] = hex[((char*)&gdbr)[i] & 0x0F];
	}
	buf[sizeof(buf) - 1] = '\0';

	return gdb_send_packet(buf, sizeof(buf) - 1);
}

// See sh_sh4_nofpu_register_name in gdb/sh-tdep.c in GDB source code
#define GDB_REGISTER_ID_MAPPING(EMUR, NAME)                                                                      \
	uint32_t* NAME[] = {&EMUR->r0,	 &EMUR->r1,  &EMUR->r2, &EMUR->r3,  &EMUR->r4,	&EMUR->r5,  &EMUR->r6,   \
			    &EMUR->r7,	 &EMUR->r8,  &EMUR->r9, &EMUR->r10, &EMUR->r11, &EMUR->r12, &EMUR->r13,  \
			    &EMUR->r14,	 &EMUR->r15, &EMUR->pc, &EMUR->pr,  &EMUR->gbr, &EMUR->vbr, &EMUR->mach, \
			    &EMUR->macl, &EMUR->sr,  NULL,	NULL,	    NULL,	NULL,	    NULL,        \
			    NULL,	 NULL,	     NULL,	NULL,	    NULL,	NULL,	    NULL,        \
			    NULL,	 NULL,	     NULL,	NULL,	    NULL,	NULL,	    &EMUR->ssr,  \
			    &EMUR->spc}

static int gdb_handle_p_packet(char* buf) {
	buf++;
	unsigned char register_id = (unsigned char)strtoul(buf, NULL, 16);
	struct registers* emur = real_cpu_registers();

	GDB_REGISTER_ID_MAPPING(emur, register_mapping);
	if (register_id >= sizeof(register_mapping) / sizeof(uint32_t*) || register_mapping[register_id] == NULL) {
		return gdb_send_packet("xxxxxxxx", 8);
	}
	uint32_t register_value = htoel(*register_mapping[register_id]);
	char outbuf[(sizeof(register_value) * 2) + 1];
	static const char hex[] = "0123456789ABCDEF";
	for (size_t i = 0; i < sizeof(register_value); i++) {
		outbuf[(i * 2) + 0] = hex[(((char*)&register_value)[i] & 0xF0) >> 4];
		outbuf[(i * 2) + 1] = hex[((char*)&register_value)[i] & 0x0F];
	}
	outbuf[sizeof(outbuf) - 1] = '\0';
	return gdb_send_packet(outbuf, sizeof(outbuf) - 1);
}

static int gdb_handle_G_packet(char* buf) {
	buf++;
	struct gdb_registers gdbr;
	if (strlen(buf) > sizeof(gdbr) * 2) {
		return gdb_send_packet("E01", 3);
	}

	memset(&gdbr, 0, sizeof(gdbr));
	for (size_t i = 0; i < sizeof(gdbr); i++) {
		char byte[3];
		byte[0] = buf[i * 2];
		byte[1] = buf[(i * 2) + 1];
		byte[2] = '\0';
		((char*)&gdbr)[i] = strtoul(byte, NULL, 16) & 0xFF;
	}

	struct registers* emur = real_cpu_registers();
	emur->r0 = etohl(gdbr.r0);
	emur->r1 = etohl(gdbr.r1);
	emur->r2 = etohl(gdbr.r2);
	emur->r3 = etohl(gdbr.r3);
	emur->r4 = etohl(gdbr.r4);
	emur->r5 = etohl(gdbr.r5);
	emur->r6 = etohl(gdbr.r6);
	emur->r7 = etohl(gdbr.r7);
	emur->r8 = etohl(gdbr.r8);
	emur->r9 = etohl(gdbr.r9);
	emur->r10 = etohl(gdbr.r10);
	emur->r11 = etohl(gdbr.r11);
	emur->r12 = etohl(gdbr.r12);
	emur->r13 = etohl(gdbr.r13);
	emur->r14 = etohl(gdbr.r14);
	emur->r15 = etohl(gdbr.r15);
	emur->pc = etohl(gdbr.pc);
	emur->pr = etohl(gdbr.pr);
	emur->gbr = etohl(gdbr.gbr);
	emur->vbr = etohl(gdbr.vbr);
	emur->mach = etohl(gdbr.mach);
	emur->macl = etohl(gdbr.macl);
	emur->sr = etohl(gdbr.sr);

	return gdb_send_packet("OK", 2);
}

static int gdb_handle_P_packet(char* buf) {
	char localbuf[(sizeof(uint32_t) * 2) + 1];

	buf++;
	localbuf[0] = buf[0];
	localbuf[1] = buf[1];
	localbuf[2] = '\0';
	unsigned char register_id = (unsigned char)strtoul(localbuf, NULL, 16);

	// A kind of weird way to check if the register number has one or two digits and increment the pointer
	// accordingly
	buf += localbuf[1] == '=' ? 2 : 3;
	strncpy(localbuf, buf, sizeof(localbuf));
	localbuf[sizeof(localbuf) - 1] = '\0';
	uint32_t register_value = strtoul(localbuf, NULL, 16);

	struct registers* emur = real_cpu_registers();
	GDB_REGISTER_ID_MAPPING(emur, register_mapping);
	if (register_id >= sizeof(register_mapping) / sizeof(uint32_t*) || register_mapping[register_id] == NULL) {
		return gdb_send_packet("E01", 3);
	}
	*register_mapping[register_id] = register_value;
	return gdb_send_packet("OK", 2);
}

static int gdb_handle_m_packet(char* buf) {
	char address_hex[16], size_hex[16];
	uint32_t address;
	size_t size;

	memset(address_hex, 0, sizeof(address_hex));
	memset(size_hex, 0, sizeof(size_hex));

	buf++;
	for (size_t i = 0; i < sizeof(address_hex); i++) {
		address_hex[i] = *buf;
		buf++;
		if (*buf == ',')
			break;
	}
	buf++;
	for (size_t i = 0; i < sizeof(size_hex); i++) {
		size_hex[i] = *buf;
		buf++;
		if (*buf == '\0')
			break;
	}

	address = strtoul(address_hex, NULL, 16);
	size = strtoul(size_hex, NULL, 16);

	// Kinda ugly but 16 should be enough to take into account the alignment of the address and size rounding
	uint8_t* outbuf_back = malloc(size + 16);
	uint8_t* outbuf;
	mmu_read(address, outbuf_back, size, size + 16, &outbuf);

	size_t outbuf_size = size * 2 + 1;
	char* outbuf_hex = malloc(outbuf_size);
	memset(outbuf_hex, 0, outbuf_size);
	static const char hex[] = "0123456789ABCDEF";
	for (size_t i = 0; i < size; i++) {
		uint8_t byte = outbuf[i];
		outbuf_hex[(i * 2) + 0] = hex[(byte & 0xF0) >> 4];
		outbuf_hex[(i * 2) + 1] = hex[byte & 0x0F];
	}
	int err = gdb_send_packet(outbuf_hex, outbuf_size - 1);
	free(outbuf_hex);
	free(outbuf_back);
	return err;
}

static int gdb_handle_M_packet(char* buf) {
	char address_hex[16], size_hex[16];
	uint32_t address;
	size_t size;

	memset(address_hex, 0, sizeof(address_hex));
	memset(size_hex, 0, sizeof(size_hex));

	buf++;
	for (size_t i = 0; i < sizeof(address_hex); i++) {
		address_hex[i] = *buf;
		buf++;
		if (*buf == ',')
			break;
	}
	buf++;
	for (size_t i = 0; i < sizeof(size_hex); i++) {
		size_hex[i] = *buf;
		buf++;
		if (*buf == ':')
			break;
	}
	buf++;

	address = strtoul(address_hex, NULL, 16);
	size = strtoul(size_hex, NULL, 16);
	assert(size > 0);

	assert(strlen(buf) >= size * 2);
	uint8_t* inbuf = malloc(size);

	for (size_t i = 0; i < size; i++) {
		char byte_hex[3];
		byte_hex[0] = buf[(i * 2) + 0];
		byte_hex[1] = buf[(i * 2) + 1];
		byte_hex[2] = '\0';
		inbuf[i] = (uint8_t)strtoul(byte_hex, NULL, 16);
	}

	mmu_write(address, inbuf, size);
	free(inbuf);
	return gdb_send_packet("OK", 2);
}

static int gdb_handle_Zz_packet(char* buf) {
	bool delete = (*buf) == 'z';
	buf++;
	if (*buf != '1')
		return gdb_send_packet(NULL, 0);
	buf += 2;

	char address_hex[16], kind_hex[16];
	uint32_t address;
	unsigned int kind;

	memset(address_hex, 0, sizeof(address_hex));
	memset(kind_hex, 0, sizeof(kind_hex));

	for (size_t i = 0; i < sizeof(address_hex); i++) {
		address_hex[i] = *buf;
		buf++;
		if (*buf == ',')
			break;
	}
	buf++;
	for (size_t i = 0; i < sizeof(kind_hex); i++) {
		kind_hex[i] = *buf;
		buf++;
		if (*buf == ';' || *buf == '\0')
			break;
	}

	address = strtoul(address_hex, NULL, 16);
	kind = strtoul(kind_hex, NULL, 16);
	assert(kind == 2);

	// We do not support different kinds nor conditions

	if (delete)
		gdb_del_breakpoint(address);
	else
		gdb_add_breakpoint(address);
	return gdb_send_packet("OK", 2);
}

#ifdef GDB_NON_STANDARD_FEATURES
static int gdb_handle_non_standard(char* buf) {
	buf++;
	// We only support the "dump_region" command right now
	assert(strncmp(buf, "dump_region", 11) == 0);

	buf += 11;
	uint32_t physical_address = strtoul(buf, NULL, 16);
	fxCG50gdb_printf("gdb_handle_non_standard : Dumping region at 0x%08X\n", physical_address);

	uint32_t* data = (uint32_t*)((uint32_t)mmu_get_region(physical_address)->data & 0xfffffffc);
	uint32_t* outbuf = malloc(MMU_REGION_SIZE);

	memcpy(outbuf, data, MMU_REGION_SIZE);
	for (size_t i = 0; i < MMU_REGION_SIZE / sizeof(uint32_t); i++) {
		outbuf[i] = htoel(outbuf[i]);
	}
	int ret = gdb_io_send((void*)outbuf, MMU_REGION_SIZE);
	free(outbuf);
	return ret == MMU_REGION_SIZE ? 0 : -1;
}
#endif

void gdb_main(bool program_started) {
	char buf[256];
	size_t packet_len = 0;
	if (program_started) {
		assert(WaitForSingleObject(gdb_io_mutex, INFINITE) == WAIT_OBJECT_0);
		assert(gdb_send_signal(GDB_SIGNAL_TRAP) >= 0);
	}
	for (;;) {
		assert(gdb_recv_packet(buf, sizeof(buf), &packet_len) >= 0);
		if (packet_len <= 0)
			continue;

		switch (buf[0]) {
			case '?':
				assert(gdb_send_signal(GDB_SIGNAL_TRAP) >= 0);
				break;

			case 'q':
				assert(gdb_handle_q_packet(buf) >= 0);
				break;

			case 'g':
				if (!program_started)
					assert(gdb_send_empty_registers() >= 0);
				else
					assert(gdb_send_registers() >= 0);
				break;

			case 'm':
				if (!program_started)
					assert(gdb_send_packet(NULL, 0) >= 0);
				else
					assert(gdb_handle_m_packet(buf) >= 0);
				break;

			case 'p':
				if (!program_started)
					assert(gdb_send_packet(NULL, 0) >= 0);
				else
					assert(gdb_handle_p_packet(buf) >= 0);
				break;

			case 'G':
				if (!program_started)
					assert(gdb_send_packet(NULL, 0) >= 0);
				else
					assert(gdb_handle_G_packet(buf) >= 0);
				break;

			case 'P':
				if (!program_started)
					assert(gdb_send_packet(NULL, 0) >= 0);
				else
					assert(gdb_handle_P_packet(buf) >= 0);
				break;

			case 'M':
				if (!program_started)
					assert(gdb_send_packet(NULL, 0) >= 0);
				else
					assert(gdb_handle_M_packet(buf) >= 0);
				break;

			case 'Z':
			case 'z':
				assert(gdb_handle_Zz_packet(buf) >= 0);
				break;

#ifdef GDB_NON_STANDARD_FEATURES
			case '\xFF':
				assert(gdb_handle_non_standard(buf) >= 0);
				break;
#endif
			case 's':
				gdb_wants_step = true;
#if !defined(_MSC_VER)
				__attribute__((fallthrough));
#endif
			case 'c':
				if (program_started)
					assert(ReleaseMutex(gdb_io_mutex));
				gdb_start_SIGINT_thread();
				return;

			default:
				assert(gdb_send_packet(NULL, 0) >= 0);
				break;
		}
	}
}
