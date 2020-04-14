/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author          Notes
 * 2019-08-15     xiangxistu      the first version
 * 2019-10-01     xiaofan         rewrite ppp_recv thread
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

enum
{
    PPP_STATE_PREPARE,
    PPP_STATE_WAIT_HEAD,
    PPP_STATE_RECV_DATA,
};

#define PPP_DATA_BEGIN_END       0x7e
#define DATA_EFFECTIVE_FLAG      0x00
#define RECON_ERR_COUNTS         0x02
#define PPP_RECONNECT_TIME       2500
#define PPP_RECV_READ_MAX        32

#define PPP_RECEIVE_THREAD_STACK_SIZE 768
#define PPP_RECEIVE_THREAD_PRIORITY   7


#define PPP_EVENT_RX_NOTIFY 1   // serial incoming a byte
#define PPP_EVENT_LOST      2   // PPP connection is lost
#define PPP_EVENT_CLOSE_REQ 4   // user want close ppp_device
#define PPP_EVENT_CLOSED    8   // ppp_recv thread will send CLOSED event when ppp_recv thread is safe exit

#if RT_LWIP_TCPTHREAD_STACKSIZE < 2048
#error "tcpip stack is too small, should greater than 2048."
#endif

static struct ppp_device *_g_ppp_device = RT_NULL;

/**
 * dump ppp data according to hex format,you can see data what you recieve , send, and ppp_device dorp out
 *
 * @param data      the data what you want to dump out
 * @param len       the length of those data
 */
#ifdef PPP_DEVICE_DEBUG
static void ppp_debug_hexdump(const void *data, size_t len)
{
    rt_uint32_t offset = 0;
    size_t curlen = 0, i = 0;
    char line[16 * 4 + 3] = {0};
    char *p = RT_NULL;
    const unsigned char *src = data;

    while (len > 0)
    {
        curlen = len < maxlen ? len : maxlen;
        p = line;
        for (i = 0; i < curlen; i++)
        {
            rt_sprintf(p, "%02x ", (unsigned char)src[i]);
            p += 3;
        }
        memset(p, ' ', (maxlen - curlen) * 3);
        p += (maxlen - curlen) * 3;
        *p++ = '|';
        *p++ = ' ';
        for (i = 0; i < curlen; i++)
        {
            *p++ = (0x20 < src[i] && src[i] < 0x7e) ? src[i] : '.';
        }
        *p++ = '\0';
        LOG_D("[%04x] %s", offset, line);
        len -= curlen;
        src += curlen;
        offset += curlen;
    }
}
#endif

/**
 * Receive callback function , send PPP_EVENT_RX_NOTIFY event when uart acquire data
 *
 * @param dev       the point of device driver structure, uart structure
 * @param size      the indication callback function need this parameter
 *
 * @return  RT_EOK
 */
static rt_err_t ppp_device_rx_ind(rt_device_t dev, rt_size_t size)
{
    RT_ASSERT(dev != RT_NULL);
    struct ppp_device *ppp_dev = _g_ppp_device;

    /* when recieve data from uart , send event to wake up recieve thread */
    rt_event_send(&ppp_dev->event, PPP_EVENT_RX_NOTIFY);

    return RT_EOK;
}

/**
 *  using ppp_data_send send data to lwIP procotol stack     PPPoS serial output callback
 *
 * @param pcb           pcb PPP control block
 * @param data          data Buffer to write to serial port
 * @param len           the Length of the data buffer
 * @param ppp_device    ctx Context of callback , ppp_device
 *
 * @return  the point of rt_device_write fucntion or RT_NULL
 */
static uint32_t ppp_data_send(ppp_pcb *pcb, uint8_t *data, uint32_t len, void *ppp_device)
{
    RT_ASSERT(pcb != RT_NULL);
    RT_ASSERT(ppp_device != RT_NULL);

    struct ppp_device *device = (struct ppp_device *)ppp_device;

    if (device->state == PPP_STATE_PREPARE)
        return 0;

#ifdef PPP_DEVICE_DEBUG_TX
    LOG_D("TX:");
    ppp_debug_hexdump(data, len);
#endif
    /* the return data is the actually written size on successful */
    return rt_device_write(device->uart, 0, data, len);
}

/**
 * ppp_status_changed callback function
 *
 * @param pcb           protocol caontrol block
 * @param err_code      the result of ppp conncet
 * @param ctx           ctx Context of callback , ppp_device
 */
static void ppp_status_changed(ppp_pcb *pcb, int err_code, void *ctx)
{
    struct ppp_device *pppdev = (struct ppp_device *)ctx;
    struct netif *pppif = ppp_netif(pcb);
    switch (err_code)
    {
    case PPPERR_NONE: /* Connected */
        pppdev->pppif.mtu = pppif->mtu;
        ppp_netdev_refresh(&pppdev->pppif);
        LOG_I("ppp_device connect successfully.");
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
    case PPPERR_CONNECT: /* Connection lost */
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

/**
 * prepare for starting recieve ppp frame, clear recieve buff and set ppp device state
 *
 * @param device  the point of device driver structure, ppp_device structure
 */
static inline void ppp_start_receive_frame(struct ppp_device *device)
{
    device->rxpos = 0;
    device->state = PPP_STATE_WAIT_HEAD;
}

#ifdef PPP_DEVICE_DEBUG_DROP
/**
 * ppp_show_dropbuf, printf the data whom ppp devcie drop out, and increase drop data conut
 *
 * @param device  the point of device driver structure, ppp_device structure
 */
static inline void ppp_show_dropbuf(struct ppp_device *device)
{
    if (!device->droppos)
        return;
    LOG_D("DROP: ");
    ppp_debug_hexdump(device->dropbuf, device->droppos);
    device->dropcnt += device->droppos;
    device->droppos = 0;
}

/**
 * ppp_show_rxbuf_as_drop, printf the data whom ppp devcie drop out, and increase drop data conut
 *
 * @param device  the point of device driver structure, ppp_device structure
 */
static inline void ppp_show_rxbuf_as_drop(struct ppp_device *device)
{
    if (!device->rxpos)
        return;
    LOG_D("DROP: ");
    ppp_debug_hexdump(device->rxbuf, device->rxpos);
    device->dropcnt += device->rxpos;
    device->rxpos = 0;
}

/**
 * ppp_rxbuf_drop, printf the data whom ppp devcie drop out
 *
 * @param device  the point of device driver structure, ppp_device structure
 */
static inline void ppp_rxbuf_drop(struct ppp_device *device)
{
    /* if we have no enough drop-buffer, we should display and clear drop-buffer */
    if (PPP_DROP_BUF - device->droppos < device->rxpos)
    {
        ppp_show_dropbuf(device);
        /* if our drop-buffer size less than or equal current valid size of rx-buffer,
            we should display and clear rx-buffer */
        if (PPP_DROP_BUF <= device->rxpos)
        {
            ppp_show_rxbuf_as_drop(device);
        }
    }

    if (device->rxpos)
    {
        rt_memcpy(&device->dropbuf[device->droppos], device->rxbuf, device->rxpos);
        device->droppos += device->rxpos;
        device->rxpos = 0;

        if (device->droppos == PPP_DROP_BUF)
        {
            ppp_show_dropbuf(device);
        }
    }

    ppp_start_receive_frame(device);
}
#else
static inline void ppp_show_dropbuf(struct ppp_device *device) {}
#define ppp_rxbuf_drop(device) ppp_start_receive_frame(device)
#endif /* PPP_DEVICE_DEBUG_DROP */

/**
 * ppp_processdata_enter, prepare to recieve data
 *
 * @param device  the point of device driver structure, ppp_device structure
 */
static inline void ppp_processdata_enter(struct ppp_device *device)
{
    ppp_start_receive_frame(device);
#ifdef PPP_DEVICE_DEBUG_DROP
    device->droppos = 0;
#endif
}

/**
 * ppp_processdata_leave, throw ppp device data when ppp connection is closed
 *
 * @param device  the point of device driver structure, ppp_device structure
 */
static inline void ppp_processdata_leave(struct ppp_device *device)
{
    ppp_rxbuf_drop(device);
    ppp_show_dropbuf(device);
}

/**
 * ppp_savebyte, save this data into ppp device buff
 *
 * @param   device  the point of device driver structure, ppp_device structure
 * @param   dat     the character of recieve data
 */
static inline void ppp_savebyte(struct ppp_device *device, rt_uint8_t dat)
{
    RT_ASSERT(device->rxpos < sizeof(device->rxbuf));
    device->rxbuf[device->rxpos++] = dat;
}

/**
 * ppp_recv_processdata, save data from uart, recieve complete ppp frame data
 *
 * @param   device  the point of device driver structure, ppp_device structure
 * @param   buf     the address of recieve data from uart
 * @param   len     the length of recieve data
 */
static void ppp_recv_processdata(struct ppp_device *device, const rt_uint8_t *buf, rt_size_t len)
{
    rt_uint8_t dat;

    while (len--)
    {
        dat = *buf++;

process_dat:
        switch (device->state)
        {
        case PPP_STATE_WAIT_HEAD:
            ppp_savebyte(device, dat);
            if (dat == PPP_DATA_BEGIN_END)              /* if recieve 0x7e */
            {
                ppp_show_dropbuf(device);
                device->state = PPP_STATE_RECV_DATA;    /* begin recieve the second data */
            }
            else
            {
                ppp_rxbuf_drop(device);
            }
            break;
        case PPP_STATE_RECV_DATA:
            if (dat == PPP_DATA_BEGIN_END && device->rxpos == 1)    /* if we recieve 0x7e when this data is the second data */
            {
                LOG_D("found continuous 0x7e");
                // start receive a new frame
                ppp_rxbuf_drop(device);                 /* throw this data, because 0x7e is the begin of ppp frame, the second data shouldn't been 0x7e */
                goto process_dat;
            }
            ppp_savebyte(device, dat);
            if (dat == PPP_DATA_BEGIN_END)      /* the end of ppp frame */
            {
#ifdef PPP_DEVICE_DEBUG_RX
                LOG_D("RX:");
                ppp_debug_hexdump(device->rxbuf, device->rxpos);
#endif
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
            ppp_rxbuf_drop(device);
        }
    }
}

/**
 * Receive thread , store uart data and transform tcpip stack
 *
 * @param device    the point of device driver structure, ppp_device structure
 *
 * @return  RT_EOK  we shouldn't let the recieve thread return data, recieve thread need keepalive all the time
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
                ppp_processdata_enter(device);
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
            ppp_processdata_leave(device);
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

/**
 * Creat a thread to creat receive thread function
 *
 * @param   device      the point of device driver structure, ppp_device structure
 *
 * @return  RT_EOK      recieve thread create and startup successfully
 *          -RT_ERROR   create recieve thread successfully
 *          -RT_ENOMEM  startup recieve thread successfully
 */
static int ppp_recv_entry_creat(struct ppp_device *device)
{
    rt_int8_t result = RT_EOK;

    /* dynamic creat a recv_thread */
    device->recv_tid = rt_thread_create("ppp_recv",
                                        (void (*)(void *parameter))ppp_recv_entry,
                                        device,
                                        PPP_RECEIVE_THREAD_STACK_SIZE,
                                        PPP_RECEIVE_THREAD_PRIORITY,
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

/**
 * ppp device init function,set ops funciton and base config
 *
 * @param device    the point of device driver structure, rt_device structure
 *
 * @return  RT_EOK
 */
static rt_err_t ppp_device_init(struct rt_device *device)
{
    RT_ASSERT(device != RT_NULL);

    struct ppp_device *ppp_device = (struct ppp_device *)device;
    RT_ASSERT(ppp_device != RT_NULL);
    device->flag |= RT_DEVICE_FLAG_STANDALONE;
#ifdef PPP_DEVICE_DEBUG_DROP
    ppp_device->dropcnt = 0;
    ppp_device->droppos = 0;
#endif
    return RT_EOK;
}

/**
 * initialize ppp device and set callback function
 *
 * @param device    the point of device driver structure, rt_device structure
 * @param oflag     the open flag of rt_device
 *
 * @return  the result
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

    if (rt_device_open(ppp_device->uart, uart_oflag) != RT_EOK)
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
    LOG_D("(%s) is used by ppp_device.", ppp_device->uart->parent.name);


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

/**
 * Close ppp device
 *
 * @param device    the point of device driver structure, rt_device structure
 *
 * @return  RT_EOK
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

/**
 * Control ppp device , access ppp mode or accsee AT mode; but not it is useless
 *
 * @param device    the point of device driver structure, rt_device structure
 * @param cmd       the command of device
 * @param args      the private data of you send
 *
 * @return  -RT_ENOSYS
 */
static rt_err_t ppp_device_control(struct rt_device *device,int cmd, void *args)
{
    RT_ASSERT(device != RT_NULL);

    return -RT_ENOSYS;
}

/* ppp device ops */
#ifdef RT_USING_DEVICE_OPS
const struct rt_device_ops ppp_device_ops =
{
    ppp_device_init,
    ppp_device_open,
    ppp_device_close,
    ppp_device_control
};
#endif

/**
 * Register ppp_device into rt_device frame,set ops function to rt_device inferface
 *
 * @param ppp_device    the point of device driver structure, ppp_device structure
 * @param dev_name      the name of ppp_device name
 * @param uart_name     the name of uart name what you used
 * @param user_data     private data
 *
 * @return  RT_EOK      ppp_device registered into rt_device frame successfully
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
        LOG_I("ppp_device(%s) register successfully.", PPP_DEVICE_NAME);
    }

    return result;
}

/**
 * attach data interface device into ppp device frame
 *
 * @param   ppp_device      the point of device driver structure, ppp_device structure
 * @param   uart_name       the name of uart name what you used
 * @param   user_data       private data
 *
 * @return  RT_EOK          execute successful
 * @return -RT_ERROR        error
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

    if (rt_device_open(&ppp_device->parent, 0) != RT_EOK)
    {
        LOG_E("ppp_device_attach failed. Can't open device(%d)");
        return -RT_ERROR;
    }

    return RT_EOK;
}

/**
 * detach data interface device from ppp device frame
 *
 * @param   ppp_device      the point of device driver structure, ppp_device structure
 *
 * @return  RT_EOK          execute successful
 * @return -RT_ERROR        error
 */
int ppp_device_detach(struct ppp_device *ppp_device)
{
    RT_ASSERT(ppp_device != RT_NULL);

    if (rt_device_close(&ppp_device->parent) != RT_EOK)
    {
        LOG_E("ppp_device_detach failed. Can't open device.");
        return -RT_ERROR;
    }

    ppp_device->uart = RT_NULL;
    ppp_device->user_data = RT_NULL;

    return RT_EOK;
}
