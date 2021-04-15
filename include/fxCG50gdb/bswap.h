#ifndef BSWAP_H
#define BSWAP_H

// host to emulator long and short
#if defined(_MSC_VER)
#define htoel(x) _byteswap_ulong(x)
#define htoes(x) _byteswap_ushort(x)
#else
#define htoel(x) __builtin_bswap32(x)
#define htoes(x) __builtin_bswap16(x)
#endif

// emulator to host long and short
#define etohl(x) htoel(x)
#define etohs(x) htoes(x)

#endif
