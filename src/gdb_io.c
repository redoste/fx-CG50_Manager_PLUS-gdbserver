#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

#include <shellapi.h>
#include <winsock2.h>

#include <afunix.h>

#include <fxCG50gdb/bswap.h>
#include <fxCG50gdb/gdb_io.h>
#include <fxCG50gdb/stdio.h>
#include <fxCG50gdb/wine.h>

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

static HANDLE gdb_io_peek_buffer_mutex;
char gdb_io_peek_buffer[16];
size_t gdb_io_peek_buffer_size = 0;

static HANDLE gdb_io_com_handle;
static int gdb_io_com_accept(void) {
	wchar_t filename[16];
	_snwprintf(filename, sizeof(filename) / sizeof(filename[0]), L"\\\\.\\%ls", gdb_io_current_interface_options);
	/*
	 * NOTE : We need to use Overlapped I/O to be able to read and write at the same time from the same handle
	 *        (ref : https://learn.microsoft.com/en-us/previous-versions/ff802693(v=msdn.10)#overlapped-io)
	 */
	gdb_io_com_handle =
		CreateFileW(filename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

	if (gdb_io_com_handle == INVALID_HANDLE_VALUE) {
		fxCG50gdb_printf("Unable to open \"%ls\" : %lu\n", filename, GetLastError());
		return -1;
	}

	fxCG50gdb_printf("Handle to \"%ls\" opened\n", filename);

	gdb_io_peek_buffer_mutex = CreateMutexA(NULL, FALSE, NULL);
	assert(gdb_io_peek_buffer_mutex != NULL);
	return 0;
}

static ssize_t gdb_io_com_internal_read(char* buffer, size_t buffer_size) {
	HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (event == NULL) {
		return -1;
	}

	OVERLAPPED overlapped;
	overlapped.Offset = 0;
	overlapped.OffsetHigh = 0;
	overlapped.hEvent = event;

	DWORD read_bytes;
	ReadFile(gdb_io_com_handle, buffer, buffer_size, &read_bytes, &overlapped);

	assert(WaitForSingleObject(event, INFINITE) == WAIT_OBJECT_0);
	bool ret = GetOverlappedResult(gdb_io_com_handle, &overlapped, &read_bytes, TRUE);
	CloseHandle(event);

	return ret ? (ssize_t)read_bytes : -1;
}

// TODO : Implement better READ_PEEK support : currently it assumes only small and non consecutive peeks
static ssize_t gdb_io_com_recv(char* buffer, size_t buffer_size, enum gdb_io_read_mode read_mode) {
	assert(WaitForSingleObject(gdb_io_peek_buffer_mutex, INFINITE) == WAIT_OBJECT_0);
	assert(read_mode == READ_WAIT_ALL || read_mode == READ_PEEK);

	if (gdb_io_peek_buffer_size > 0) {
		assert(buffer_size <= gdb_io_peek_buffer_size);
		fxCG50gdb_printf("com peek buffer read : %zu : 0x%02hhx\n", gdb_io_peek_buffer_size,
				 gdb_io_peek_buffer[0]);
		memcpy(buffer, gdb_io_peek_buffer, buffer_size);
		if (read_mode == READ_WAIT_ALL) {
			gdb_io_peek_buffer_size = 0;
			fxCG50gdb_printf("com peek buffer drained : %zu\n", gdb_io_peek_buffer_size);
		}
		assert(ReleaseMutex(gdb_io_peek_buffer_mutex));
		return buffer_size;
	}

	size_t total_read = 0;
	while (total_read < buffer_size) {
		ssize_t ret = gdb_io_com_internal_read(buffer + total_read, buffer_size - total_read);
		if (ret > 0) {
			total_read += ret;
		} else {
			assert(ReleaseMutex(gdb_io_peek_buffer_mutex));
			return -1;
		}
	}

	if (read_mode == READ_PEEK) {
		assert(gdb_io_peek_buffer_size == 0 && buffer_size < sizeof(gdb_io_peek_buffer));
		memcpy(gdb_io_peek_buffer, buffer, buffer_size);
		gdb_io_peek_buffer_size = buffer_size;
		fxCG50gdb_printf("com peek buffer filled : %zu : 0x%02hhx\n", gdb_io_peek_buffer_size,
				 gdb_io_peek_buffer[0]);
	}
	assert(ReleaseMutex(gdb_io_peek_buffer_mutex));
	return total_read;
}

static ssize_t gdb_io_com_send(const char* buffer, size_t buffer_size) {
	HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (event == NULL) {
		return -1;
	}

	OVERLAPPED overlapped;
	overlapped.Offset = 0;
	overlapped.OffsetHigh = 0;
	overlapped.hEvent = event;

	DWORD written_bytes;
	WriteFile(gdb_io_com_handle, buffer, buffer_size, &written_bytes, &overlapped);

	assert(WaitForSingleObject(event, INFINITE) == WAIT_OBJECT_0);
	bool ret = GetOverlappedResult(gdb_io_com_handle, &overlapped, &written_bytes, TRUE);
	CloseHandle(event);
	return ret ? (ssize_t)written_bytes : -1;
}

static int gdb_io_wine_unix_socket;
static int gdb_io_wine_accept(void) {
	int err;
	int listen_socket;

	fxCG50gdb_printf(
		"fxCG50gdb is now about to call `int 0x80` : This will crash on real Windows (i.e. not Wine on "
		"*/Linux)\n");
	listen_socket = wine_socket(AF_UNIX, SOCK_STREAM, 0);
	if (listen_socket < 0) {
		fxCG50gdb_printf("Unable to open socket : %d\n", listen_socket);
		return -1;
	}

	struct sockaddr_un listen_address = {
		.sun_family = AF_UNIX,
		.sun_path = {0},
	};
	err = WideCharToMultiByte(CP_UTF8, 0, gdb_io_current_interface_options, -1, listen_address.sun_path,
				  sizeof(listen_address.sun_path), NULL, NULL);
	if (err <= 0) {
		fxCG50gdb_printf("Unable to convert to UTF-8 socket path \"%ls\" : %lu\n",
				 gdb_io_current_interface_options, GetLastError());
	}

	err = wine_bind(listen_socket, (struct sockaddr*)&listen_address, sizeof(listen_address));
	if (err < 0) {
		fxCG50gdb_printf("Unable to bind socket : %d\n", err);
		wine_close(listen_socket);
		return -1;
	}

	err = wine_listen(listen_socket, SOMAXCONN);
	if (err < 0) {
		fxCG50gdb_printf("Unable to listen socket : %d\n", err);
		wine_close(listen_socket);
		return -1;
	}

	fxCG50gdb_printf("Listening on unix socket \"%s\" : wating for GDB...\n", listen_address.sun_path);

	gdb_io_wine_unix_socket = wine_accept4(listen_socket, NULL, NULL, 0);
	if (gdb_io_wine_unix_socket < 0) {
		fxCG50gdb_printf("Unable to accept socket : %d\n", gdb_io_wine_unix_socket);
		wine_close(listen_socket);
		return -1;
	}
	wine_close(listen_socket);
	return 0;
}

static ssize_t gdb_io_wine_recv(char* buffer, size_t buffer_size, enum gdb_io_read_mode read_mode) {
	assert(read_mode == READ_WAIT_ALL || read_mode == READ_PEEK);
	int flags = read_mode == READ_WAIT_ALL ? WINE_MSG_WAITALL : WINE_MSG_PEEK;
	return wine_recvfrom(gdb_io_wine_unix_socket, buffer, buffer_size, flags, NULL, 0);
}

static ssize_t gdb_io_wine_send(const char* buffer, size_t buffer_size) {
	return wine_sendto(gdb_io_wine_unix_socket, buffer, buffer_size, 0, NULL, 0);
}

static const struct gdb_io_interface gdb_io_interfaces[] = {
	{L"tcp", gdb_io_tcp_accept, gdb_io_tcp_recv, gdb_io_tcp_send},
	{L"com", gdb_io_com_accept, gdb_io_com_recv, gdb_io_com_send},
	{L"wine", gdb_io_wine_accept, gdb_io_wine_recv, gdb_io_wine_send},
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
