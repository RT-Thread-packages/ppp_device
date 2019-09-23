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
    {"ATH",          MODEM_CHAT_RESP_OK,        30, 1},
    {"AT",           MODEM_CHAT_RESP_OK,        10, 1},
    {"ATE0",         MODEM_CHAT_RESP_OK,        1,  1},
    {PPP_APN_CMD,    MODEM_CHAT_RESP_OK,        1,  5},
    {PPP_DAIL_CMD,   MODEM_CHAT_RESP_CONNECT,   2, 30},
};

/*
 * Use AT command init modem
 *
 * @param struct ppp_device *device
 *
 * @return  0: execute successful
 *         -1: send AT commands errorstruct struct ppp_device *device *devicestruct ppp_device *device
 *         -5: no memory
 */
static rt_err_t ppp_air720_init(struct ppp_device *device)
{
    RT_ASSERT(device != RT_NULL);
    return RT_EOK;
}

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

    rt_thread_mdelay(1000);
    rt_device_write(uart_device, 0, "+++", 3);
    rt_thread_mdelay(500);

    return modem_chat(uart_device, mcd, sizeof(mcd) / sizeof(mcd[0]));
}

/*
 * close ppp , deinit uart
 *
 * @param NULL
 *
 * @return  0: execute successful
 *         -1: send AT commands error
 *         -5: no memory
 */
static rt_err_t ppp_air720_close(struct ppp_device *device)
{
    RT_ASSERT(device != RT_NULL);
    return RT_EOK;
}

/*
 * control,using AT command to get csq,imei,net_type value
 *
 * @param struct ppp_device *
 *               cmd
 *               *arg
 * @return  0: execute successful
 *         -1: send AT commands error
 *         -5: no memory
 */
static rt_err_t ppp_air720_control(struct ppp_device *device, int cmd, void *arg)
{
    RT_ASSERT(device != RT_NULL);
    return RT_EOK;
}

/* ppp_air720_ops for ppp_device_ops , a common interface */
static struct ppp_device_ops air720_ops =
{
    ppp_air720_init,
    ppp_air720_open,
    ppp_air720_close,
    ppp_air720_control
};

/*
 * register air720 into ppp_device
 *
 * @param struct ppp_air720 *       piont
 *        const char *              name
 *        int                       flag
 *        void *                    resever data
 * @return  ppp_device function piont
 *
 */
int ppp_air720_register(struct ppp_air720 *air720, const char *dev_name, const char *uart_name, void *user_data)
{
    struct ppp_device *ppp_device = RT_NULL;

    RT_ASSERT(air720 != RT_NULL);

    ppp_device = &(air720->device);
    ppp_device->ops = &air720_ops;

    LOG_D("ppp air720 is registering ppp_device");

    return ppp_device_register(ppp_device, dev_name, uart_name, user_data);
}
