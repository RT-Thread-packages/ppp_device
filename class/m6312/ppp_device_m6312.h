/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2019-09-24     xiaofao        the first version
 */

#ifndef __PPP_M6312_H__
#define __PPP_M6312_H__

#include <ppp_device.h>

/* ppp_device base from ppp_device */
struct ppp_m6312
{
    struct ppp_device  device;          /* ppp_device struct in ppp_air720 */
    enum ppp_trans_type type;           /* the type is used to establish a ppp connection */
};

extern int ppp_m6312_register(void);

#endif  /* __PPP_M6312_H__ */
