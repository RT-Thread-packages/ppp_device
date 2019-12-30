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

#define DBG_TAG    "ppp.ec20"

#ifdef PPP_DEVICE_DEBUG
#define DBG_LVL   DBG_LOG
#else
#define DBG_LVL   DBG_INFO
#endif
#include <rtdbg.h>


#define EC20_POWER_ON  PIN_LOW
#define EC20_POWER_OFF PIN_HIGH
#ifndef EC20_POWER_PIN
#include <drv_gpio.h>
#define EC20_POWER_PIN GET_PIN(A, 11)
#endif

#define EC20_WARTING_TIME_BASE 2000

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

static rt_err_t ppp_ec20_prepare(struct ppp_device *device)
{
    if (device->power_pin >= 0)
    {
        rt_pin_write(device->power_pin, EC20_POWER_OFF);
        rt_thread_mdelay(EC20_WARTING_TIME_BASE / 20);
        rt_pin_write(device->power_pin, EC20_POWER_ON);
        rt_thread_mdelay(EC20_WARTING_TIME_BASE / 2 + EC20_WARTING_TIME_BASE);
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

/* ppp_ec20_ops for ppp_device_ops , a common interface */
static struct ppp_device_ops ec20_ops =
{
    .prepare = ppp_ec20_prepare,
};

/**
 * register ec20 into ppp_device
 *
 * @return  =0:   ppp_device register successfully
 *          <0:   ppp_device register failed
 */
int ppp_ec20_register(void)
{
    struct ppp_device *ppp_device = RT_NULL;

    ppp_device = rt_malloc(sizeof(struct ppp_device));
    if(ppp_device == RT_NULL)
    {
        LOG_E("No memory for ec20 ppp_device.");
        return -RT_ENOMEM;
    }

    ppp_device->power_pin = EC20_POWER_PIN;
    if (ppp_device->power_pin >= 0)
    {
        rt_pin_mode(ppp_device->power_pin, PIN_MODE_OUTPUT);
        rt_pin_write(ppp_device->power_pin, EC20_POWER_ON);
        rt_thread_mdelay(EC20_WARTING_TIME_BASE / 2 + EC20_WARTING_TIME_BASE);
    }
    ppp_device->ops = &ec20_ops;

    LOG_D("ppp ec20 is registering ppp_device");

    return ppp_device_register(ppp_device, PPP_DEVICE_NAME, RT_NULL, RT_NULL);
}
INIT_COMPONENT_EXPORT(ppp_ec20_register);
