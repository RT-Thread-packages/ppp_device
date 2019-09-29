/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author          Notes
 * 2019-08-15     xiangxistu      the first version
 */

#ifndef __PPP_DEVICE_H__
#define __PPP_DEVICE_H__

#include <rtdef.h>

#include <netif/ppp/ppp.h>
#include <netif/ppp/pppos.h>
#include <netif/ppp/pppapi.h>
#include <lwip/dns.h>
#include <lwip/netif.h>

#ifndef PPP_DEVICE_NAME
#define PPP_DEVICE_NAME "pp"
#endif

#define PPP_DAIL_CMD         "ATD*99#"                                  /* common dailing cmd */
#ifdef  PPP_APN_CMCC
#define PPP_APN_CMD          "AT+CGDCONT=1,\"IP\",\"CMNET\""            /* China Mobile Communication Company */
#endif
#ifdef  PPP_APN_CUCC
#define PPP_APN_CMD          "AT+CGDCONT=1,\"IP\",\"UNINET\""           /* China Unicom Communication Company */
#endif
#ifdef  PPP_APN_CTCC
#define PPP_APN_CMD          "AT+CGDCONT=1,\"IP\",\"CTNET\""            /* China Telecom Communication Company */
#endif
#define PPP_CTL_GET_CSQ      1
#define PPP_CTL_GET_IEMI     2
#define PPP_CTL_GET_TYPE     3
#define PPP_CTL_PREPARE      10

#define PPP_FRAME_MAX       1550

enum ppp_trans_type
{
    PPP_TRANS_CHAT,
    PPP_TRANS_AT_CLIENT
};

enum ppp_conn_type
{
    PPP_CONNET_UART,                            /* using uart connect ppp */
    PPP_CONNET_USB                              /* using  usb connect ppp */
};

/**
 * @brief PPPoS Client IP Information
 *
 */
struct ppp_device
{
    struct rt_device parent;                    /* join rt_device frame */
    rt_device_t uart;
    char *uart_name;                            /* the name of the low-level driver device */
    const struct ppp_device_ops *ops;           /* ppp device ops interface */
    enum ppp_conn_type conn_type;               /* using usb or uart */

    ppp_pcb *pcb;                               /* ppp protocol control block */
    struct netif pppif;

    rt_size_t  rxpos;
    rt_uint8_t rxbuf[PPP_FRAME_MAX];
    rt_uint8_t state;

    struct rt_event event;

    rt_thread_t recv_tid;                       /* recieve thread point */
    void *user_data;                            /* reserve */
};

struct ppp_device_ops
{
    rt_err_t  (*init)   (struct ppp_device *dev);
    rt_err_t  (*open)   (struct ppp_device *dev, rt_uint16_t oflag);
    rt_err_t  (*close)  (struct ppp_device *dev);
    rt_err_t  (*control)(struct ppp_device *dev, int cmd, void *args);
};

enum ppp_reci_status
{
    PPP_DATA_VERIFY,
    PPP_DATA_START,
    PPP_DATA_END
};

/* store at_client rx_callback function */
typedef  rt_err_t (*uart_rx_cb)(rt_device_t dev, rt_size_t size);

/* offer register funciton to user */
int ppp_device_register(struct ppp_device *ppp_device, const char *dev_name, const char *uart_name, void *user_data);
int ppp_device_attach(struct ppp_device *ppp_device, char *uart_name, void *user_data);
int ppp_device_detach(struct ppp_device *ppp_device);

#endif /* __PPP_DEVICE_H__ */
