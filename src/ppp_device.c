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

enum {
    PPP_STATE_PREPARE,
    PPP_STATE_WAIT_HEAD,
    PPP_STATE_WAIT_ADDR,
    PPP_STATE_RECV_DATA,
};

#define PPP_DATA_BEGIN_END       0x7e
#define PPP_DATA_ADDR            0xff
#define DATA_EFFECTIVE_FLAG      0x00
#define RECON_ERR_COUNTS         0x02
#define PPP_RECONNECT_TIME       2500
#define PPP_RECV_READ_MAX        32


#define PPP_EVENT_RX_NOTIFY 1   // serial incoming a byte
#define PPP_EVENT_LOST      2   // PPP connection is lost
#define PPP_EVENT_CLOSE_REQ 4   // user want close ppp_device
#define PPP_EVENT_CLOSED    8   // ppp_recv thread will send CLOSED event when ppp_recv thread is safe exit

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
    rt_event_send(&ppp_dev->event, PPP_EVENT_RX_NOTIFY);

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

    struct ppp_device *device = (struct ppp_device *)ppp_device;

    if (device->state == PPP_STATE_PREPARE)
        return 0;

    /* the return data is the actually written size on successful */
    return rt_device_write(device->uart, 0, data, len);
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
    switch (err_code)
    {
    case PPPERR_NONE:                /* Connected */
        pppdev->pppif.mtu = pppif->mtu;
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
        LOG_D("User interrupt");
        break;
    case PPPERR_CONNECT:            /* Connection lost */
        LOG_E("ppp connect lost.");
        break;
    case PPPERR_AUTHFAIL:
        LOG_E("Failed authentication challenge.");
        break;
    case PPPERR_PROTOCOL:
        LOG_E("Failed to meet protocol.");
        break;
    case PPPERR_PEERDEAD:
        LOG_E("Connection timeout.");
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
    if (err_code != PPPERR_NONE)
        rt_event_send(&pppdev->event, PPP_EVENT_LOST);
}

static inline void ppp_savebyte(struct ppp_device *device, rt_uint8_t dat)
{
    RT_ASSERT(device->rxpos < sizeof(device->rxbuf));
    device->rxbuf[device->rxpos++] = dat;
}

static inline void ppp_start_receive_frame(struct ppp_device *device)
{
    device->rxpos = 0;
    device->state = PPP_STATE_WAIT_HEAD;
}

static void ppp_recv_processdata(struct ppp_device *device, const rt_uint8_t *buf, rt_size_t len)
{
    rt_uint8_t dat;

    while (len--)
    {
        dat = *buf++;
process_dat:
        ppp_savebyte(device, dat);
        switch (device->state)
        {
        case PPP_STATE_WAIT_HEAD:
            if (dat == PPP_DATA_BEGIN_END)
            {
                device->state = PPP_STATE_WAIT_ADDR;
            }
            else
            {
                ppp_start_receive_frame(device);
            }
            break;
        case PPP_STATE_WAIT_ADDR:
            if (dat == PPP_DATA_ADDR)
            {
                device->state = PPP_STATE_RECV_DATA;
            }
            else
            {
                ppp_start_receive_frame(device);
                goto process_dat;
            }

            break;
        case PPP_STATE_RECV_DATA:
            if (dat == PPP_DATA_BEGIN_END)
            {
                rt_enter_critical();
                pppos_input_tcpip(device->pcb, (u8_t *)device->rxbuf, device->rxpos);
                rt_exit_critical();
                ppp_start_receive_frame(device);
            }
            break;
        default:
            LOG_E("BUG: unexpect state: %u", (unsigned int)device->state);
        }
        if (device->rxpos == sizeof(device->rxbuf))
        {
            LOG_W("receive ppp frame is lagger than %u", (unsigned int)sizeof(device->rxbuf));
            ppp_start_receive_frame(device);
        }
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
    const rt_uint32_t interested_event = PPP_EVENT_RX_NOTIFY | PPP_EVENT_LOST | PPP_EVENT_CLOSE_REQ;
    rt_uint32_t event;
    rt_size_t len;
    rt_uint8_t buffer[PPP_RECV_READ_MAX];
    rt_bool_t closing = RT_FALSE;

    rt_event_control(&device->event, RT_IPC_CMD_RESET, RT_NULL);
    device->state = PPP_STATE_PREPARE;

    while (1)
    {
        if (device->state == PPP_STATE_PREPARE)
        {
            if (!device->ops->prepare || device->ops->prepare(device) == RT_EOK)
            {
                ppp_start_receive_frame(device);
                pppapi_connect(device->pcb, 0);
            }
            else
            {
                LOG_E("%s prepare fail, try again later", device->parent.parent.name);
                rt_thread_mdelay(10*1000);
            }
            continue;
        }

        rt_event_recv(&device->event, interested_event, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, &event);

        if (event & PPP_EVENT_RX_NOTIFY)
        {
            do
            {
                len = rt_device_read(device->uart, 0, buffer, PPP_RECV_READ_MAX);
                if (len)
                    ppp_recv_processdata(device, buffer, len);
            } while (len);
        }

        if (event & PPP_EVENT_CLOSE_REQ)
        {
            LOG_D("user request close ppp");
            closing = RT_TRUE;
            pppapi_close(device->pcb, 0);
        }

        if (event & PPP_EVENT_LOST)
        {
            if (closing)
            {
                LOG_W("ppp closing success");
                rt_event_send(&device->event, PPP_EVENT_CLOSED);
                break;
            }
            LOG_W("ppp lost, try reconnect");
            device->state = PPP_STATE_PREPARE;
        }
    }

    return RT_EOK;
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

    /* dynamic creat a recv_thread */
    device->recv_tid = rt_thread_create("ppp_recv",
                                        (void (*)(void *parameter))ppp_recv_entry,
                                        device,
                                        768,
                                        8,
                                        20);
    if (device->recv_tid == RT_NULL)
    {
        LOG_E("PPP device initialize failed! ppp_device_recv_tid create failed!");
        result = -RT_ENOMEM;
        goto __exit;
    }

    /* if you create a thread, never forget to start it */
    result = rt_thread_startup(device->recv_tid);
    if(result != RT_EOK)
        goto __exit;

    return result;

__exit:
    rt_memset(device, 0x00, sizeof(struct ppp_device));
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
    RT_ASSERT(device != RT_NULL);

    struct ppp_device *ppp_device = (struct ppp_device *)device;
    RT_ASSERT(ppp_device != RT_NULL);

    return RT_EOK;
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
    rt_uint16_t uart_oflag;
    RT_ASSERT(device != RT_NULL);

    struct ppp_device *ppp_device = (struct ppp_device *)device;

    uart_oflag = RT_DEVICE_OFLAG_RDWR;
    if (ppp_device->uart->flag & RT_DEVICE_FLAG_DMA_RX)
        uart_oflag |= RT_DEVICE_FLAG_DMA_RX;
    else if (ppp_device->uart->flag & RT_DEVICE_FLAG_INT_RX)
        uart_oflag = RT_DEVICE_FLAG_INT_RX;
    if (ppp_device->uart->flag & RT_DEVICE_FLAG_DMA_TX)
        uart_oflag |= RT_DEVICE_FLAG_DMA_TX;
    else if (ppp_device->uart->flag & RT_DEVICE_FLAG_INT_TX)
        uart_oflag |= RT_DEVICE_FLAG_INT_TX;

    if (rt_device_open(ppp_device->uart, uart_oflag))
    {
        LOG_E("ppp device open failed.");
        result = -RT_ERROR;
        goto __exit;
    }

    rt_event_init(&ppp_device->event, "pppev", RT_IPC_FLAG_FIFO);
    /* we can do nothing */

    RT_ASSERT(ppp_device->uart && ppp_device->uart->type == RT_Device_Class_Char);

    /* uart transfer into tcpip protocol stack */
    rt_device_set_rx_indicate(ppp_device->uart, ppp_device_rx_ind);
    LOG_I("(%s) is used by ppp_device.", ppp_device->uart->parent.name);


    /* creat pppos */
    ppp_device->pcb = pppapi_pppos_create(&(ppp_device->pppif), ppp_data_send, ppp_status_changed, ppp_device);
    if (ppp_device->pcb == RT_NULL)
    {
        LOG_E("Create ppp pcb failed.");
        result = -RT_ERROR;
        goto __exit;
    }
    LOG_D("pppapi_pppos_create has created a protocol control block.");
    ppp_netdev_add(&ppp_device->pppif);

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

    /* Creat a thread to creat ppp recieve function */
    result = ppp_recv_entry_creat(ppp_device);
    if (result != RT_EOK)
    {
        LOG_E("Creat a thread to creat ppp recieve function failed.");
        result = -RT_ERROR;
        goto __exit;
    }
    LOG_D("Creat a thread to creat ppp recieve function successful.");

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
    rt_uint32_t event;
    RT_ASSERT(device != RT_NULL);
	extern void ppp_netdev_del(struct netif *ppp_netif);

    struct ppp_device *ppp_device = (struct ppp_device *)device;
    RT_ASSERT(ppp_device != RT_NULL);

    rt_event_send(&ppp_device->event, PPP_EVENT_CLOSE_REQ);
    rt_event_recv(&ppp_device->event, PPP_EVENT_CLOSED, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, &event);

    rt_device_set_rx_indicate(ppp_device->uart, RT_NULL);
    ppp_netdev_del(&ppp_device->pppif);
    ppp_free(ppp_device->pcb);
    LOG_D("ppp netdev has been detach.");
    rt_event_detach(&ppp_device->event);

    rt_device_close(ppp_device->uart);
    /* cut down piont to piont at data link layer */
    LOG_I("ppp_device has been closed.");
    return RT_EOK;
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

    return -RT_ENOSYS;
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
int ppp_device_register(struct ppp_device *ppp_device, const char *dev_name, const char *uart_name, void *user_data)
{
    RT_ASSERT(ppp_device != RT_NULL);
    rt_err_t result = RT_EOK;
    struct rt_device *device = RT_NULL;

    device = &(ppp_device->parent);
    device->type = RT_Device_Class_NetIf;

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

    /* now we supprot only one device */
    if (_g_ppp_device != RT_NULL)
    {
        LOG_E("Only one device support.");
        RT_ASSERT(_g_ppp_device == RT_NULL);
    }

    /* attention: you can't use ppp_device as a server in you network, unless
     sim of the modem module used supprots getting public IP address. */

    /* register ppp device into rt_device frame */
    result = rt_device_register(&ppp_device->parent, dev_name, RT_DEVICE_OFLAG_RDWR);
    if( result == RT_EOK)
    {
        _g_ppp_device = ppp_device;
        LOG_I("ppp_device has registered rt_device frame successful.");
    }

    return result;
}

/*
 * attach data interface device into ppp device frame
 *
 * @param       struct ppp_device *ppp_device
 *              char *uart_name
 *              void *user_data
 * @return  0: execute successful
 *         -1: error
 *
 */
int ppp_device_attach(struct ppp_device *ppp_device, const char *uart_name, void *user_data)
{
    RT_ASSERT(ppp_device != RT_NULL);

    ppp_device->uart = rt_device_find(uart_name);
    if (!ppp_device->uart)
    {
        LOG_E("ppp_device_attach, cannot found %s", uart_name);
        return -RT_ERROR;
    }
    ppp_device->user_data = user_data;

    if (ppp_device->parent.open && ppp_device->parent.open((rt_device_t)ppp_device, RT_TRUE) != RT_EOK)
    {
        LOG_E("ppp_device_attach failed. Can't open device(%d)");
        return -RT_ERROR;
    }

    return RT_EOK;
}

/*
 * detach data interface device from ppp device frame
 *
 * @param       struct ppp_device *ppp_device
 *
 * @return  0: execute successful
 *         -1: error
 *
 */
int ppp_device_detach(struct ppp_device *ppp_device)
{
    RT_ASSERT(ppp_device != RT_NULL);

    if (ppp_device->parent.close && ppp_device->parent.close((rt_device_t)ppp_device) != RT_EOK)
    {
        LOG_E("ppp_device_detach failed. Can't open device.");
        return -RT_ERROR;
    }

    ppp_device->uart = RT_NULL;
    ppp_device->user_data = RT_NULL;

    return RT_EOK;
}
