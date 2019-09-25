/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2019-08-15    xiangxistu      the first version
 */

#include<rtthread.h>
#include<ppp_device.h>

#define PPP_DEVICE_NAME "pp"

#define DBG_TAG    "ppp.sample"

#ifdef PPP_DEVICE_DEBUG
#define DBG_LVL   DBG_LOG
#else
#define DBG_LVL   DBG_INFO
#endif

#include <rtdbg.h>

#ifdef PPP_DEVICE_USING_AIR720
#include "ppp_device_air720.h"
static struct ppp_air720 ppp_device;
#endif
#ifdef PPP_DEVICE_USING_M6312
#include "ppp_device_m6312.h"
static struct ppp_m6312 ppp_device;
#endif
#ifdef PPP_DEVICE_USING_SIM800
#include "ppp_device_sim800.h"
static struct ppp_sim800 ppp_device;
#endif

/* register device into rt_device frame */
int ppp_sample_register(void)
{
    int result = RT_EOK;

#ifdef PPP_DEVICE_USING_AIR720
    result = ppp_air720_register(&ppp_device, PPP_DEVICE_NAME, PPP_CLIENT_NAME, RT_NULL);
#endif
#ifdef PPP_DEVICE_USING_M6312
    result = ppp_m6312_register(&ppp_device, PPP_DEVICE_NAME, PPP_CLIENT_NAME, RT_NULL);
#endif
#ifdef PPP_DEVICE_USING_SIM800
    result = ppp_sim800_register(&ppp_device, PPP_DEVICE_NAME, PPP_CLIENT_NAME, RT_NULL);
#endif
    if (result != RT_EOK)
    {
        LOG_E("%s registered failed, please try again.", PPP_DEVICE_NAME);
    }

    return result;
}
INIT_ENV_EXPORT(ppp_sample_register);

/* if you want connect network again, you can use this function to create a new ppp link */
int ppp_sample_start(void)
{
    rt_device_t device = RT_NULL;

    device = rt_device_find(PPP_DEVICE_NAME);
    if (device == RT_NULL)
    {
        LOG_E("Cann't find device %s, execute failed", PPP_DEVICE_NAME);
        return -RT_ERROR;
    }

    if (rt_device_open(device, 0) != RT_EOK)
    {
        LOG_E("Cann't open device %s, execute failed", PPP_DEVICE_NAME);
        return -RT_ERROR;
    }

    return RT_EOK;
}
INIT_APP_EXPORT(ppp_sample_start);
MSH_CMD_EXPORT_ALIAS(ppp_sample_start, ppp_start, a sample of ppp device  for dailing to network);

/* close ppp link ,turn off modem form network */
int ppp_sample_stop(void)
{
    rt_device_t device = RT_NULL;

    device = rt_device_find(PPP_DEVICE_NAME);
    if (device == RT_NULL)
    {
        LOG_E("Cann't find device %s, execute failed", PPP_DEVICE_NAME);
        return -RT_ERROR;
    }

    if (rt_device_close(device) != RT_EOK)
    {
        LOG_E("Cann't close device %s, execute failed", PPP_DEVICE_NAME);
        return -RT_ERROR;
    }

    return RT_EOK;
}
MSH_CMD_EXPORT_ALIAS(ppp_sample_stop, ppp_stop, a sample of ppp device for turning off network);
