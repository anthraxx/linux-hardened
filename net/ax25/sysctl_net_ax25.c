// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *
 * Copyright (C) 1996 Mike Shaver (shaver@zeroknowledge.com)
 */
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/spinlock.h>
#include <net/ax25.h>

static int min_ipdefmode[1],    	max_ipdefmode[] __read_only = {1};
static int min_axdefmode[1],            max_axdefmode[] __read_only = {1};
static int min_backoff[1],		max_backoff[] __read_only = {2};
static int min_conmode[1],		max_conmode[] __read_only = {2};
static int min_window[] __read_only = {1},		max_window[] __read_only = {7};
static int min_ewindow[] __read_only = {1},		max_ewindow[] __read_only = {63};
static int min_t1[] __read_only = {1},		max_t1[] __read_only = {30000};
static int min_t2[] __read_only = {1},		max_t2[] __read_only = {20000};
static int min_t3[1],			max_t3[] __read_only = {3600000};
static int min_idle[1],			max_idle[] __read_only = {65535000};
static int min_n2[] __read_only = {1},		max_n2[] __read_only = {31};
static int min_paclen[] __read_only = {1},		max_paclen[] __read_only = {512};
static int min_proto[1],		max_proto[] __read_only = { AX25_PROTO_MAX };
#ifdef CONFIG_AX25_DAMA_SLAVE
static int min_ds_timeout[1],		max_ds_timeout[] __read_only = {65535000};
#endif

static const struct ctl_table ax25_param_table[] = {
	{
		.procname	= "ip_default_mode",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_ipdefmode,
		.extra2		= &max_ipdefmode
	},
	{
		.procname	= "ax25_default_mode",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_axdefmode,
		.extra2		= &max_axdefmode
	},
	{
		.procname	= "backoff_type",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_backoff,
		.extra2		= &max_backoff
	},
	{
		.procname	= "connect_mode",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_conmode,
		.extra2		= &max_conmode
	},
	{
		.procname	= "standard_window_size",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_window,
		.extra2		= &max_window
	},
	{
		.procname	= "extended_window_size",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_ewindow,
		.extra2		= &max_ewindow
	},
	{
		.procname	= "t1_timeout",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_t1,
		.extra2		= &max_t1
	},
	{
		.procname	= "t2_timeout",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_t2,
		.extra2		= &max_t2
	},
	{
		.procname	= "t3_timeout",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_t3,
		.extra2		= &max_t3
	},
	{
		.procname	= "idle_timeout",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_idle,
		.extra2		= &max_idle
	},
	{
		.procname	= "maximum_retry_count",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_n2,
		.extra2		= &max_n2
	},
	{
		.procname	= "maximum_packet_length",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_paclen,
		.extra2		= &max_paclen
	},
	{
		.procname	= "protocol",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_proto,
		.extra2		= &max_proto
	},
#ifdef CONFIG_AX25_DAMA_SLAVE
	{
		.procname	= "dama_slave_timeout",
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_ds_timeout,
		.extra2		= &max_ds_timeout
	},
#endif

	{ }	/* that's all, folks! */
};

int ax25_register_dev_sysctl(ax25_dev *ax25_dev)
{
	char path[sizeof("net/ax25/") + IFNAMSIZ];
	int k;
	struct ctl_table *table;

	table = kmemdup(ax25_param_table, sizeof(ax25_param_table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	for (k = 0; k < AX25_MAX_VALUES; k++)
		table[k].data = &ax25_dev->values[k];

	snprintf(path, sizeof(path), "net/ax25/%s", ax25_dev->dev->name);
	ax25_dev->sysheader = register_net_sysctl(&init_net, path, table);
	if (!ax25_dev->sysheader) {
		kfree(table);
		return -ENOMEM;
	}
	return 0;
}

void ax25_unregister_dev_sysctl(ax25_dev *ax25_dev)
{
	struct ctl_table_header *header = ax25_dev->sysheader;
	struct ctl_table *table;

	if (header) {
		ax25_dev->sysheader = NULL;
		table = header->ctl_table_arg;
		unregister_net_sysctl_table(header);
		kfree(table);
	}
}
