#ifndef __NET_RTNETLINK_H
#define __NET_RTNETLINK_H

#include <linux/rtnetlink.h>
#include <net/netlink.h>

typedef int (*rtnl_doit_func)(struct sk_buff *, struct nlmsghdr *, void *);
typedef int (*rtnl_dumpit_func)(struct sk_buff *, struct netlink_callback *);

extern int	__rtnl_register(int protocol, int msgtype,
				rtnl_doit_func, rtnl_dumpit_func);
extern void	rtnl_register(int protocol, int msgtype,
			      rtnl_doit_func, rtnl_dumpit_func);
extern int	rtnl_unregister(int protocol, int msgtype);
extern void	rtnl_unregister_all(int protocol);
extern int	rtnl_dump_all(struct sk_buff *skb, struct netlink_callback *cb);

#endif
