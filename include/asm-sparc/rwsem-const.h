/* rwsem-const.h: RW semaphore counter constants.  */
#ifndef _SPARC64_RWSEM_CONST_H
#define _SPARC64_RWSEM_CONST_H

#define RWSEM_UNLOCKED_VALUE		0x00000000
#define RWSEM_ACTIVE_BIAS		0x00000001
#define RWSEM_ACTIVE_MASK		0x0000ffff
#define RWSEM_WAITING_BIAS		0xffff0000
#define RWSEM_ACTIVE_READ_BIAS		RWSEM_ACTIVE_BIAS
#define RWSEM_ACTIVE_WRITE_BIAS		(RWSEM_WAITING_BIAS + RWSEM_ACTIVE_BIAS)

#endif /* _SPARC64_RWSEM_CONST_H */
