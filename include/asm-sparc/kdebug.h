#ifndef ___ASM_SPARC_KDEBUG_H
#define ___ASM_SPARC_KDEBUG_H
#if defined(__sparc__) && defined(__arch64__)
#include <asm-sparc/kdebug_64.h>
#else
#include <asm-sparc/kdebug_32.h>
#endif
#endif
