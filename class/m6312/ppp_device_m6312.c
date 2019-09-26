/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2019-09-24     xiaofao        the first version
 */

#include <ppp_device_m6312.h>

#define DBG_TAG    "ppp.m6312"

#ifdef PPP_DEVICE_DEBUG
#define DBG_LVL   DBG_LOG
#else
#define DBG_LVL   DBG_INFO
#endif

#include <rtdbg.h>

static const struct modem_chat_data mcd[] =
{
    {"+++",          MODEM_CHAT_RESP_OK,        10, 1, RT_FALSE},
    {"ATH",          MODEM_CHAT_RESP_OK,        1,  1, RT_FALSE},
    {"AT+CMRESET",   MODEM_CHAT_RESP_NOT_NEED,  1,  1, RT_FALSE},
    {"AT",           MODEM_CHAT_RESP_OK,        10, 1, RT_FALSE},
    {"ATE0",         MODEM_CHAT_RESP_OK,        1,  1, RT_FALSE},
    {PPP_APN_CMD,    MODEM_CHAT_RESP_OK,        1,  5, RT_FALSE},
    {PPP_DAIL_CMD,   MODEM_CHAT_RESP_CONNECT,   2, 30, RT_FALSE},
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
static rt_err_t ppp_m6312_open(struct ppp_device *device, rt_uint16_t oflag)
{
    RT_ASSERT(device != RT_NULL);

    return modem_chat(device->uart_name, mcd, sizeof(mcd) / sizeof(mcd[0]));
}

/* ppp_m6312_ops for ppp_device_ops , a common interface */
static struct ppp_device_ops m6312_ops =
{
    RT_NULL,
    ppp_m6312_open,
    RT_NULL,
    RT_NULL,
};

/*
 * register m6312 into ppp_device
 *
 * @param
 *
 *
 *
 * @return  ppp_device function piont
 *
 */
int ppp_m6312_register(void)
{
    struct ppp_device *ppp_device = RT_NULL;
    struct ppp_m6312 *m6312 = RT_NULL;

    m6312 = rt_malloc(sizeof(struct ppp_m6312));
    if(m6312 == RT_NULL)
    {
        LOG_E("No memory for struct m6312.");
        return -RT_ENOMEM;
    }

    ppp_device = &(m6312->device);
    ppp_device->ops = &m6312_ops;

    LOG_D("ppp m6312 is registering ppp_device");

    return ppp_device_register(ppp_device, PPP_DEVICE_NAME, RT_NULL, RT_NULL);
}
INIT_COMPONENT_EXPORT(ppp_m6312_register);
