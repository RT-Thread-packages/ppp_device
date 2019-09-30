/*
 * Copyright (c) 2019 xiaofan <xfan1024@live.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author          Notes
 * 2019-09-19     xiaofan         the first version
 */

#ifndef __MODEM_CHAT_H__
#define __MODEM_CHAT_H__

#include <rtthread.h>
#include <rtdevice.h>

#define MODEM_CHAT_RESP_LIST(F) \
    F(MODEM_CHAT_RESP_OK,         "OK"), \
    F(MODEM_CHAT_RESP_READY,      "READY"), \
    F(MODEM_CHAT_RESP_CONNECT,    "CONNECT"), \
    F(MODEM_CHAT_RESP_BUSY,       "BUSY"), \
    F(MODEM_CHAT_RESP_NO_CARRIER, "NO CARRIER"), \
    F(MODEM_CHAT_RESP_ERROR,      "ERROR") \

#define DEFINE_MODEM_RESP_ID_TABLE(id, s) id

enum {
    MODEM_CHAT_RESP_LIST(DEFINE_MODEM_RESP_ID_TABLE),
    MODEM_CHAT_RESP_MAX,
    MODEM_CHAT_RESP_NOT_NEED,
};

struct modem_chat_data {
    const char* transmit;
    rt_uint8_t expect;      // use CHAT_RESP_xxx
    rt_uint8_t retries;
    rt_uint8_t timeout;     // second
    rt_bool_t  ignore_cr;   // ignore CR character if it is RT_TRUE
};

rt_err_t modem_chat(rt_device_t serial, const struct modem_chat_data *data, rt_size_t len);

#endif  /* __MODEM_CHAT_H__ */
