/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2019-09-24     xiaofao        the first version
 */

#include<rtthread.h>
#include<ppp_device_m6312.h>


#define DBG_TAG    "m6312.sample"

#ifdef PPP_DEVICE_DEBUG
#define DBG_LVL   DBG_LOG
#else
#define DBG_LVL   DBG_INFO
#endif

#include <rtdbg.h>

static struct ppp_m6312 m6312;

/*
 * init m6312 mode
 *
 * @param NULL
 *
 *
 * @return  RT_EOK      successful
 *          -RT_ERROR   failed
 *
 */

int ppp_m6312_init(void)
{
    int result = RT_EOK;

    result = ppp_m6312_register(&m6312, PPP_M6312_DEVICE_NAME, PPP_M6312_CLIENT_NAME, RT_NULL);
    if (result != RT_EOK)
    {
        LOG_E("%s registered failed, please try again.", PPP_M6312_DEVICE_NAME);
    }

    return result;
}
INIT_ENV_EXPORT(ppp_m6312_init);

int ppp_m6312_start(void)
{
    rt_device_t device = RT_NULL;

    device = rt_device_find(PPP_M6312_DEVICE_NAME);
    if (device == RT_NULL)
    {
        LOG_E("Cann't find device %s, execute failed", PPP_M6312_DEVICE_NAME);
        return -RT_ERROR;
    }

    if (rt_device_open(device, 0) != RT_EOK)
    {
        LOG_E("Cann't open device %s, execute failed", PPP_M6312_DEVICE_NAME);
        return -RT_ERROR;
    }

    return RT_EOK;
}
INIT_APP_EXPORT(ppp_m6312_start);
MSH_CMD_EXPORT(ppp_m6312_start, a sample create m6312 for dail to network);
/*
 * close m6312 ppp mode, hang up from network
 *
 * @param NULL
 *
 *
 * @return  NULL
 *
 */
int ppp_m6312_stop(void)
{
    rt_device_t device = RT_NULL;

    device = rt_device_find(PPP_M6312_DEVICE_NAME);
    if (device == RT_NULL)
    {
        LOG_E("Cann't find device %s, execute failed", PPP_M6312_DEVICE_NAME);
        return -RT_ERROR;
    }

    if (rt_device_close(device) != RT_EOK)
    {
        LOG_E("Cann't close device %s, execute failed", PPP_M6312_DEVICE_NAME);
        return -RT_ERROR;
    }

    return RT_EOK;
}
MSH_CMD_EXPORT(ppp_m6312_stop, a sample stop m6312 for dail to network);

