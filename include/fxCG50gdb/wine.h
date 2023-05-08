#ifndef WINE_H
#define WINE_H

#include <winsock2.h>

#include <fxCG50gdb/stdio.h>

#define WINE_MSG_WAITALL 0x100
#define WINE_MSG_PEEK 0x02

int __cdecl wine_socket(int domain, int type, int protocol);
int __cdecl wine_bind(int sockfd, const struct sockaddr* addr, size_t addrlen);
int __cdecl wine_listen(int sockfd, int backlog);
int __cdecl wine_accept4(int sockfd, struct sockaddr* addr, size_t* addrlen, int flags);
int __cdecl wine_close(int fd);

ssize_t __cdecl wine_recvfrom(int sockfd,
			      void* buf,
			      size_t len,
			      int flags,
			      struct sockaddr* src_addr,
			      size_t* restrict addrlen);
ssize_t __cdecl wine_sendto(int sockfd,
			    const void* buf,
			    size_t len,
			    int flags,
			    const struct sockaddr* dest_addr,
			    size_t addrlen);

#endif
