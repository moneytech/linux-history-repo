#include <linux/suspend.h>
#include <linux/utsname.h>

struct swsusp_info {
	struct new_utsname	uts;
	u32			version_code;
	unsigned long		num_physpages;
	int			cpus;
	unsigned long		image_pages;
	unsigned long		pages;
} __attribute__((aligned(PAGE_SIZE)));



#ifdef CONFIG_SOFTWARE_SUSPEND
extern int pm_suspend_disk(void);

#else
static inline int pm_suspend_disk(void)
{
	return -EPERM;
}
#endif
extern struct semaphore pm_sem;
#define power_attr(_name) \
static struct subsys_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

extern struct subsystem power_subsys;

/* References to section boundaries */
extern const void __nosave_begin, __nosave_end;

extern struct pbe *pagedir_nosave;

/* Preferred image size in bytes (default 500 MB) */
extern unsigned long image_size;

extern int in_suspend;

extern asmlinkage int swsusp_arch_suspend(void);
extern asmlinkage int swsusp_arch_resume(void);

extern unsigned int count_data_pages(void);
extern void swsusp_free(void);

struct snapshot_handle {
	loff_t		offset;
	unsigned int	page;
	unsigned int	page_offset;
	unsigned int	prev;
	struct pbe	*pbe;
	void		*buffer;
	unsigned int	buf_offset;
};

#define data_of(handle)	((handle).buffer + (handle).buf_offset)

extern int snapshot_read_next(struct snapshot_handle *handle, size_t count);
extern int snapshot_write_next(struct snapshot_handle *handle, size_t count);
int snapshot_image_loaded(struct snapshot_handle *handle);
