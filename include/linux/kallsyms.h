/* Rewritten and vastly simplified by Rusty Russell for in-kernel
 * module loader:
 *   Copyright 2002 Rusty Russell <rusty@rustcorp.com.au> IBM Corporation
 */
#ifndef _LINUX_KALLSYMS_H
#define _LINUX_KALLSYMS_H


#define KSYM_NAME_LEN 127
#define KSYM_SYMBOL_LEN (sizeof("%s+%#lx/%#lx [%s]") + KSYM_NAME_LEN +	\
			 2*(BITS_PER_LONG*3/10) + MODULE_NAME_LEN + 1)

#ifdef CONFIG_KALLSYMS
/* Lookup the address for a symbol. Returns 0 if not found. */
unsigned long kallsyms_lookup_name(const char *name);

extern int kallsyms_lookup_size_offset(unsigned long addr,
				  unsigned long *symbolsize,
				  unsigned long *offset);

/* Lookup an address.  modname is set to NULL if it's in the kernel. */
const char *kallsyms_lookup(unsigned long addr,
			    unsigned long *symbolsize,
			    unsigned long *offset,
			    char **modname, char *namebuf);

/* Look up a kernel symbol and return it in a text buffer. */
extern int sprint_symbol(char *buffer, unsigned long address);

/* Look up a kernel symbol and print it to the kernel messages. */
extern void __print_symbol(const char *fmt, unsigned long address);

#else /* !CONFIG_KALLSYMS */

static inline unsigned long kallsyms_lookup_name(const char *name)
{
	return 0;
}

static inline int kallsyms_lookup_size_offset(unsigned long addr,
					      unsigned long *symbolsize,
					      unsigned long *offset)
{
	return 0;
}

static inline const char *kallsyms_lookup(unsigned long addr,
					  unsigned long *symbolsize,
					  unsigned long *offset,
					  char **modname, char *namebuf)
{
	return NULL;
}

static inline int sprint_symbol(char *buffer, unsigned long addr)
{
	*buffer = '\0';
	return 0;
}

/* Stupid that this does nothing, but I didn't create this mess. */
#define __print_symbol(fmt, addr)
#endif /*CONFIG_KALLSYMS*/

/* This macro allows us to keep printk typechecking */
static void __check_printsym_format(const char *fmt, ...)
__attribute__((format(printf,1,2)));
static inline void __check_printsym_format(const char *fmt, ...)
{
}
/* ia64 and ppc64 use function descriptors, which contain the real address */
#if defined(CONFIG_IA64) || defined(CONFIG_PPC64)
#define print_fn_descriptor_symbol(fmt, addr)		\
do {						\
	unsigned long *__faddr = (unsigned long*) addr;		\
	print_symbol(fmt, __faddr[0]);		\
} while (0)
#else
#define print_fn_descriptor_symbol(fmt, addr) print_symbol(fmt, addr)
#endif

static inline void print_symbol(const char *fmt, unsigned long addr)
{
	__check_printsym_format(fmt, "");
	__print_symbol(fmt, (unsigned long)
		       __builtin_extract_return_addr((void *)addr));
}

#ifndef CONFIG_64BIT
#define print_ip_sym(ip)		\
do {					\
	printk("[<%08lx>]", ip);	\
	print_symbol(" %s\n", ip);	\
} while(0)
#else
#define print_ip_sym(ip)		\
do {					\
	printk("[<%016lx>]", ip);	\
	print_symbol(" %s\n", ip);	\
} while(0)
#endif

#endif /*_LINUX_KALLSYMS_H*/
