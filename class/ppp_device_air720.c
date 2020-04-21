/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2019-08-15    xiangxistu      the first version
 */

#include <ppp_device.h>
#include <ppp_chat.h>
#include <rtdevice.h>

#define DBG_TAG    "ppp.air720"

#ifdef PPP_DEVICE_DEBUG
#define DBG_LVL   DBG_LOG
#else
#define DBG_LVL   DBG_INFO
#endif
#include <rtdbg.h>

#define AIR720_POWER_ON  PIN_HIGH
#define AIR720_POWER_OFF PIN_LOW
#ifndef AIR720_POWER_PIN
#define AIR720_POWER_PIN -1
#endif
#define AIR720_WARTING_TIME_BASE 2000

#ifndef PKG_USING_CMUX
static const struct modem_chat_data rst_mcd[] =
{
    {"+++",          MODEM_CHAT_RESP_NOT_NEED,        30, 1, RT_TRUE},
    {"ATH",          MODEM_CHAT_RESP_OK,              30, 1, RT_FALSE},
};

static const struct modem_chat_data mcd[] =
{
    {"AT",           MODEM_CHAT_RESP_OK,              10, 1, RT_FALSE},
    {"ATE0",         MODEM_CHAT_RESP_OK,              1,  1, RT_FALSE},
    {PPP_APN_CMD,    MODEM_CHAT_RESP_OK,              1,  5, RT_FALSE},
    {PPP_DAIL_CMD,   MODEM_CHAT_RESP_CONNECT,         1, 30, RT_FALSE},
};
#else
static const struct modem_chat_data mcd[] =
{
    {"AT",           MODEM_CHAT_RESP_NOT_NEED,        10, 1, RT_FALSE},
    {PPP_APN_CMD,    MODEM_CHAT_RESP_NOT_NEED,        1,  5, RT_FALSE},
    {PPP_DAIL_CMD,   MODEM_CHAT_RESP_NOT_NEED,        1, 30, RT_FALSE},
};
#endif

static rt_err_t ppp_air720_prepare(struct ppp_device *device)
{
#ifndef PKG_USING_CMUX
    if (device->power_pin >= 0)
    {
        rt_pin_write(device->power_pin, AIR720_POWER_OFF);
        rt_thread_mdelay(AIR720_WARTING_TIME_BASE / 20);
        rt_pin_write(device->power_pin, AIR720_POWER_ON);
        rt_thread_mdelay(AIR720_WARTING_TIME_BASE);
    }
    else
    {
        rt_err_t err;
        err = modem_chat(device->uart, rst_mcd, sizeof(rst_mcd) / sizeof(rst_mcd[0]));
        if (err)
            return err;
    }
#endif
    return modem_chat(device->uart, mcd, sizeof(mcd) / sizeof(mcd[0]));
}

/* ppp_air720_ops for ppp_device_ops , a common interface */
static struct ppp_device_ops air720_ops =
{
    .prepare = ppp_air720_prepare,
};

/**
 * register air720 into ppp_device
 *
 * @return  =0:   ppp_device register successfully
 *          <0:   ppp_device register failed
 */
int ppp_air720_register(void)
{
    struct ppp_device *ppp_device = RT_NULL;

    ppp_device = rt_malloc(sizeof(struct ppp_device));
    if(ppp_device == RT_NULL)
    {
        LOG_E("No memory for air720 ppp_device.");
        return -RT_ENOMEM;
    }

    ppp_device->power_pin = AIR720_POWER_PIN;
    if (ppp_device->power_pin >= 0)
    {
        rt_pin_mode(ppp_device->power_pin, PIN_MODE_OUTPUT);
        rt_pin_write(ppp_device->power_pin, AIR720_POWER_ON);
        rt_thread_mdelay(AIR720_WARTING_TIME_BASE);
    }
    ppp_device->ops = &air720_ops;

    LOG_D("ppp air720 is registering ppp_device");

    return ppp_device_register(ppp_device, PPP_DEVICE_NAME, RT_NULL, RT_NULL);
}
INIT_COMPONENT_EXPORT(ppp_air720_register);
