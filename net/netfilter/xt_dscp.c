/* IP tables module for matching the value of the IPv4/IPv6 DSCP field
 *
 * xt_dscp.c,v 1.3 2002/08/05 19:00:21 laforge Exp
 *
 * (C) 2002 by Harald Welte <laforge@netfilter.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/dsfield.h>

#include <linux/netfilter/xt_dscp.h>
#include <linux/netfilter/x_tables.h>

MODULE_AUTHOR("Harald Welte <laforge@netfilter.org>");
MODULE_DESCRIPTION("x_tables DSCP matching module");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ipt_dscp");
MODULE_ALIAS("ip6t_dscp");

static int match(const struct sk_buff *skb,
		 const struct net_device *in,
		 const struct net_device *out,
		 const struct xt_match *match,
		 const void *matchinfo,
		 int offset,
		 unsigned int protoff,
		 int *hotdrop)
{
	const struct xt_dscp_info *info = matchinfo;
	u_int8_t dscp = ipv4_get_dsfield(skb->nh.iph) >> XT_DSCP_SHIFT;

	return (dscp == info->dscp) ^ !!info->invert;
}

static int match6(const struct sk_buff *skb,
		  const struct net_device *in,
		  const struct net_device *out,
		  const struct xt_match *match,
		  const void *matchinfo,
		  int offset,
		  unsigned int protoff,
		  int *hotdrop)
{
	const struct xt_dscp_info *info = matchinfo;
	u_int8_t dscp = ipv6_get_dsfield(skb->nh.ipv6h) >> XT_DSCP_SHIFT;

	return (dscp == info->dscp) ^ !!info->invert;
}

static int checkentry(const char *tablename,
		      const void *info,
		      const struct xt_match *match,
		      void *matchinfo,
		      unsigned int matchsize,
		      unsigned int hook_mask)
{
	const u_int8_t dscp = ((struct xt_dscp_info *)matchinfo)->dscp;

	if (dscp > XT_DSCP_MAX) {
		printk(KERN_ERR "xt_dscp: dscp %x out of range\n", dscp);
		return 0;
	}

	return 1;
}

static struct xt_match dscp_match = {
	.name		= "dscp",
	.match		= match,
	.checkentry	= checkentry,
	.matchsize	= sizeof(struct xt_dscp_info),
	.family		= AF_INET,
	.me		= THIS_MODULE,
};

static struct xt_match dscp6_match = {
	.name		= "dscp",
	.match		= match6,
	.checkentry	= checkentry,
	.matchsize	= sizeof(struct xt_dscp_info),
	.family		= AF_INET6,
	.me		= THIS_MODULE,
};

static int __init xt_dscp_match_init(void)
{
	int ret;
	ret = xt_register_match(&dscp_match);
	if (ret)
		return ret;

	ret = xt_register_match(&dscp6_match);
	if (ret)
		xt_unregister_match(&dscp_match);

	return ret;
}

static void __exit xt_dscp_match_fini(void)
{
	xt_unregister_match(&dscp_match);
	xt_unregister_match(&dscp6_match);
}

module_init(xt_dscp_match_init);
module_exit(xt_dscp_match_fini);
