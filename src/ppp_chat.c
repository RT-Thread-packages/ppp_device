/*
 * Copyright (c) 2019 xiaofan <xfan1024@live.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author          Notes
 * 2019-09-19     xiaofan         the first version
 */

#include "ppp_chat.h"
#define DBG_TAG    "ppp.chat"

#ifdef PPP_DEVICE_DEBUG
#define DBG_LVL   DBG_LOG
#else
#define DBG_LVL   DBG_INFO
#endif

#include <rtdbg.h>

#define CHAT_READ_BUF_MAX 16

// In order to match response, we need a string search algorithm
// KMP and AC algorithm both are good choice, But we need the code
// is simple, readable and use lower RAM/ROM.
// So We use a simplified search algorithm, this alg is like the KMP.
// Specifically we assume the failure vecotor is [-1, 0, 0, 0, ...]
// This assuming is not work for all pattern string. Fortunately,
// it's work  for this scene.

#define DEFINE_MODEM_RESP_STRDATA_TABLE(id, str) [id] = str
#define DEFINE_MODEM_RESP_STRLEN_TABLE(id, str)  [id] = (sizeof(str)-1)


static char *resp_strdata[] =
{
    MODEM_CHAT_RESP_LIST(DEFINE_MODEM_RESP_STRDATA_TABLE)
};

static rt_uint8_t resp_strlen[] =
{
    MODEM_CHAT_RESP_LIST(DEFINE_MODEM_RESP_STRLEN_TABLE)
};

#define CHAT_DATA_FMT           "<tx: %s, want: %s, retries: %u, timeout: %u>"
#define CHAT_DATA_STR(data)     (data)->transmit, resp2str((data)->expect), (data)->retries, (data)->timeout

static const char* resp2str(rt_uint8_t resp_id)
{
    if (resp_id == MODEM_CHAT_RESP_NOT_NEED)
        return "(not need)";
    RT_ASSERT(resp_id < MODEM_CHAT_RESP_MAX);
    return resp_strdata[resp_id];
}

/* only one device support */
static struct rt_completion rx_comp_p;

static rt_uint8_t resp_match(rt_uint8_t resp_id, rt_uint8_t state, char ch)
{
    while (1)
    {
        if (resp2str(resp_id)[state] == ch)
            return state + 1;
        if (state == 0)
            return 0;
        state = 0;
    };
}

static rt_bool_t resp_matched(rt_uint8_t resp_id, rt_uint8_t state)
{
    return state == resp_strlen[resp_id];
}

/*
 * chat_rx_ind , callback function if serial recieve data
 *
 * @param rt_device_t                       device
 *        rt_size_t                         size
 *
 * @return  0   :   successful
 *
 */
static rt_err_t chat_rx_ind(rt_device_t device, rt_size_t size)
{
    rt_completion_done(&rx_comp_p);
    return RT_EOK;
}

/*
 * chat_read_until , waitting for recieve data from serial
 *
 * @param struct rt_serial_device           *serial
 *        const struct modem_chat_data      *data
 *
 * @return  0   :   timeout, can't recieve any data
 *          size:   the size of recieve data
 */
static rt_size_t chat_read_until(rt_device_t serial, void *buffer, rt_size_t size, rt_tick_t stop)
{
    rt_size_t rdlen;
    rt_tick_t wait;

    rt_completion_init(&rx_comp_p);
    rdlen = rt_device_read(serial, 0, buffer, size);
    if (rdlen)
        return rdlen;

    wait = stop - rt_tick_get();
    if (wait > RT_TICK_MAX / 2)
        return 0;

    rt_completion_wait(&rx_comp_p, wait);
    return rt_device_read(serial, 0, buffer, size);
}

/*
 * modem_flush_rx , clear data what is in the rx buffer
 *
 * @param struct rt_serial_device           *serial
 *
 * @return  RT_NULL: none
 *
 */
static void modem_flush_rx(rt_device_t serial)
{
    char rdbuf[CHAT_READ_BUF_MAX] = {0};

    while (rt_device_read(serial, 0, rdbuf, CHAT_READ_BUF_MAX));
}

/*
 * modem_chat_once , send an order to control modem
 *
 * @param struct rt_serial_device           *serial
 *        const struct modem_chat_data      *data
 *
 * @return  0: execute successful
 *
 */
static rt_err_t modem_chat_once(rt_device_t serial, const struct modem_chat_data *data)
{
    rt_uint8_t resp_state[MODEM_CHAT_RESP_MAX] = { 0 }, resp;
    rt_tick_t stop = rt_tick_get() + data->timeout*RT_TICK_PER_SECOND;
    rt_size_t rdlen, pos;
    char rdbuf[CHAT_READ_BUF_MAX];

    if (data->transmit)
    {
        LOG_D(CHAT_DATA_FMT " transmit --> modem", CHAT_DATA_STR(data));
        rt_device_write(serial, 0, data->transmit, rt_strlen(data->transmit));
        if (data->ignore_cr == RT_FALSE)
            rt_device_write(serial, 0, "\r", 1);
    }

    if (data->expect == MODEM_CHAT_RESP_NOT_NEED)
    {
        rt_thread_mdelay(1000*data->timeout);
        return RT_EOK;
    }

    do
    {
        rdlen = chat_read_until(serial, rdbuf, CHAT_READ_BUF_MAX, stop);
        for (pos = 0; pos < rdlen; pos++)
        {
            for (resp = 0; resp < MODEM_CHAT_RESP_MAX; resp++)
            {
                resp_state[resp] = resp_match(resp, resp_state[resp], rdbuf[pos]);
                if (resp_matched(resp, resp_state[resp]))
                {
                    if (resp == data->expect)
                        return RT_EOK;

                    LOG_W(CHAT_DATA_FMT" not matched, got: %s", CHAT_DATA_STR(data), resp2str(resp));
                    return -RT_ERROR;
                }
            }
        }
    } while ( stop - rt_tick_get() < RT_TICK_MAX / 2);
    LOG_W(CHAT_DATA_FMT" timeout", CHAT_DATA_STR(data));
    return -RT_ETIMEOUT;
}

/*
 * modem_chat_internal , init modem and turn modem into ppp type
 *
 * @param struct rt_serial_device           *serial
 *        const struct modem_chat_data      *data
 *        rt_size_t                         len
 *
 * @return  0: execute successful
 *
 */
static rt_err_t modem_chat_internal(rt_device_t serial, const struct modem_chat_data *data, rt_size_t len)
{
    rt_err_t err = RT_EOK;
    rt_size_t i;
    rt_uint8_t retry_time;

    for (i = 0; i < len; i++)
    {
        LOG_D(CHAT_DATA_FMT" running", CHAT_DATA_STR(&data[i]));
        for (retry_time = 0; retry_time < data[i].retries; retry_time++)
        {
            modem_flush_rx(serial);
            err = modem_chat_once(serial, &data[i]);
            if (err == RT_EOK)
                break;
        }
        if (err != RT_EOK)
        {
            LOG_E(CHAT_DATA_FMT" fail", CHAT_DATA_STR(&data[i]));
            break;
        }
        LOG_D(CHAT_DATA_FMT" success", CHAT_DATA_STR(&data[i]));
    }
    return err;
}

/*
 * modem_chat , a function for ppp dailing to network
 *
 * @param struct ppp_device                 *device
 *        const struct modem_chat_data      *data
 *        rt_size_t                         len
 *
 * @return  0: execute successful
 *
 */
rt_err_t modem_chat(rt_device_t serial, const struct modem_chat_data *data, rt_size_t len)
{
    rt_err_t (*old_rx_ind)(rt_device_t dev, rt_size_t size) = RT_NULL;

    rt_err_t err = RT_EOK;

    rt_completion_init(&rx_comp_p);
    old_rx_ind = serial->rx_indicate;
    rt_device_set_rx_indicate(serial, chat_rx_ind);

    LOG_D("(%s) has control by modem_chat.", serial->parent.name);
    err = modem_chat_internal(serial, data, len);
    if (err != RT_EOK)
    {
        LOG_E("chat failed");
    }

    serial->rx_indicate = old_rx_ind;
    LOG_D("(%s) has control by ppp_device.", serial->parent.name);
    return err;
}
