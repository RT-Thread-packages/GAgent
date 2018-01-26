/*
 * File      : gagent_cloud.h
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2018, RT-Thread Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-01-03     flyingcys    first version
 */
#ifndef __GAGENT_CLOUD_H__
#define __GAGENT_CLOUD_H__

#ifdef __cplusplus
extern "C" {
#endif

#define     PRODUCT_KEY_LEN             32
#define     PRODUCT_SECERT_LEN          32
#define     MAX_MAC_LEN                 32

struct gagent_config
{
    char mac[32 + 1];
    char did[32 + 1];
    char passcode[16 + 1];
    char pk[48 + 1];
    char pk_secret[48 + 1];
    char hard_version[16 + 1];
    char soft_version[16 + 1];
}; 


typedef struct
{
    char product_key[PRODUCT_KEY_LEN + 1];
    char product_secret[PRODUCT_SECERT_LEN + 1];
    char mac[MAX_MAC_LEN + 1];
    int (*read_param_callback)(struct gagent_config *param, rt_uint32_t len);
    int (*write_param_callback)(struct gagent_config *param, rt_uint32_t len);
    int (*recv_packet_callback)(rt_uint8_t from, rt_uint8_t action, rt_uint8_t *kv, rt_uint16_t kv_len);
} gagent_cloud_param;

enum ATCTION_TYPE
{
    ACTION_CONTROL = 1,
    ACTION_READ_STATUS = 2,
    ACTION_READ_STATUS_ACK = 3,
    ACTION_REPORT_STATUS = 4,
    ACTION_TRANS_RECV = 5,
    ACTION_TRANS_SEND = 6,
    ACTION_PUSH_OTA = 254,	
};

int gagent_cloud_recv_packet(rt_uint8_t from, rt_uint8_t action, rt_uint8_t *kv, rt_uint16_t kv_len);

int gagent_cloud_send_packet(rt_uint8_t action, rt_uint8_t *buf, rt_uint16_t buf_len);

int gagent_cloud_start(gagent_cloud_param *param);


#ifdef __cplusplus
}
#endif

#endif
