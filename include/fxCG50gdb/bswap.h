#ifndef BSWAP_H
#define BSWAP_H

// host to emulator long and short
#define htoel(x) __builtin_bswap32(x)
#define htoes(x) __builtin_bswap16(x)

// emulator to host long and short
#define etohl(x) htoel(x)
#define etohs(x) htoes(x)

#endif
