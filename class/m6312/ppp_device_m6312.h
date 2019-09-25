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

extern int ppp_m6312_register(struct ppp_sample *m6312, const char *dev_name, const char *rely_name, void *user_data);

#endif  /* __PPP_M6312_H__ */
