#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/init.h>
#include <linux/sysctl.h>
#include <linux/usb.h>

static int zero = 0;
static int one = 1;

static struct ctl_table usb_table[] = {
	{
		.procname	= "deny_new_usb",
		.data		= &deny_new_usb,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax_sysadmin,
		.extra1		= &zero,
		.extra2		= &one,
	},
	{ }
};

static struct ctl_table usb_root_table[] = {
	{ .procname	= "kernel",
	  .mode		= 0555,
	  .child	= usb_table },
	{ }
};

static struct ctl_table_header *usb_table_header;

int __init usb_init_sysctl(void)
{
	usb_table_header = register_sysctl_table(usb_root_table);
	if (!usb_table_header) {
		pr_warn("usb: sysctl registration failed\n");
		return -ENOMEM;
	}

	return 0;
}

void usb_exit_sysctl(void)
{
	unregister_sysctl_table(usb_table_header);
}
