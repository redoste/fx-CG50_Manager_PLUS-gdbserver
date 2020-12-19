#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winsock2.h>

#include <fxCG50gdb/emulator.h>
#include <fxCG50gdb/gdb.h>
#include <fxCG50gdb/mmu.h>
#include <fxCG50gdb/stdio.h>

SOCKET gdb_client_socket = INVALID_SOCKET;
static HANDLE gdb_client_socket_mutex;
bool gdb_wants_step = false;

static int gdb_init_socket() {
	int err;
	WSADATA wsa_data;
	SOCKET listen_socket = INVALID_SOCKET;

	err = WSAStartup(0x0202, &wsa_data);
	if (err != 0) {
		fxCG50gdb_printf("Unable to WSAStartup : %d\n", err);
		return -1;
	}

	listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_socket == (SOCKET)SOCKET_ERROR) {
		fxCG50gdb_printf("Unable to open socket : %d\n", WSAGetLastError());
		return -1;
	}

	struct sockaddr_in listen_address = {0};
	listen_address.sin_family = AF_INET;
	listen_address.sin_port = htons(GDB_SERVER_PORT);
	err = bind(listen_socket, (struct sockaddr*)&listen_address, sizeof(listen_address));
	if (err == SOCKET_ERROR) {
		fxCG50gdb_printf("Unable to bind socket : %d\n", WSAGetLastError());
		closesocket(listen_socket);
		return -1;
	}

	err = listen(listen_socket, SOMAXCONN);
	if (err == SOCKET_ERROR) {
		fxCG50gdb_printf("Unable to listen socket : %d\n", WSAGetLastError());
		closesocket(listen_socket);
		return -1;
	}

	fxCG50gdb_printf("Listening on port %d : wating for GDB...\n", GDB_SERVER_PORT);

	gdb_client_socket = accept(listen_socket, NULL, NULL);
	if (gdb_client_socket == (SOCKET)SOCKET_ERROR) {
		fxCG50gdb_printf("Unable to accept socket : %d\n", WSAGetLastError());
		closesocket(listen_socket);
		return -1;
	}
	closesocket(listen_socket);
	return 0;
}

static unsigned long __stdcall gdb_start_thread(void* lpParameter) {
	(void)lpParameter;
	assert(WaitForSingleObject(gdb_client_socket_mutex, INFINITE) == WAIT_OBJECT_0);

	if (gdb_init_socket() < 0) {
		WSACleanup();
		ExitProcess(-1);
		return 0;
	}

	gdb_main(false);
	assert(ReleaseMutex(gdb_client_socket_mutex));
	return 0;
}

void gdb_start() {
	gdb_client_socket_mutex = CreateMutexA(NULL, FALSE, NULL);
	CreateThread(NULL, 0, &gdb_start_thread, NULL, 0, NULL);
}

uint8_t* gdb_breakpoints[128] = {NULL};
static void gdb_add_breakpoint(uint32_t address) {
	fxCG50gdb_printf("gdb_add_breakpoint : New breakpoint at 0x%08X (bp@0x%08X)\n", address, gdb_breakpoints);
	address = address >> 1;
	uint8_t prefix = address >> 24;
	uint32_t suffix = address & 0xFFFFFF;
	if (gdb_breakpoints[prefix] == NULL) {
		// We use about 2MiB per region, we should probably found an other fast way to store/use breakpoints but
		// less memory intensive
		gdb_breakpoints[prefix] = malloc(0x1000000 >> 3);
		memset(gdb_breakpoints[prefix], 0, 0x1000000 >> 3);
	}
	gdb_breakpoints[prefix][suffix >> 3] |= 1 << (suffix & 7);
	fxCG50gdb_printf("gdb_add_breakpoint : address=0x%08X p=0x%02X s=0x%06X bp@0x%08X bp=0x%02X\n", address, prefix,
			 suffix, &gdb_breakpoints[prefix][suffix >> 3], gdb_breakpoints[prefix][suffix >> 3]);
}

static void gdb_del_breakpoint(uint32_t address) {
	fxCG50gdb_printf("gdb_del_breakpoint : Remove breakpoint at 0x%08X (bp@0x%08X)\n", address, gdb_breakpoints);
	address = address >> 1;
	uint8_t prefix = address >> 24;
	uint32_t suffix = address & 0xFFFFFF;
	if (gdb_breakpoints[prefix] != NULL) {
		gdb_breakpoints[prefix][suffix >> 3] &= ~(1 << (suffix & 7));
		fxCG50gdb_printf("gdb_del_breakpoint : address=0x%08X p=0x%02X s=0x%06X bp@0x%08X bp=0x%02X\n", address,
				 prefix, suffix, &gdb_breakpoints[prefix][suffix >> 3],
				 gdb_breakpoints[prefix][suffix >> 3]);
	}
	// Currently we don't garbage collect gdb_breakpoints, if the user puts and delete a lot of breakpoints, this
	// might be heavy on memory usage
}

int gdb_recv_packet(char* buf, size_t buf_len, size_t* packet_len) {
	char read_char;
	int read_size;
	unsigned char checksum;
	unsigned char read_checksum;
	char read_checksum_hex[3];

	// buf_len == sizeof(buf) so we substract 1 to be sure to be able to fit a \0
	buf_len -= 1;

	// Wating for packet start
	for (;;) {
		read_size = recv(gdb_client_socket, &read_char, 1, MSG_WAITALL);
		if (read_size != 1)
			return -1;
		if (read_char == '$')
			break;
	}

	checksum = 0;
	*packet_len = 0;
	for (;;) {
		read_size = recv(gdb_client_socket, &read_char, 1, MSG_WAITALL);
		if (read_size != 1)
			return -1;
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

	read_size = recv(gdb_client_socket, (char*)&read_checksum_hex, sizeof(read_checksum_hex) - 1, MSG_WAITALL);
	if (read_size != sizeof(read_checksum_hex) - 1)
		return -1;
	read_checksum_hex[2] = '\0';
	read_checksum = strtol(read_checksum_hex, NULL, 16);

	if (read_checksum != checksum) {
		fxCG50gdb_printf("Packet checksum is invalid (%hhd != %hhd)\n", read_checksum, checksum);
		read_char = '-';
		send(gdb_client_socket, &read_char, 1, 0);
		return -1;
	}
	read_char = '+';
	send(gdb_client_socket, &read_char, 1, 0);
	return 0;
}

int gdb_send_packet(char* buf, size_t buflen) {
	char local_buf[4];
	unsigned char checksum = 0;
	bool err = false;

	local_buf[0] = '$';
	err |= send(gdb_client_socket, local_buf, 1, 0) != 1;
	if (buf != NULL && buflen > 0) {
		err |= send(gdb_client_socket, buf, buflen, 0) != (ssize_t)buflen;
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
	err |= send(gdb_client_socket, local_buf, 3, 0) != 3;
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

static int gdb_send_empty_registers() {
	char packet[(23 * 8) + 1];
	memset(packet, 'x', sizeof(packet));
	packet[sizeof(packet) - 1] = '\0';

	memcpy(packet + 16 * 8, "A0000000", 8);
	return gdb_send_packet(packet, sizeof(packet) - 1);
}

static int gdb_send_registers() {
	struct registers* emur = real_cpu_registers();
	struct gdb_registers gdbr = {
		htonl(emur->r0),   htonl(emur->r1),   htonl(emur->r2),	htonl(emur->r3),  htonl(emur->r4),
		htonl(emur->r5),   htonl(emur->r6),   htonl(emur->r7),	htonl(emur->r8),  htonl(emur->r9),
		htonl(emur->r10),  htonl(emur->r11),  htonl(emur->r12), htonl(emur->r13), htonl(emur->r14),
		htonl(emur->r15),  htonl(emur->pc),   htonl(emur->pr),	htonl(emur->gbr), htonl(emur->vbr),
		htonl(emur->mach), htonl(emur->macl), htonl(emur->sr),
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
	unsigned char register_id = strtoul(buf, NULL, 16);
	struct registers* emur = real_cpu_registers();

	GDB_REGISTER_ID_MAPPING(emur, register_mapping);
	if (register_id >= sizeof(register_mapping) / sizeof(uint32_t*) || register_mapping[register_id] == NULL) {
		return gdb_send_packet("xxxxxxxx", 8);
	}
	uint32_t register_value = htonl(*register_mapping[register_id]);
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
	emur->r0 = ntohl(gdbr.r0);
	emur->r1 = ntohl(gdbr.r1);
	emur->r2 = ntohl(gdbr.r2);
	emur->r3 = ntohl(gdbr.r3);
	emur->r4 = ntohl(gdbr.r4);
	emur->r5 = ntohl(gdbr.r5);
	emur->r6 = ntohl(gdbr.r6);
	emur->r7 = ntohl(gdbr.r7);
	emur->r8 = ntohl(gdbr.r8);
	emur->r9 = ntohl(gdbr.r9);
	emur->r10 = ntohl(gdbr.r10);
	emur->r11 = ntohl(gdbr.r11);
	emur->r12 = ntohl(gdbr.r12);
	emur->r13 = ntohl(gdbr.r13);
	emur->r14 = ntohl(gdbr.r14);
	emur->r15 = ntohl(gdbr.r15);
	// TODO : Implement "set $pc" correctly
	// emur->pc = gdbr.pc;
	emur->pr = ntohl(gdbr.pr);
	emur->gbr = ntohl(gdbr.gbr);
	emur->vbr = ntohl(gdbr.vbr);
	emur->mach = ntohl(gdbr.mach);
	emur->macl = ntohl(gdbr.macl);
	emur->sr = ntohl(gdbr.sr);

	return gdb_send_packet("OK", 2);
}

static int gdb_handle_P_packet(char* buf) {
	char localbuf[(sizeof(uint32_t) * 2) + 1];

	buf++;
	localbuf[0] = buf[0];
	localbuf[1] = buf[1];
	localbuf[2] = '\0';
	unsigned char register_id = strtoul(localbuf, NULL, 16);

	// A kind of weird way to check if the register number has one or two digits and increment the pointer
	// accordingly
	buf += localbuf[1] == '=' ? 2 : 3;
	strncpy(localbuf, buf, sizeof(localbuf));
	localbuf[sizeof(localbuf) - 1] = '\0';
	uint32_t register_value = strtoul(localbuf, NULL, 16);

	struct registers* emur = real_cpu_registers();
	GDB_REGISTER_ID_MAPPING(emur, register_mapping);
	// TODO : Implement "set $pc" correctly
	if (register_id >= sizeof(register_mapping) / sizeof(uint32_t*) || register_mapping[register_id] == NULL ||
	    register_mapping[register_id] == &emur->pc) {
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

	uint8_t* outbuf = malloc(size);
	mmu_read(address, outbuf, size);

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
	free(outbuf);
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
		inbuf[i] = strtoul(byte_hex, NULL, 16);
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

void gdb_main(bool program_started) {
	char buf[256];
	size_t packet_len;
	if (program_started) {
		assert(WaitForSingleObject(gdb_client_socket_mutex, INFINITE) == WAIT_OBJECT_0);
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

			case 's':
				gdb_wants_step = true;
				__attribute__((fallthrough));
			case 'c':
				if (program_started)
					assert(ReleaseMutex(gdb_client_socket_mutex));
				return;

			default:
				assert(gdb_send_packet(NULL, 0) >= 0);
				break;
		}
	}
}
