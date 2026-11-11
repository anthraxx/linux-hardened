/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_MEMFD_H
#define __LINUX_MEMFD_H

#include <linux/fcntl.h>
#include <linux/file.h>

static inline int memfd_check_seals_mode(unsigned int seals, umode_t old_mode,
		umode_t new_mode)
{
	if ((seals & F_SEAL_EXEC) && ((old_mode ^ new_mode) & 0111))
		return -EPERM;
	return 0;
}

#ifdef CONFIG_MEMFD_CREATE
extern long memfd_fcntl(struct file *file, unsigned int cmd, unsigned int arg);
unsigned int *memfd_file_seals_ptr(struct file *file);
#else
static inline long memfd_fcntl(struct file *f, unsigned int c, unsigned int a)
{
	return -EINVAL;
}

static inline unsigned int *memfd_file_seals_ptr(struct file *file)
{
	return NULL;
}
#endif

/* Retrieve memfd seals associated with the file, if any. */
static inline unsigned int memfd_file_seals(struct file *file)
{
	unsigned int *sealsp = memfd_file_seals_ptr(file);

	return sealsp ? *sealsp : 0;
}

#endif /* __LINUX_MEMFD_H */
