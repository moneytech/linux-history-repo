#ifndef __SOUND_CORE_H
#define __SOUND_CORE_H

/*
 *  Main header file for the ALSA driver
 *  Copyright (c) 1994-2001 by Jaroslav Kysela <perex@suse.cz>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/sched.h>		/* wake_up() */
#include <linux/mutex.h>		/* struct mutex */
#include <linux/rwsem.h>		/* struct rw_semaphore */
#include <linux/workqueue.h>		/* struct workqueue_struct */
#include <linux/pm.h>			/* pm_message_t */

/* forward declarations */
#ifdef CONFIG_PCI
struct pci_dev;
#endif
#ifdef CONFIG_SBUS
struct sbus_dev;
#endif

/* device allocation stuff */

#define SNDRV_DEV_TYPE_RANGE_SIZE		0x1000

typedef int __bitwise snd_device_type_t;
#define	SNDRV_DEV_TOPLEVEL	((__force snd_device_type_t) 0)
#define	SNDRV_DEV_CONTROL	((__force snd_device_type_t) 1)
#define	SNDRV_DEV_LOWLEVEL_PRE	((__force snd_device_type_t) 2)
#define	SNDRV_DEV_LOWLEVEL_NORMAL ((__force snd_device_type_t) 0x1000)
#define	SNDRV_DEV_PCM		((__force snd_device_type_t) 0x1001)
#define	SNDRV_DEV_RAWMIDI	((__force snd_device_type_t) 0x1002)
#define	SNDRV_DEV_TIMER		((__force snd_device_type_t) 0x1003)
#define	SNDRV_DEV_SEQUENCER	((__force snd_device_type_t) 0x1004)
#define	SNDRV_DEV_HWDEP		((__force snd_device_type_t) 0x1005)
#define	SNDRV_DEV_INFO		((__force snd_device_type_t) 0x1006)
#define	SNDRV_DEV_BUS		((__force snd_device_type_t) 0x1007)
#define	SNDRV_DEV_CODEC		((__force snd_device_type_t) 0x1008)
#define	SNDRV_DEV_LOWLEVEL	((__force snd_device_type_t) 0x2000)

typedef int __bitwise snd_device_state_t;
#define	SNDRV_DEV_BUILD		((__force snd_device_state_t) 0)
#define	SNDRV_DEV_REGISTERED	((__force snd_device_state_t) 1)
#define	SNDRV_DEV_DISCONNECTED	((__force snd_device_state_t) 2)

typedef int __bitwise snd_device_cmd_t;
#define	SNDRV_DEV_CMD_PRE	((__force snd_device_cmd_t) 0)
#define	SNDRV_DEV_CMD_NORMAL	((__force snd_device_cmd_t) 1)	
#define	SNDRV_DEV_CMD_POST	((__force snd_device_cmd_t) 2)

struct snd_device;

struct snd_device_ops {
	int (*dev_free)(struct snd_device *dev);
	int (*dev_register)(struct snd_device *dev);
	int (*dev_disconnect)(struct snd_device *dev);
	int (*dev_unregister)(struct snd_device *dev);
};

struct snd_device {
	struct list_head list;		/* list of registered devices */
	struct snd_card *card;		/* card which holds this device */
	snd_device_state_t state;	/* state of the device */
	snd_device_type_t type;		/* device type */
	void *device_data;		/* device structure */
	struct snd_device_ops *ops;	/* operations */
};

#define snd_device(n) list_entry(n, struct snd_device, list)

/* monitor files for graceful shutdown (hotplug) */

struct snd_monitor_file {
	struct file *file;
	struct snd_monitor_file *next;
};

struct snd_shutdown_f_ops;	/* define it later in init.c */

/* main structure for soundcard */

struct snd_card {
	int number;			/* number of soundcard (index to
								snd_cards) */

	char id[16];			/* id string of this card */
	char driver[16];		/* driver name */
	char shortname[32];		/* short name of this soundcard */
	char longname[80];		/* name of this soundcard */
	char mixername[80];		/* mixer name */
	char components[80];		/* card components delimited with
								space */
	struct module *module;		/* top-level module */

	void *private_data;		/* private data for soundcard */
	void (*private_free) (struct snd_card *card); /* callback for freeing of
								private data */
	struct list_head devices;	/* devices */

	unsigned int last_numid;	/* last used numeric ID */
	struct rw_semaphore controls_rwsem;	/* controls list lock */
	rwlock_t ctl_files_rwlock;	/* ctl_files list lock */
	int controls_count;		/* count of all controls */
	int user_ctl_count;		/* count of all user controls */
	struct list_head controls;	/* all controls for this card */
	struct list_head ctl_files;	/* active control files */

	struct snd_info_entry *proc_root;	/* root for soundcard specific files */
	struct snd_info_entry *proc_id;	/* the card id */
	struct proc_dir_entry *proc_root_link;	/* number link to real id */

	struct snd_monitor_file *files; /* all files associated to this card */
	struct snd_shutdown_f_ops *s_f_ops; /* file operations in the shutdown
								state */
	spinlock_t files_lock;		/* lock the files for this card */
	int shutdown;			/* this card is going down */
	wait_queue_head_t shutdown_sleep;
	struct work_struct free_workq;	/* for free in workqueue */
	struct device *dev;

#ifdef CONFIG_PM
	unsigned int power_state;	/* power state */
	struct mutex power_lock;	/* power lock */
	wait_queue_head_t power_sleep;
#endif

#if defined(CONFIG_SND_MIXER_OSS) || defined(CONFIG_SND_MIXER_OSS_MODULE)
	struct snd_mixer_oss *mixer_oss;
	int mixer_oss_change_count;
#endif
};

#ifdef CONFIG_PM
static inline void snd_power_lock(struct snd_card *card)
{
	mutex_lock(&card->power_lock);
}

static inline void snd_power_unlock(struct snd_card *card)
{
	mutex_unlock(&card->power_lock);
}

static inline unsigned int snd_power_get_state(struct snd_card *card)
{
	return card->power_state;
}

static inline void snd_power_change_state(struct snd_card *card, unsigned int state)
{
	card->power_state = state;
	wake_up(&card->power_sleep);
}

/* init.c */
int snd_power_wait(struct snd_card *card, unsigned int power_state);

#else /* ! CONFIG_PM */

#define snd_power_lock(card)		do { (void)(card); } while (0)
#define snd_power_unlock(card)		do { (void)(card); } while (0)
static inline int snd_power_wait(struct snd_card *card, unsigned int state) { return 0; }
#define snd_power_get_state(card)	SNDRV_CTL_POWER_D0
#define snd_power_change_state(card, state)	do { (void)(card); } while (0)

#endif /* CONFIG_PM */

struct snd_minor {
	int type;			/* SNDRV_DEVICE_TYPE_XXX */
	int card;			/* card number */
	int device;			/* device number */
	const struct file_operations *f_ops;	/* file operations */
	void *private_data;		/* private data for f_ops->open */
};

/* sound.c */

extern int snd_major;
extern int snd_ecards_limit;

void snd_request_card(int card);

int snd_register_device(int type, struct snd_card *card, int dev,
			const struct file_operations *f_ops, void *private_data,
			const char *name);
int snd_unregister_device(int type, struct snd_card *card, int dev);
void *snd_lookup_minor_data(unsigned int minor, int type);

#ifdef CONFIG_SND_OSSEMUL
int snd_register_oss_device(int type, struct snd_card *card, int dev,
			    const struct file_operations *f_ops, void *private_data,
			    const char *name);
int snd_unregister_oss_device(int type, struct snd_card *card, int dev);
void *snd_lookup_oss_minor_data(unsigned int minor, int type);
#endif

int snd_minor_info_init(void);
int snd_minor_info_done(void);

/* sound_oss.c */

#ifdef CONFIG_SND_OSSEMUL
int snd_minor_info_oss_init(void);
int snd_minor_info_oss_done(void);
#else
#define snd_minor_info_oss_init() /*NOP*/
#define snd_minor_info_oss_done() /*NOP*/
#endif

/* memory.c */

int copy_to_user_fromio(void __user *dst, const volatile void __iomem *src, size_t count);
int copy_from_user_toio(volatile void __iomem *dst, const void __user *src, size_t count);

/* init.c */

extern struct snd_card *snd_cards[SNDRV_CARDS];
int snd_card_locked(int card);
#if defined(CONFIG_SND_MIXER_OSS) || defined(CONFIG_SND_MIXER_OSS_MODULE)
#define SND_MIXER_OSS_NOTIFY_REGISTER	0
#define SND_MIXER_OSS_NOTIFY_DISCONNECT	1
#define SND_MIXER_OSS_NOTIFY_FREE	2
extern int (*snd_mixer_oss_notify_callback)(struct snd_card *card, int cmd);
#endif

struct snd_card *snd_card_new(int idx, const char *id,
			 struct module *module, int extra_size);
int snd_card_disconnect(struct snd_card *card);
int snd_card_free(struct snd_card *card);
int snd_card_free_in_thread(struct snd_card *card);
int snd_card_register(struct snd_card *card);
int snd_card_info_init(void);
int snd_card_info_done(void);
int snd_component_add(struct snd_card *card, const char *component);
int snd_card_file_add(struct snd_card *card, struct file *file);
int snd_card_file_remove(struct snd_card *card, struct file *file);

#ifndef snd_card_set_dev
#define snd_card_set_dev(card,devptr) ((card)->dev = (devptr))
#endif

/* device.c */

int snd_device_new(struct snd_card *card, snd_device_type_t type,
		   void *device_data, struct snd_device_ops *ops);
int snd_device_register(struct snd_card *card, void *device_data);
int snd_device_register_all(struct snd_card *card);
int snd_device_disconnect(struct snd_card *card, void *device_data);
int snd_device_disconnect_all(struct snd_card *card);
int snd_device_free(struct snd_card *card, void *device_data);
int snd_device_free_all(struct snd_card *card, snd_device_cmd_t cmd);

/* isadma.c */

#ifdef CONFIG_ISA_DMA_API
#define DMA_MODE_NO_ENABLE	0x0100

void snd_dma_program(unsigned long dma, unsigned long addr, unsigned int size, unsigned short mode);
void snd_dma_disable(unsigned long dma);
unsigned int snd_dma_pointer(unsigned long dma, unsigned int size);
#endif

/* misc.c */
struct resource;
void release_and_free_resource(struct resource *res);

#ifdef CONFIG_SND_VERBOSE_PRINTK
void snd_verbose_printk(const char *file, int line, const char *format, ...)
     __attribute__ ((format (printf, 3, 4)));
#endif
#if defined(CONFIG_SND_DEBUG) && defined(CONFIG_SND_VERBOSE_PRINTK)
void snd_verbose_printd(const char *file, int line, const char *format, ...)
     __attribute__ ((format (printf, 3, 4)));
#endif

/* --- */

#ifdef CONFIG_SND_VERBOSE_PRINTK
/**
 * snd_printk - printk wrapper
 * @fmt: format string
 *
 * Works like print() but prints the file and the line of the caller
 * when configured with CONFIG_SND_VERBOSE_PRINTK.
 */
#define snd_printk(fmt, args...) \
	snd_verbose_printk(__FILE__, __LINE__, fmt ,##args)
#else
#define snd_printk(fmt, args...) \
	printk(fmt ,##args)
#endif

#ifdef CONFIG_SND_DEBUG

#define __ASTRING__(x) #x

#ifdef CONFIG_SND_VERBOSE_PRINTK
/**
 * snd_printd - debug printk
 * @fmt: format string
 *
 * Compiled only when Works like snd_printk() for debugging purpose.
 * Ignored when CONFIG_SND_DEBUG is not set.
 */
#define snd_printd(fmt, args...) \
	snd_verbose_printd(__FILE__, __LINE__, fmt ,##args)
#else
#define snd_printd(fmt, args...) \
	printk(fmt ,##args)
#endif
/**
 * snd_assert - run-time assertion macro
 * @expr: expression
 *
 * This macro checks the expression in run-time and invokes the commands
 * given in the rest arguments if the assertion is failed.
 * When CONFIG_SND_DEBUG is not set, the expression is executed but
 * not checked.
 */
#define snd_assert(expr, args...) do {					\
	if (unlikely(!(expr))) {					\
		snd_printk(KERN_ERR "BUG? (%s)\n", __ASTRING__(expr));	\
		dump_stack();						\
		args;							\
	}								\
} while (0)

#define snd_BUG() do {				\
	snd_printk(KERN_ERR "BUG?\n");		\
	dump_stack();				\
} while (0)

#else /* !CONFIG_SND_DEBUG */

#define snd_printd(fmt, args...)	/* nothing */
#define snd_assert(expr, args...)	(void)(expr)
#define snd_BUG()			/* nothing */

#endif /* CONFIG_SND_DEBUG */

#ifdef CONFIG_SND_DEBUG_DETECT
/**
 * snd_printdd - debug printk
 * @format: format string
 *
 * Compiled only when Works like snd_printk() for debugging purpose.
 * Ignored when CONFIG_SND_DEBUG_DETECT is not set.
 */
#define snd_printdd(format, args...) snd_printk(format, ##args)
#else
#define snd_printdd(format, args...) /* nothing */
#endif


#define SNDRV_OSS_VERSION         ((3<<16)|(8<<8)|(1<<4)|(0))	/* 3.8.1a */

/* for easier backward-porting */
#if defined(CONFIG_GAMEPORT) || defined(CONFIG_GAMEPORT_MODULE)
#ifndef gameport_set_dev_parent
#define gameport_set_dev_parent(gp,xdev) ((gp)->dev.parent = (xdev))
#define gameport_set_port_data(gp,r) ((gp)->port_data = (r))
#define gameport_get_port_data(gp) (gp)->port_data
#endif
#endif

#include "typedefs.h"

#endif /* __SOUND_CORE_H */
