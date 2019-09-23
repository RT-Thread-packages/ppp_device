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
#include<ppp_device_air720.h>


#define DBG_TAG    "air720.sample"

#ifdef PPP_DEVICE_DEBUG
#define DBG_LVL   DBG_LOG
#else
#define DBG_LVL   DBG_INFO
#endif

#include <rtdbg.h>

static struct ppp_air720 air720;

/*
 * init air720 mode
 *
 * @param NULL
 *
 *
 * @return  RT_EOK      successful
 *          -RT_ERROR   failed
 *
 */

int air720_register(void)
{
    int result = RT_EOK;

    result = ppp_air720_register(&air720, PPP_AIR720_DEVICE_NAME, PPP_AIR720_CLIENT_NAME, RT_NULL);
    if (result != RT_EOK)
    {
        LOG_E("%s registered failed, please try again.", PPP_AIR720_DEVICE_NAME);
    }

    return result;
}
INIT_ENV_EXPORT(air720_register);

int air720_start(void)
{
    rt_device_t device = RT_NULL;

    device = rt_device_find(PPP_AIR720_DEVICE_NAME);
    if (device == RT_NULL)
    {
        LOG_E("Cann't find device %s, execute failed", PPP_AIR720_DEVICE_NAME);
        return -RT_ERROR;
    }

    if (rt_device_open(device, 0) != RT_EOK)
    {
        LOG_E("Cann't open device %s, execute failed", PPP_AIR720_DEVICE_NAME);
        return -RT_ERROR;
    }

    return RT_EOK;
}
INIT_APP_EXPORT(air720_start);
MSH_CMD_EXPORT(air720_start, a sample create air720 for dail to network);
/*
 * close air720 ppp mode, hang up from network
 *
 * @param NULL
 *
 *
 * @return  NULL
 *
 */
int ppp_air720_stop(void)
{
    rt_device_t device = RT_NULL;

    device = rt_device_find(PPP_AIR720_DEVICE_NAME);
    if (device == RT_NULL)
    {
        LOG_E("Cann't find device %s, execute failed", PPP_AIR720_DEVICE_NAME);
        return -RT_ERROR;
    }

    if (rt_device_close(device) != RT_EOK)
    {
        LOG_E("Cann't close device %s, execute failed", PPP_AIR720_DEVICE_NAME);
        return -RT_ERROR;
    }

    return RT_EOK;
}
MSH_CMD_EXPORT(ppp_air720_stop, a sample stop air720 for dail to network);
