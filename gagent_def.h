/*
 * File      : gagent_def.h
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
#ifndef __GAGENT_DEF_H__
#define __GAGENT_DEF_H__

#ifdef __cplusplus
extern "C" {
#endif


#include <rtthread.h>
#include <string.h>

#include <lwip/sockets.h>
#include "webclient.h"
#include "paho_mqtt.h"

#ifdef PKG_GAGENT_CLOUD_DEBUG
#define     gagent_dbg            rt_kprintf("[gagent dbg:%d] %s ", __LINE__, __FUNCTION__);rt_kprintf
#define     gagent_err            rt_kprintf("[gagent err:%d] %s ", __LINE__, __FUNCTION__);rt_kprintf
#else
#define     gagent_dbg(...)
#define     gagent_err(...)
#endif

#define MAX_CLIENT              8


#define BUF_LEN                 2048 

#define HEAD_LEN                5       
#define UDP_RECV_ERROR          199
#define TCP_RECV_ERROR_BASE     100
#define LOG_IP_BUF_LENGTH       16

#define HARD_VERSION            "01RTT001"
#define SOFT_VERSION            "04020020"


#define G_SERVICE_DOMAIN        "api.gizwits.com"
#define G_SERVICE_PORT          "80"
#define G_M2M_DOMAIN            "sandbox.gizwits.com"
#define G_M2M_PORT              "1883"
#define G_M2M_SSL_PORT          "8883"

#define DID_LENGTH              22


enum GAGENT_HARD_TYPE
{
    GAGENT_HARD_SOC = 1,
    GAGENT_HARD_MCU = 2,
};


typedef struct _con_st	con_st;

struct _con_st
{
    char mac[32 + 1];
    char did[32 + 1];
    char passcode[16 + 1];
    char pk[48 + 1];
    char pk_secret[48 + 1];
    char hard_version[16 + 1];
    char soft_version[16 + 1];
}; 


typedef struct _cloud_st cloud_st;

struct _cloud_st
{
    con_st *con;
    //
    char mqtt_server[128];
    int mqtt_port;
    char sub_topic[2][128];
    //
    char recv_buf[BUF_LEN];
    int recv_len;
    char send_buf[BUF_LEN];
    int send_len;
    //
    char ota_info[128];
    int sn;
};

typedef struct _lan_st lan_st;

struct _lan_st
{
    con_st	*con;
    //
    int	client_fd[MAX_CLIENT];
    int tcp_server;
    int udp_server;
    struct sockaddr_in tcp_server_addr;
    struct sockaddr_in udp_socket_addr;
    struct sockaddr_in broadcast_to;

    //local
    int local_sock;
    int local_port;
    //
    char recv_buf[BUF_LEN];
    int	recv_len;
    char send_buf[BUF_LEN];
    int	send_len;
    //
    int	sn;
};


enum CMD_FROM_TYPE
{
    CMD_FROM_LAN = 0,
    CMD_FROM_MQTT,
};

enum UDP_SEND_TYPE
{
    UDP_SEND_TYPE_DISCOVER = 0,
    UDP_SEND_TYPE_BOARDCAST,
};

int gagent_add_pkcs(char *src, int len);

uint16_t gagent_parse_rem_len(const uint8_t* buf);

uint8_t gagent_num_rem_len_bytes(const uint8_t* buf);

int gagent_get_one_packet(char *packet, int *data_len, rt_uint8_t *len_num, int remain_len);

int gagent_set_one_packet(char *packet, uint8_t action, uint8_t *buf, uint32_t buf_len);

uint8_t gagent_get_rem_len(int length, char *buf);

int gagent_strtohex(char *pbDest, char *pbSrc, int nLen);

int gagent_lan_send_packet(lan_st *lan, rt_uint8_t action, rt_uint8_t *buf, rt_uint16_t buf_len);

int gagent_lan_init(lan_st *lan);

int gagent_cloud_register(cloud_st *cloud);

int gagent_cloud_provision(cloud_st *cloud);

int gagent_cloud_check_ota(cloud_st *cloud);

int gagent_mqtt_send_packet(cloud_st *cloud, rt_uint8_t action, rt_uint8_t *kv, rt_uint16_t kv_len);

int gagent_mqtt_init(cloud_st *cloud);


#ifdef __cplusplus
}
#endif

#endif
