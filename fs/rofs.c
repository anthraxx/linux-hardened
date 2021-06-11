// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mount.h>
#include <linux/major.h>
#include <linux/fs.h>

int enable_rofs __read_mostly = 0;

int
handle_rofs_mount(struct dentry *dentry, struct vfsmount *mnt, int mnt_flags)
{
	if (enable_rofs && !(mnt_flags & MNT_READONLY))
		return -EPERM;
	else
		return 0;
}

int
handle_rofs_blockwrite(struct dentry *dentry, struct vfsmount *mnt, int acc_mode)
{
	struct inode *inode = d_backing_inode(dentry);

	if (enable_rofs && (acc_mode & MAY_WRITE) &&
	    inode && (S_ISBLK(inode->i_mode) || (S_ISCHR(inode->i_mode) && imajor(inode) == RAW_MAJOR)))
		return -EPERM;
	else
		return 0;
}
