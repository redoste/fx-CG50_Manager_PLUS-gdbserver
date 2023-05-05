#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

#include <shellapi.h>
#include <winsock2.h>

#include <fxCG50gdb/bswap.h>
#include <fxCG50gdb/gdb_io.h>
#include <fxCG50gdb/stdio.h>

typedef int (*gdb_io_accept_handler)(void);
typedef ssize_t (*gdb_io_recv_handler)(char*, size_t, enum gdb_io_read_mode);
typedef ssize_t (*gdb_io_send_handler)(const char*, size_t);

struct gdb_io_interface {
	const wchar_t* name;
	gdb_io_accept_handler accept_handler;
	gdb_io_recv_handler recv_handler;
	gdb_io_send_handler send_handler;
};

static const struct gdb_io_interface* gdb_io_current_interface;
static const wchar_t* gdb_io_current_interface_options;

static SOCKET gdb_io_tcp_socket;
static int gdb_io_tcp_accept(void) {
	int err;
	WSADATA wsa_data;
	SOCKET listen_socket = INVALID_SOCKET;
	uint16_t port = _wtoi(gdb_io_current_interface_options);

	err = WSAStartup(0x0202, &wsa_data);
	if (err != 0) {
		fxCG50gdb_printf("Unable to WSAStartup : %d\n", err);
		WSACleanup();
		return -1;
	}

	listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_socket == (SOCKET)SOCKET_ERROR) {
		fxCG50gdb_printf("Unable to open socket : %d\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}

	struct sockaddr_in listen_address = {0};
	listen_address.sin_family = AF_INET;
	listen_address.sin_port = htoes(port);
	err = bind(listen_socket, (struct sockaddr*)&listen_address, sizeof(listen_address));
	if (err == SOCKET_ERROR) {
		fxCG50gdb_printf("Unable to bind socket : %d\n", WSAGetLastError());
		closesocket(listen_socket);
		WSACleanup();
		return -1;
	}

	err = listen(listen_socket, SOMAXCONN);
	if (err == SOCKET_ERROR) {
		fxCG50gdb_printf("Unable to listen socket : %d\n", WSAGetLastError());
		closesocket(listen_socket);
		WSACleanup();
		return -1;
	}

	fxCG50gdb_printf("Listening on port %hu : wating for GDB...\n", port);

	gdb_io_tcp_socket = accept(listen_socket, NULL, NULL);
	if (gdb_io_tcp_socket == (SOCKET)SOCKET_ERROR) {
		fxCG50gdb_printf("Unable to accept socket : %d\n", WSAGetLastError());
		closesocket(listen_socket);
		WSACleanup();
		return -1;
	}
	closesocket(listen_socket);
	return 0;
}

static ssize_t gdb_io_tcp_recv(char* buffer, size_t buffer_size, enum gdb_io_read_mode read_mode) {
	assert(read_mode == READ_WAIT_ALL || read_mode == READ_PEEK);
	int flags = read_mode == READ_WAIT_ALL ? MSG_WAITALL : MSG_PEEK;
	return recv(gdb_io_tcp_socket, buffer, buffer_size, flags);
}

static ssize_t gdb_io_tcp_send(const char* buffer, size_t buffer_size) {
	return send(gdb_io_tcp_socket, buffer, buffer_size, 0);
}

static const struct gdb_io_interface gdb_io_interfaces[] = {
	{L"tcp", gdb_io_tcp_accept, gdb_io_tcp_recv, gdb_io_tcp_send},
};

void gdb_io_init(void) {
	ssize_t argc;
	wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	assert(argv != NULL);

	const wchar_t* arg_prefix = L"/gdb:";
	const size_t arg_prefix_len = wcslen(arg_prefix);
	wchar_t* gdb_arg = NULL;
	for (ssize_t i = 0; i < argc; i++) {
		if (wcsncmp(argv[i], arg_prefix, arg_prefix_len) == 0) {
			gdb_arg = argv[i];
			break;
		}
	}
	if (gdb_arg != NULL) {
		for (size_t i = 0; i < sizeof(gdb_io_interfaces) / sizeof(gdb_io_interfaces[0]); i++) {
			const wchar_t* iface_name = gdb_io_interfaces[i].name;
			size_t iface_name_len = wcslen(iface_name);
			if (wcsncmp(gdb_arg + arg_prefix_len, iface_name, iface_name_len) == 0 &&
			    gdb_arg[arg_prefix_len + iface_name_len] == L':') {
				const wchar_t* options = gdb_arg + arg_prefix_len + iface_name_len + 1;
				fxCG50gdb_printf("gdb_io_init : selected interface %ls with options \"%ls\"\n",
						 iface_name, options);
				gdb_io_current_interface = &gdb_io_interfaces[i];
				gdb_io_current_interface_options = options;
				break;
			}
		}
	}

	if (gdb_io_current_interface == NULL) {
		fxCG50gdb_printf(
			"Use `/gdb:[interface name]:[interface options]` to set communication interface with GDB\n");
		fxCG50gdb_printf("Avaliable interfaces :\n");
		for (size_t i = 0; i < sizeof(gdb_io_interfaces) / sizeof(gdb_io_interfaces[0]); i++) {
			fxCG50gdb_printf(" - %ls\n", gdb_io_interfaces[i].name);
		}
		fxCG50gdb_printf("Defaulting to `/gdb:" GDB_IO_DEFAULT_INTERFACE ":%ls`\n",
				 GDB_IO_DEFAULT_INTERFACE_OPTION);
		gdb_io_current_interface = &gdb_io_interfaces[GDB_IO_DEFAULT_INTERFACE_INDEX];
		gdb_io_current_interface_options = GDB_IO_DEFAULT_INTERFACE_OPTION;
	}
}

int gdb_io_accept(void) {
	assert(gdb_io_current_interface != NULL);
	return gdb_io_current_interface->accept_handler();
}

ssize_t gdb_io_recv(char* buffer, size_t buffer_size, enum gdb_io_read_mode read_mode) {
	assert(gdb_io_current_interface != NULL);
	return gdb_io_current_interface->recv_handler(buffer, buffer_size, read_mode);
}

ssize_t gdb_io_send(const char* buffer, size_t buffer_size) {
	assert(gdb_io_current_interface != NULL);
	return gdb_io_current_interface->send_handler(buffer, buffer_size);
}
