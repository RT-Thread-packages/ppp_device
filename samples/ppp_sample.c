/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2019-08-15    xiangxistu      the first version
 * 2019-09-26    xiangxistu      add ppp_device_attach and ppp_device_detach
 */

#include<rtthread.h>
#include<ppp_device.h>

#define DBG_TAG    "ppp.sample"

#ifdef PPP_DEVICE_DEBUG
#define DBG_LVL   DBG_LOG
#else
#define DBG_LVL   DBG_INFO
#endif

#include <rtdbg.h>

/* if you want connect network again, you can use this function to create a new ppp link */
int ppp_sample_start(void)
{
    rt_device_t device = RT_NULL;
    device = rt_device_find(PPP_DEVICE_NAME);
    if(device == RT_NULL)
    {
        LOG_E("Can't find device (%s).", PPP_DEVICE_NAME);
        return -RT_ERROR;
    }
    if(ppp_device_attach((struct ppp_device *)device, PPP_CLIENT_NAME, RT_NULL) != RT_EOK)
    {
        LOG_E("ppp_device_attach execute failed.");
        return -RT_ERROR;
    }
    return RT_EOK;
}
#ifndef PKG_USING_CMUX
INIT_APP_EXPORT(ppp_sample_start);
#endif
MSH_CMD_EXPORT_ALIAS(ppp_sample_start, ppp_start, a sample of ppp device  for dailing to network);

/* close ppp link ,turn off modem form network */
int ppp_sample_stop(void)
{
    rt_device_t device = RT_NULL;
    device = rt_device_find(PPP_DEVICE_NAME);
    if(device == RT_NULL)
    {
        LOG_E("Can't find device (%s).", PPP_DEVICE_NAME);
        return -RT_ERROR;
    }
    if(ppp_device_detach((struct ppp_device *)device) != RT_EOK)
    {
        LOG_E("ppp_device_detach execute failed.");
        return -RT_ERROR;
    }
    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(ppp_sample_stop, ppp_stop, a sample of ppp device for turning off network);

#ifdef PPP_DEVICE_DEBUG_DROP
static int ppp_drop(void)
{
    rt_device_t device = RT_NULL;
    struct ppp_device *ppp_device;
    device = rt_device_find(PPP_DEVICE_NAME);
    if(device == RT_NULL)
    {
        LOG_E("Can't find device (%s).", PPP_DEVICE_NAME);
        return -RT_ERROR;
    }
    ppp_device = (struct ppp_device*)device;
    LOG_I("%ld", (unsigned long)(ppp_device->dropcnt + ppp_device->droppos));
    return RT_EOK;
 }

MSH_CMD_EXPORT_ALIAS(ppp_drop, ppp_drop, show drop statistics);
#endif
