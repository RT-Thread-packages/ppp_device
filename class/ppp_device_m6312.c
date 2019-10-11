/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2019-09-24     xiaofao        the first version
 */

#include <ppp_device.h>
#include <ppp_chat.h>
#include <rtdevice.h>

#define DBG_TAG    "ppp.m6312"

#ifdef PPP_DEVICE_DEBUG
#define DBG_LVL   DBG_LOG
#else
#define DBG_LVL   DBG_INFO
#endif
#include <rtdbg.h>

#define M6312_POWER_ON  PIN_HIGH
#define M6312_POWER_OFF PIN_LOW
#ifndef M6312_POWER_PIN
#define M6312_POWER_PIN -1
#endif

#define M6312_WARTING_TIME_BASE 500

static const struct modem_chat_data rst_mcd[] =
{
    {"+++",          MODEM_CHAT_RESP_OK,        10, 1, RT_FALSE},
    {"ATH",          MODEM_CHAT_RESP_OK,        1,  1, RT_FALSE},
    {"AT+CMRESET",   MODEM_CHAT_RESP_NOT_NEED,  1,  1, RT_FALSE},
};

static const struct modem_chat_data mcd[] =
{
    {"AT",           MODEM_CHAT_RESP_OK,        10, 1, RT_FALSE},
    {"ATE0",         MODEM_CHAT_RESP_OK,        1,  1, RT_FALSE},
    {PPP_APN_CMD,    MODEM_CHAT_RESP_OK,        1,  5, RT_FALSE},
    {PPP_DAIL_CMD,   MODEM_CHAT_RESP_CONNECT,   1, 30, RT_FALSE},
};

static rt_err_t ppp_m6312_prepare(struct ppp_device *device)
{
    if (device->power_pin >= 0)
    {
        rt_pin_write(device->power_pin, M6312_POWER_OFF);
        rt_thread_mdelay(M6312_WARTING_TIME_BASE);
        rt_pin_write(device->power_pin, M6312_POWER_ON);
    }
    else
    {
        rt_err_t err;
        err = modem_chat(device->uart, rst_mcd, sizeof(rst_mcd) / sizeof(rst_mcd[0]));
        if (err)
            return err;
    }
    return modem_chat(device->uart, mcd, sizeof(mcd) / sizeof(mcd[0]));
}

/* ppp_m6312_ops for ppp_device_ops , a common interface */
static struct ppp_device_ops m6312_ops =
{
    .prepare = ppp_m6312_prepare,
};

/**
 * register m6312 into ppp_device
 *
 * @return  =0:   ppp_device register successfully
 *          <0:   ppp_device register failed
 */
int ppp_m6312_register(void)
{
    struct ppp_device *ppp_device = RT_NULL;

    ppp_device = rt_malloc(sizeof(struct ppp_device));
    if(ppp_device == RT_NULL)
    {
        LOG_E("No memory for struct m6312 ppp_device.");
        return -RT_ENOMEM;
    }

    ppp_device->power_pin = M6312_POWER_PIN;
    if (ppp_device->power_pin >= 0)
    {
        rt_pin_mode(ppp_device->power_pin, PIN_MODE_OUTPUT);
        rt_pin_write(ppp_device->power_pin, M6312_POWER_OFF);
    }

    ppp_device->ops = &m6312_ops;
    LOG_D("ppp m6312 is registering ppp_device");
    return ppp_device_register(ppp_device, PPP_DEVICE_NAME, RT_NULL, RT_NULL);
}
INIT_COMPONENT_EXPORT(ppp_m6312_register);
