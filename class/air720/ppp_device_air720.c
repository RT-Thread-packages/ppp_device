/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2019-08-15    xiangxistu      the first version
 */

#include <ppp_device_air720.h>

#define DBG_TAG    "ppp.air720"

#ifdef PPP_DEVICE_DEBUG
#define DBG_LVL   DBG_LOG
#else
#define DBG_LVL   DBG_INFO
#endif

#include <rtdbg.h>

static const struct modem_chat_data mcd[] =
{
    {"+++",          MODEM_CHAT_RESP_NOT_NEED,        30, 1, RT_TRUE},
    {"ATH",          MODEM_CHAT_RESP_OK,              30, 1, RT_FALSE},
    {"AT",           MODEM_CHAT_RESP_OK,              10, 1, RT_FALSE},
    {"ATE0",         MODEM_CHAT_RESP_OK,              1,  1, RT_FALSE},
    {PPP_APN_CMD,    MODEM_CHAT_RESP_OK,              1,  5, RT_FALSE},
    {PPP_DAIL_CMD,   MODEM_CHAT_RESP_CONNECT,         2, 30, RT_FALSE},
};

/*
 * Get into PPP modem,using uart
 *
 * @param NULL
 *
 * @return  0: execute successful
 *         -1: send AT commands error
 *         -5: no memory
 */
static rt_err_t ppp_air720_open(struct ppp_device *device, rt_uint16_t oflag)
{
    rt_device_t uart_device = RT_NULL;
    rt_err_t result = RT_EOK;

    RT_ASSERT(device != RT_NULL);

    uart_device = rt_device_find(device->rely_name);
    if (uart_device == RT_NULL)
    {
        LOG_E("Can't find relying device %s.", device->rely_name);
        return -RT_ERROR;
    }
    result = rt_device_open(uart_device, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX);
    if (result != RT_EOK)
    {
        LOG_E("relying device open(%s) fail.", device->rely_name);
        return RT_NULL;
    }

    return modem_chat(uart_device, mcd, sizeof(mcd) / sizeof(mcd[0]));
}

/* ppp_air720_ops for ppp_device_ops , a common interface */
static struct ppp_device_ops air720_ops =
{
    RT_NULL,
    ppp_air720_open,
    RT_NULL,
    RT_NULL,
};

/*
 * register air720 into ppp_device
 *
 * @parameter   RT_NULL
 *
 *
 *
 *
 * @return  ppp_device function piont
 *
 */
int ppp_air720_register(void)
{
    struct ppp_device *ppp_device = RT_NULL;
    struct ppp_air720 *air720 = RT_NULL;

    air720 = rt_malloc(sizeof(struct ppp_air720));

    ppp_device = &(air720->device);
    ppp_device->ops = &air720_ops;

    LOG_D("ppp air720 is registering ppp_device");

    return ppp_device_register(ppp_device, PPP_DEVICE_NAME, RT_NULL, RT_NULL);
}
INIT_ENV_EXPORT(ppp_air720_register);
