/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author          Notes
 * 2019-08-15     xiangxistu      the first version
 */

#ifndef __NETIF_PPPNETIF_H__
#define __NETIF_PPPNETIF_H__

#include "lwip/netif.h"
#include <rtthread.h>

#define NIOCTL_GADDR        0x01
#ifndef RT_LWIP_PPP_MTU
#define PPPNET_MTU      1500
#else
#define PPPNET_MTU      RT_LWIP_PPP_MTU
#endif

/* eth flag with auto_linkup or phy_linkup */
#define ETHIF_LINK_AUTOUP   0x0000
#define ETHIF_LINK_PHYUP    0x0100

/* proviode a public interface to register netdev */
rt_err_t ppp_netdev_add(struct netif *ppp_netif);
rt_err_t ppp_netdev_refresh(struct netif *ppp_netif);
void ppp_netdev_del(struct netif *ppp_netif);
extern struct netdev *netdev_get_by_name(const char *name);
extern int netdev_unregister(struct netdev *netdev);

#endif /* __NETIF_PPPNETIF_H__ */
