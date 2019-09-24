/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author          Notes
 * 2019-08-15     xiangxistu      the first version
 */

#include <ppp_device.h>
#include <ppp_netif.h>

#define DBG_TAG    "ppp.dev"

#ifdef PPP_DEVICE_DEBUG
#define DBG_LVL   DBG_LOG
#else
#define DBG_LVL   DBG_INFO
#endif

#include <rtdbg.h>

#define PPP_DEV_TABLE_LEN    4

#if RT_LWIP_TCPTHREAD_STACKSIZE < 2048
#error "tcpip stack is too small, should greater than 2048."
#endif


static struct ppp_device *_g_ppp_device = RT_NULL;

/*
 * Receive callback function , release rx_notice when uart acquire data
 *
 * @param rt_device_t
 *        rt_size_t
 *
 * @return  0: execute successful
 *
 */
static rt_err_t ppp_device_rx_ind(rt_device_t dev, rt_size_t size)
{
    RT_ASSERT(dev != RT_NULL);
    struct ppp_device *ppp_dev = _g_ppp_device;

    /* when recieve data from uart , release semphone to wake up recieve thread */
    rt_sem_release(ppp_dev->rx_notice);

    return RT_EOK;
}

/*
 *  using ppp_data_send send data to lwIP procotol stack     PPPoS serial output callback
 *
 * @param ppp_pcb   *pcb                 pcb PPP control block
 *        uint8_t   *data                data Buffer to write to serial port
 *        uint32_t  len                  len Length of the data buffer
 *        void      *ppp_device          ctx Context of callback , ppp_device
 *
 * @return  0: creat response fail
 *
 */
static uint32_t ppp_data_send(ppp_pcb *pcb, uint8_t *data, uint32_t len, void *ppp_device)
{
    RT_ASSERT(pcb != RT_NULL);
    RT_ASSERT(ppp_device != RT_NULL);

    int result = RT_EOK;
    struct ppp_device *device = (struct ppp_device *)ppp_device;
    rt_device_t recv_device = RT_NULL;

    RT_ASSERT(device != RT_NULL);

    /* recv_device is rt_device , find recv_device through device name */
    recv_device = rt_device_find(device->rely_name);
    if (recv_device == RT_NULL)
    {
        LOG_E("Can find device (%s), ppp send data execute failed.",device->rely_name);
        result = -RT_ERROR;
        goto __exit;
    }

    /* the return data is the actually written size on successful */
    len = rt_device_write(recv_device,0,data,len);

    /* must be return data length , or will get warning like "pppos_write[0]: output failed len=24" */
    return len;

__exit:

    return result;
}

/*
 * ppp_status_changed callback function
 *
 * @param   ppp_pcb *pcb            protocol caontrol block
 *          int     err_code
 *          void    *ctx            reserver
 *
 * @return  0: creat response fail
 *
 */
static void ppp_status_changed(ppp_pcb *pcb, int err_code, void *ctx)
{
    struct ppp_device *pppdev = (struct ppp_device *)ctx;
    struct netif *pppif = ppp_netif(pcb);
    static uint8_t re_count = 0;
    switch (err_code)
    {
    case PPPERR_NONE:                /* Connected */
        pppdev->pppif.mtu = pppif->mtu;
        if (pppdev->ppp_link_status == 1)
        {
            ppp_netdev_add(&pppdev->pppif);
        }
        re_count = 0;
        LOG_I("ppp connect successful.");
        break;
    case PPPERR_PARAM:
        LOG_E("Invalid parameter.");
        break;
    case PPPERR_OPEN:
        LOG_E("Unable to open PPP session.");
        break;
    case PPPERR_DEVICE:
        LOG_E("Invalid I/O device for PPP.");
        break;
    case PPPERR_ALLOC:
        LOG_E("Unable to allocate resources.");
        break;
    case PPPERR_USER:
        LOG_E("free the ppp control block , because user control.");
        pppapi_free(pcb);           /* Free the PPP control block */
        break;
    case PPPERR_CONNECT:            /* Connection lost */
        LOG_E("ppp connect lost.");
        pppapi_connect(pcb, 0);
        LOG_I("Reconnection.");
        break;
    case PPPERR_AUTHFAIL:
        LOG_E("Failed authentication challenge.");
        break;
    case PPPERR_PROTOCOL:
        LOG_E("Failed to meet protocol.");
        break;
    case PPPERR_PEERDEAD:
        LOG_E("Connection timeout.");
        pppapi_connect(pcb, 0);
        if(re_count++ == 2)
        {
            re_count = 0;
            pppdev->ppp_link_status = 0;
            pppdev->ops->close(pppdev);
        }
        else
        {
             LOG_I("Reconnection.");
        }
        break;
    case PPPERR_IDLETIMEOUT:
        LOG_E("Idle Timeout.");
        break;
    case PPPERR_CONNECTTIME:
        LOG_E("Max connect time reached.");
        break;
    case PPPERR_LOOPBACK:
        LOG_E("Loopback detected.");
        break;
    default:
        LOG_E("Unknown error code %d.", err_code);
        break;
    }
}

/*
 * Receive thread , store uart data and transform tcpip stack
 *
 * @param ppp_device *device
 *
 *
 * @return  0: execute successful
 *
 */
static int ppp_recv_entry(struct ppp_device *device)
{
    RT_ASSERT(device != RT_NULL);

    char ch = 0, old_ch = 0;
    int result = RT_EOK;
    static char thrans_flag = 0;
    rt_device_t recv_dev = RT_NULL;
    device->recv_line_len = 0;

    /* alloc buff to store uart data */
    device->recv_line_buf = (char *)rt_calloc(1, 1550);
    if (device->recv_line_buf == RT_NULL)
    {
        LOG_E("ppp_recv_line_buff alloc memory failed! No memory for receive buffer.");
        result = -RT_ENOMEM;
        goto __exit;
    }

    /* use name to find rt_devcie */
    recv_dev = rt_device_find(device->rely_name);
    if (recv_dev == RT_NULL)
    {
        LOG_E("Can find device (%s), ppp recv entry creat failed.",device->rely_name);
        result = -RT_ERROR;
        goto __exit;
    }

    while (1)
    {
        /* ppp link is closed, the recieve thread also need to exit */
        if(device->ppp_link_status != 1)
        {
            break;
        }
        /* when rx_notice semphone come , recieve data from uart */
        rt_sem_take(device->rx_notice, RT_WAITING_FOREVER);

        /* uart devcie , recieve data from uart and store data in the recv_buff */
        while (rt_device_read(recv_dev, 0, &ch, 1))
        {
            /* begin to recieve data from uart */
            if (thrans_flag == 1)
            {
                /* if recieve 0x7e twice */
                if (ch == 0x7e && old_ch == 0x7e)
                {
                    /* choice the least 0x7e as frame head */
                    device->recv_line_buf[0] = ch;
                    device->recv_line_len = 1;

                    old_ch = ch;
                }
                else if (ch == 0x7e && old_ch == 0x00)
                {
                    thrans_flag = 2;
                    device->recv_line_buf[device->recv_line_len] = ch;
                }
                else
                {
                    old_ch = 0x00;
                    device->recv_line_buf[device->recv_line_len] = ch;
                    device->recv_line_len++;
                }

                /* when a frame is end, put data into tcpip */
                if (thrans_flag == 2)
                {
                    rt_enter_critical();
                    pppos_input_tcpip(device->pcb, (u8_t *)device->recv_line_buf, device->recv_line_len + 1);
                    rt_exit_critical();

                    thrans_flag = 0;
                    device->recv_line_len = 0;
                }
            }
            else
            {
                /* if recieve 0x7e, begin to recieve data */
                if (ch == 0x7e)
                {
                    thrans_flag = 1;
                    old_ch = ch;
                    device->recv_line_buf[0] = ch;
                    device->recv_line_len = 1;
                }
            }
        }
    }

__exit:

    rt_free(device->recv_line_buf);

    return result;
}

/*
 * Creat a thread to creat receive thread function
 *
 * @param ppp_device *device
 *
 *
 * @return  0: execute successful
 *
 */
static int ppp_recv_entry_creat(struct ppp_device *device)
{
    rt_int8_t result = RT_EOK;

    /* protect public resoure */
    device->lock = rt_mutex_create("mutex_ppp_recv", RT_IPC_FLAG_FIFO);
    if (device->lock == RT_NULL)
    {
        LOG_E("PPP device initialize failed! ppp_device_recv_lock create failed!");
        result = -RT_ENOMEM;
        goto __exit;
    }

    /* when recieve rx_notice, come to recieve a data from uart */
    device->rx_notice = rt_sem_create("sem_ppp_recv", 0, RT_IPC_FLAG_FIFO);
    if (device->rx_notice == RT_NULL)
    {
        LOG_E("PPP device initialize failed! ppp_device_recv_notice create failed!");
        result = -RT_ENOMEM;
        goto __exit;
    }

    /* dynamic creat a recv_thread */
    device->recv_tid = rt_thread_create("ppp_recv",
                                        (void (*)(void *parameter))ppp_recv_entry,
                                        device,
                                        2 * 1024,
                                        8,
                                        20);
    if (device->recv_tid == RT_NULL)
    {
        LOG_E("PPP device initialize failed! ppp_device_recv_tid create failed!");
        result = -RT_ENOMEM;
        goto __exit;
    }

    /* if you create a thread, never forget to start it */
    rt_thread_startup(device->recv_tid);

__exit:
    if (result != RT_EOK)
    {
        if (device->lock)
        {
            rt_mutex_delete(device->lock);
        }

        if (device->rx_notice)
        {
            rt_sem_delete(device->rx_notice);
        }

        rt_memset(device, 0x00, sizeof(struct ppp_device));
    }
    return result;
}

/*
 * ppp device init function,set ops funciton and base config
 *
 * @param rt_device_t *device
 *
 *
 * @return  0: execute successful
 *
 */
static rt_err_t ppp_device_init(struct rt_device *device)
{
    int result = RT_EOK;
    RT_ASSERT(device != RT_NULL);

    struct ppp_device *ppp_device = (struct ppp_device *)device;
    RT_ASSERT(ppp_device != RT_NULL);

    result = ppp_device->ops->init(ppp_device);
    if (result != RT_EOK)
    {
        LOG_E("ppp_device_init failed(%s).", ppp_device->rely_name);
        goto __exit;
    }

__exit:

    return result;
}

/*
 * initialize ppp device and set callback function
 *
 * @param rt_device_t *device
 *        rt_uint16_t oflag
 *
 * @return  0:  execute successful
 *
 */
static rt_err_t ppp_device_open(struct rt_device *device, rt_uint16_t oflag)
{
    int result = RT_EOK;
    RT_ASSERT(device != RT_NULL);

    struct ppp_device *ppp_device = (struct ppp_device *)device;
    RT_ASSERT(ppp_device != RT_NULL);
    static rt_device_t serial;

    ppp_device->ppp_link_status = 1;
    /* Creat a thread to creat ppp recieve function */
    result = ppp_recv_entry_creat(ppp_device);
    if (result != RT_EOK)
    {
        LOG_E("Creat a thread to creat ppp recieve function failed.");
        result = -RT_ERROR;
        goto __exit;
    }
    LOG_D("Creat a thread to creat ppp recieve function successful.");

    /* we can do nothing */
    result = ppp_device->ops->open(ppp_device, oflag);
    if (result != RT_EOK)
    {
        LOG_E("ppp device open failed.");
        result = -RT_ERROR;
        goto __exit;
    }

    /* uart conversion into ppp device , find and open command device */
    serial = rt_device_find(ppp_device->rely_name);
    if (serial  != RT_NULL)
    {
        RT_ASSERT(serial->type == RT_Device_Class_Char);

        /* uart transfer into tcpip protocol stack */
        rt_device_set_rx_indicate(serial, ppp_device_rx_ind);
        LOG_I("(%s) is used by ppp_device.", ppp_device->rely_name);
    }
    else
    {
        LOG_E("Cannot find %s device.", ppp_device->rely_name);
        result = -RT_ERROR;
        goto __exit;
    }

    /* creat pppos */
    ppp_device->pcb = pppapi_pppos_create(&(ppp_device->pppif), ppp_data_send, ppp_status_changed, ppp_device);
    if (ppp_device->pcb == RT_NULL)
    {
        LOG_E("Create ppp pcb failed.");
        result = -RT_ERROR;
        goto __exit;
    }
    LOG_D("pppapi_pppos_create has created a protocol control block.");

    /* set netif name */
    ppp_device->pppif.name[0] = ppp_device->parent.parent.name[0];
    ppp_device->pppif.name[1] = ppp_device->parent.parent.name[1];

    /* set default route */
    result = pppapi_set_default(ppp_device->pcb);
    if (result != RT_EOK)
    {
        LOG_E("pppapi_set_default execute failed.");
        result = -RT_ERROR;
        goto __exit;
    }
    LOG_D("pppapi_set_default has set a default route.");

    /* dns */
    ppp_set_usepeerdns(ppp_device->pcb, 1);
    LOG_D("ppp_set_usepeerdns has set a dns number.");

#ifdef USING_PPP_AUTHORIZE
    /* set authorize */
 #if PAP_SUPPORT
     ppp_set_auth(ppp_device->pcb , PPPAUTHTYPE_PAP, ppp_device->config.user_name, ppp_device->config.user_name);
 #elif CHAP_SUPPORT
     ppp_set_auth(ppp_device->pcb, PPPAUTHTYPE_CHAP, ppp_device->config.user_name, ppp_device->config.user_name);
 #else
 #error "Unsupported AUTH Negotiation"
 #endif
    LOG_D("ppp_set_auth has passed verification.");
 #endif /* PPP_AUTHORIZE */

    /* ppp connect */
    result = pppapi_connect(ppp_device->pcb, 0);
    if (result != RT_EOK)
    {
        LOG_E("pppapi_connect execute failed.");
        result = -RT_ERROR;
        goto __exit;
    }
    LOG_D("pppapi_connect execute successful, waitting connect.");

__exit:

    return result;
}

/*
 * Close ppp device
 *
 * @param rt_device_t   *device
 *
 *
 * @return  0: execute successful
 */
static rt_err_t ppp_device_close(struct rt_device *device)
{
	extern void ppp_netdev_del(struct netif *ppp_netif);
    RT_ASSERT(device != RT_NULL);

    struct ppp_device *ppp_device = (struct ppp_device *)device;
    RT_ASSERT(ppp_device != RT_NULL);

    /* use pppapi_close to shutdown ppp link status */
    pppapi_close(ppp_device->pcb, 0);

    ppp_device->ppp_link_status = 0;
    rt_sem_release(ppp_device->rx_notice);

    /* delete netdev from netdev frame */
    ppp_netdev_del(&ppp_device->pppif);
    LOG_D("ppp netdev has been detach.");

    /* cut down piont to piont at data link layer */
    return ppp_device->ops->close(ppp_device);
}

/*
 * Control ppp device , access ppp mode or accsee AT mode
 *
 * @param rt_device_t   *device
 *        int           cmd
 *        void          *args
 *
 * @return  0: execute successful
 */
static rt_err_t ppp_device_control(struct rt_device *device,int cmd, void *args)
{
    RT_ASSERT(device != RT_NULL);

    struct ppp_device *ppp_device = (struct ppp_device *)device;
    RT_ASSERT(ppp_device != RT_NULL);

    /* use ppp_device_control function */
    return ppp_device->ops->control(ppp_device,cmd,args);
}

/*
 * ppp device ops
 *
 */
#ifdef RT_USING_DEVICE_OPS
const struct rt_device_ops ppp_device_ops =
{
    ppp_device_init,
    ppp_device_open,
    ppp_device_close,
    ppp_device_control
};
#endif

/*
 * Register ppp_device into rt_device frame,set ops function to rt_device inferface
 *
 * @param struct ppp_device *ppp_device
 *
 * @return  0: execute successful
 *
 */
int ppp_device_register(struct ppp_device *ppp_device, const char *dev_name, const char *rely_name, void *user_data)
{
    rt_err_t result = RT_EOK;

    RT_ASSERT(ppp_device != RT_NULL);

    struct rt_device *device = RT_NULL;
    device = &(ppp_device->parent);

#ifdef RT_USING_DEVICE_OPS
    device->ops = &ppp_device_ops;
#else
    device->init = ppp_device_init;
    device->open = ppp_device_open;
    device->close = ppp_device_close;
    device->read = RT_NULL;
    device->write = RT_NULL;
    device->control = ppp_device_control;
#endif
    device->user_data = user_data;

    rt_strncpy((char *)ppp_device->rely_name, rely_name, rt_strlen(rely_name));

    /* now we supprot only one device */
    if (_g_ppp_device != RT_NULL)
    {
        LOG_E("Only one device support.");
        RT_ASSERT(_g_ppp_device == RT_NULL);
    }

    /* attention: you can't use ppp_device as a server in you network, unless
     sim of the modem module used supprots getting public IP address. */
    /* register ppp device into rt_device frame */
    result = rt_device_register(&ppp_device->parent, dev_name, RT_Device_Class_NetIf);
    if( result == RT_EOK)
    {
        _g_ppp_device = ppp_device;
        LOG_D("ppp_device has registered rt_device frame successful.");
    }

    /* when ppp device has register rt_device frame, start up it */
    do
    {
        result = device->init((rt_device_t)ppp_device);
        if (result != RT_EOK)
        {
            LOG_E("ppp device init failed.try it in %ds", 2500/100);
            rt_thread_mdelay(2500);
            result = -RT_ERROR;
        }
    } while (result != RT_EOK);

    return result;
}
