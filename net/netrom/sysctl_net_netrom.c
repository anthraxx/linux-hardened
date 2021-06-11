// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *
 * Copyright (C) 1996 Mike Shaver (shaver@zeroknowledge.com)
 */
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/init.h>
#include <net/ax25.h>
#include <net/netrom.h>

/*
 *	Values taken from NET/ROM documentation.
 */
static int min_quality[] __read_only = {0}, max_quality[] __read_only = {255};
static int min_obs[] __read_only     = {0}, max_obs[] __read_only     = {255};
static int min_ttl[] __read_only     = {0}, max_ttl[] __read_only     = {255};
static int min_t1[] __read_only      = {5 * HZ};
static int max_t1[] __read_only      = {600 * HZ};
static int min_n2[] __read_only      = {2}, max_n2[] __read_only      = {127};
static int min_t2[] __read_only      = {1 * HZ};
static int max_t2[] __read_only      = {60 * HZ};
static int min_t4[] __read_only      = {1 * HZ};
static int max_t4[] __read_only      = {1000 * HZ};
static int min_window[] __read_only  = {1}, max_window[] __read_only  = {127};
static int min_idle[] __read_only    = {0 * HZ};
static int max_idle[] __read_only    = {65535 * HZ};
static int min_route[] __read_only   = {0}, max_route[] __read_only   = {1};
static int min_fails[] __read_only   = {1}, max_fails[] __read_only   = {10};
static int min_reset[] __read_only   = {0}, max_reset[] __read_only   = {1};

static struct ctl_table_header *nr_table_header;

static struct ctl_table nr_table[] = {
	{
		.procname	= "default_path_quality",
		.data		= &sysctl_netrom_default_path_quality,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_quality,
		.extra2		= &max_quality
	},
	{
		.procname	= "obsolescence_count_initialiser",
		.data		= &sysctl_netrom_obsolescence_count_initialiser,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_obs,
		.extra2		= &max_obs
	},
	{
		.procname	= "network_ttl_initialiser",
		.data		= &sysctl_netrom_network_ttl_initialiser,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_ttl,
		.extra2		= &max_ttl
	},
	{
		.procname	= "transport_timeout",
		.data		= &sysctl_netrom_transport_timeout,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_t1,
		.extra2		= &max_t1
	},
	{
		.procname	= "transport_maximum_tries",
		.data		= &sysctl_netrom_transport_maximum_tries,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_n2,
		.extra2		= &max_n2
	},
	{
		.procname	= "transport_acknowledge_delay",
		.data		= &sysctl_netrom_transport_acknowledge_delay,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_t2,
		.extra2		= &max_t2
	},
	{
		.procname	= "transport_busy_delay",
		.data		= &sysctl_netrom_transport_busy_delay,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_t4,
		.extra2		= &max_t4
	},
	{
		.procname	= "transport_requested_window_size",
		.data		= &sysctl_netrom_transport_requested_window_size,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_window,
		.extra2		= &max_window
	},
	{
		.procname	= "transport_no_activity_timeout",
		.data		= &sysctl_netrom_transport_no_activity_timeout,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_idle,
		.extra2		= &max_idle
	},
	{
		.procname	= "routing_control",
		.data		= &sysctl_netrom_routing_control,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_route,
		.extra2		= &max_route
	},
	{
		.procname	= "link_fails_count",
		.data		= &sysctl_netrom_link_fails_count,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_fails,
		.extra2		= &max_fails
	},
	{
		.procname	= "reset",
		.data		= &sysctl_netrom_reset_circuit,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_reset,
		.extra2		= &max_reset
	},
	{ }
};

int __init nr_register_sysctl(void)
{
	nr_table_header = register_net_sysctl(&init_net, "net/netrom", nr_table);
	if (!nr_table_header)
		return -ENOMEM;
	return 0;
}

void nr_unregister_sysctl(void)
{
	unregister_net_sysctl_table(nr_table_header);
}
