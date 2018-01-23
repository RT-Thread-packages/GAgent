/*
* File      : gagent_lan.c
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
#include "gagent_def.h"
#include "gagent_cloud.h"

#include <lwip/netifapi.h>


#if !defined(LWIP_NETIF_LOOPBACK) || (LWIP_NETIF_LOOPBACK == 0)
#error "must enable (LWIP_NETIF_LOOPBACK = 1) for publish!"
#endif /* LWIP_NETIF_LOOPBACK */

#define     GAGENT_CLOUD_UDP_PORT           12414
#define     GAGENT_CLOUD_TCP_PORT           12416
#define     GAGENT_CLOUD_BROADCAST          2415

#define     GAGENT_LAN_TIMEOUT              5           //sec

static rt_thread_t lan_thread;
static int local_port = 7100;

#ifdef PKG_GAGENT_CLOUD_DEBUG
#define     LAN_RECV_DEBUG
#define     LAN_SEND_DEBUG
#endif


static int lan_send_device_info(lan_st *lan, rt_uint8_t send_type)
{
    char *index, *len_index;

    memset(lan->send_buf, 0, sizeof(lan->send_buf));

    index = lan->send_buf;

    //head
    *index ++ = 0x00;
    *index ++ = 0x00;
    *index ++ = 0x00;
    *index ++ = 0x03;

    //len
    len_index = index;
    index ++;

    //flag
    *index ++ = 0x00;

    //cmd
    *index ++ = 0x00;
    if(send_type == UDP_SEND_TYPE_DISCOVER)
        *index ++ = 0x04;
    else if(send_type == UDP_SEND_TYPE_BOARDCAST)
        *index ++ = 0x05;

    //did_len
    *index ++ = 0x00;
    *index ++ = strlen(lan->con->did);
    //did
    memcpy(index, lan->con->did, 0x16);
    index += strlen(lan->con->did);

    //mac_len
    *index ++ = 0x00;
    *index ++ = 0x06;
    //mac
    memcpy(index, lan->con->mac, 0x06);
    index += 0x06;

    //hard_version_len
    *index ++ = 0x00;
    *index ++ = strlen(lan->con->soft_version);
    //soft_version
    memcpy(index, lan->con->soft_version, strlen(lan->con->soft_version));
    index += strlen(lan->con->soft_version);

    //pk_len
    *index ++ = 0x00;
    *index ++ = strlen(lan->con->pk);
    //pk
    memcpy(index, lan->con->pk, strlen(lan->con->pk));
    index += strlen(lan->con->pk);

    index += 8;

    lan->send_len = index - lan->send_buf;
    *len_index = lan->send_len - HEAD_LEN;

    return RT_EOK;
}

static int lan_udp_do_packet(lan_st *lan)
{
    char *one_packet;
    int data_len;
    rt_uint8_t len_num;
    short cmd;
    int rc = RT_EOK;

    one_packet = lan->recv_buf;
    while(gagent_get_one_packet(one_packet, &data_len, &len_num, lan->recv_len) == 0)
    {   
        rc = RT_EOK;

        memcpy(&cmd, one_packet + 6, 2);
        cmd = ntohs(cmd);

        gagent_dbg("cmd:%d\n", cmd);
        switch(cmd)
        {
        case 0x03:
            rc = lan_send_device_info(lan, UDP_SEND_TYPE_DISCOVER);
            break;
        }

        one_packet += (data_len + len_num + HEAD_LEN);
        lan->recv_len -= (data_len + len_num + HEAD_LEN);

        if(rc != 0)
            return rc;
    }

    return rc;
}

static int lan_get_passcode(lan_st *lan)
{
    char *index, *len_index;

    index = lan->send_buf;

    memset(index, 0, sizeof(lan->send_buf));

    //head
    *index ++ = 0x00;
    *index ++ = 0x00;
    *index ++ = 0x00;
    *index ++ = 0x03;

    //len
    len_index = index;
    index ++;

    //flag
    *index ++ = 0x00;

    //cmd
    *index ++ = 0x00;
    *index ++ = 0x07;

    //passcode_len
    *index ++ = 0x00;
    *index ++ = strlen(lan->con->passcode);

    memcpy(index, lan->con->passcode, strlen(lan->con->passcode));
    index += strlen(lan->con->passcode);

    //
    lan->send_len = (index - lan->send_buf);
    *len_index = lan->send_len - HEAD_LEN;

    return RT_EOK;
}

static int lan_get_device_info(lan_st *lan)
{
    char *index, *len_index;

    index = lan->send_buf;

    memset(index, 0, sizeof(lan->send_buf));

    //head
    *index ++ = 0x00;
    *index ++ = 0x00;
    *index ++ = 0x00;
    *index ++ = 0x03;

    //len
    len_index = index;
    index ++;

    //flag
    *index ++ = 0x00;

    //cmd
    *index ++ = 0x00;
    *index ++ = 0x14;

    //hard_version
    memcpy(index, lan->con->hard_version, strlen(lan->con->hard_version));
    index += 8;

    //soft_version
    memcpy(index, lan->con->soft_version, strlen(lan->con->soft_version));
    index += 8;

    //mcu_hard
    index += 8;

    //mcu_soft_version
    index += 8;

    //p0 version
    index += 8;

    //remain1
    index += 8;

    //remain2_len
    *index ++ = 0x00;
    *index ++ = 0x00;

    //pk_len
    *index ++ = 0x00;
    *index ++ = strlen(lan->con->pk);

    //pk
    memcpy(index, lan->con->pk, strlen(lan->con->pk));
    index += strlen(lan->con->pk);

    lan->send_len = (index - lan->send_buf);
    *len_index = lan->send_len - HEAD_LEN;

    return RT_EOK;
}

static int lan_login_device(lan_st *lan, char *packet)
{
    char *index, *len_index;
    char passcode_recv[32];
    rt_uint16_t passcode_len;

    memcpy(&passcode_len, packet + HEAD_LEN + 1 + 2, 2);
    passcode_len = ntohs(passcode_len);

    memset(passcode_recv, 0x00, sizeof(passcode_recv));
    memcpy(passcode_recv, packet + HEAD_LEN + 1 + 2 + 2, passcode_len);

    index = lan->send_buf;

    memset(index, 0, sizeof(lan->send_buf));

    //head
    *index ++ = 0x00;
    *index ++ = 0x00;
    *index ++ = 0x00;
    *index ++ = 0x03;

    //len
    len_index = index;
    index ++;

    //flag
    *index ++ = 0x00;

    //cmd
    *index ++ = 0x00;
    *index ++ = 0x09;

    //result
    if(strncmp(lan->con->passcode, passcode_recv, passcode_len) == 0)
        *index ++ = 0x00;
    else
        *index ++ = 0x01;

    lan->send_len = index - lan->send_buf;
    *len_index = lan->send_len - HEAD_LEN;

    return RT_EOK;
}


static int lan_trans_data(lan_st *lan, char *packet)
{
    rt_uint16_t len, cmd;
    char *index, *kv;
    rt_uint16_t kv_len;
    rt_uint8_t length_len, action;

    lan->send_len = 0;
    //
    action = 0;
    kv_len = 0;
    kv = 0;

    index = packet;
    len = gagent_parse_rem_len((const uint8_t *)index + 4);
    length_len = gagent_num_rem_len_bytes((const uint8_t*)index + 4);

    index += (HEAD_LEN + length_len);

    rt_memcpy(&cmd, index, 2);
    index += 2;

    cmd = ntohs(cmd);
    gagent_dbg("cmd:%0x\n", cmd);

    // 00 00 00 03 06 00 00 90 01 01 01 
    // 00 00 00 03 0A 00 00 93 00 00 00 00 01 01 01 
    if(cmd == 0x90)
    {
        action = *index ++;
        kv =  index;
        kv_len = len - 4;
        lan->sn = -1;
    }
    else if(cmd == 0x93)
    {
        memcpy(&lan->sn, index, 4);
        index += 4;
        gagent_dbg("lan_sn:%d\n", lan->sn);

        action = *index ++;
        kv = index;
        kv_len = len - 8;
    }
    else
        return -RT_ERROR;

    return gagent_cloud_recv_packet(CMD_FROM_LAN, action, (uint8_t *)kv, kv_len);
}

static int lan_heart_beat(lan_st *lan)
{
    char *index, *len_index;

    index = lan->send_buf;

    memset(index, 0, sizeof(lan->send_buf));

    //head
    *index ++ = 0x00;
    *index ++ = 0x00;
    *index ++ = 0x00;
    *index ++ = 0x03;

    //len
    len_index = index;
    index ++;

    //flag
    *index ++ = 0x00;

    //cmd
    *index ++ = 0x00;
    *index ++ = 0x16;

    lan->send_len = index - lan->send_buf;
    *len_index = lan->send_len - HEAD_LEN;

    return RT_EOK;
}

static int lan_tcp_do_packet(lan_st *lan)
{
    char *one_packet;
    rt_uint32_t data_len;
    rt_uint8_t len_num;
    rt_uint16_t cmd;
    int rc = RT_EOK;

    one_packet = lan->recv_buf;
    while(gagent_get_one_packet(one_packet, (int *)&data_len, &len_num, lan->recv_len) == RT_EOK)
    {   
        rc = RT_EOK;

        memcpy(&cmd, one_packet + 6, 2);
        cmd = ntohs(cmd);

        gagent_dbg("lan_cmd:%x\n",cmd);
        switch(cmd)
        {
        case 0x06:
            rc = lan_get_passcode(lan);
            break;

        case 0x08:
            rc = lan_login_device(lan, one_packet);
            break;

        case 0x90:
        case 0x93:
            rc = lan_trans_data(lan, one_packet);
            break;

        case 0x13:
            rc = lan_get_device_info(lan);
            break;

        case 0x15:
            rc = lan_heart_beat(lan);
            break;

        default:
            break;
        }

        one_packet += (data_len + len_num + HEAD_LEN);
        lan->recv_len -= (data_len + len_num + HEAD_LEN);

        if(rc != 0)
            return rc;
    }

    return rc;
}

static int gagent_create_tcp_socket(lan_st *lan)
{
    int rc = RT_EOK;
    int opt;

    if(lan->tcp_server != -1)
    return RT_EOK;

    lan->tcp_server = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if(lan->tcp_server < 0)
    {
        gagent_err("tcp socket create failed!\n");
        return -RT_ERROR;
    }

    lan->tcp_server_addr.sin_family = AF_INET;
    lan->tcp_server_addr.sin_port = htons(GAGENT_CLOUD_TCP_PORT);
    lan->tcp_server_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&lan->tcp_server_addr.sin_zero, 0, sizeof(lan->tcp_server_addr.sin_zero));

    opt = 1;
    setsockopt(lan->tcp_server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    rc = lwip_bind(lan->tcp_server, (struct sockaddr *)&lan->tcp_server_addr, (socklen_t)sizeof(lan->tcp_server_addr));
    if(rc < 0)
    {
        gagent_err("tcp socket bind failed! errno:%d\n", errno);
        lwip_close(lan->tcp_server);
        return -RT_ERROR;
    }

    rc = lwip_listen(lan->tcp_server, MAX_CLIENT);
    if(rc < 0)
    {
        gagent_err("tcp socket listen failed! errno:%d\n", errno);
        lwip_close(lan->tcp_server);
        return -RT_ERROR;
    }

    return RT_EOK;
}


static int gagent_create_udp_socket(lan_st *lan)
{
    int opt;
    int broadcast;

    if(lan->udp_server != -1)
        return RT_EOK;

    lan->udp_server = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    if(lan->udp_server < 0)
    {
        gagent_err("udp socket create failed!\n");
        return -RT_ERROR;
    }

    rt_memset(&lan->udp_socket_addr, 0, sizeof(lan->udp_socket_addr));
    lan->udp_socket_addr.sin_family = AF_INET;
    lan->udp_socket_addr.sin_port = htons(GAGENT_CLOUD_UDP_PORT);
    lan->udp_socket_addr.sin_addr.s_addr = INADDR_ANY;

    opt = 1;
    lwip_setsockopt(lan->udp_server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    broadcast = 1;
    lwip_setsockopt(lan->udp_server, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    if(lwip_bind(lan->udp_server, (struct sockaddr *)&lan->udp_socket_addr, (socklen_t)sizeof(lan->udp_socket_addr)) < 0)
    {
        gagent_err("udp socket bind failed! errno:%d\n", errno);
        lwip_close(lan->udp_server);
        return -RT_ERROR;
    }

    rt_memset(&lan->broadcast_to, 0, sizeof(lan->broadcast_to));
    lan->broadcast_to.sin_family = AF_INET;
    lan->broadcast_to.sin_port = htons(GAGENT_CLOUD_BROADCAST);
    lan->broadcast_to.sin_addr.s_addr = inet_addr("255.255.255.255");

    return RT_EOK;
}

static int gagent_create_localudp_socket(lan_st *lan)
{
    struct sockaddr_in local_udp_server;

    if(lan->local_sock != -1)
        return RT_EOK;

    lan->local_sock = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    if(lan->local_sock < 0)
    {
        gagent_err("local udp socket create failed!\n");
        return -RT_ERROR;
    }

    memset(&local_udp_server, 0, sizeof(local_udp_server));
    local_udp_server.sin_family = AF_INET;
    local_udp_server.sin_port = htons(local_port);
    local_udp_server.sin_addr.s_addr = INADDR_ANY;

    if(lwip_bind(lan->local_sock, (struct sockaddr *)&local_udp_server, sizeof(local_udp_server)) < 0)
    {
        gagent_err("local udp socket bind failed!\n");
        return -RT_ERROR;
    }

    return RT_EOK;
}

static int gagent_close_tcp_socket(lan_st *lan)
{
    if(lan->tcp_server != -1)
    {
        lwip_close(lan->tcp_server);
        lan->tcp_server = -1;
    }

    return RT_EOK;
}

static int gagent_close_udp_socket(lan_st *lan)
{
    if(lan->udp_server != -1)
    {
        lwip_close(lan->udp_server);
        lan->udp_server = -1;
    }

    return RT_EOK;
}

static int gagent_close_localudp_socket(lan_st *lan)
{
    if(lan->local_sock != -1)
    {
        lwip_close(lan->local_sock);
        lan->local_sock = -1;
    }

    return RT_EOK;
}

void gagent_lan_thread(void *parameter)
{
    lan_st *lan = (lan_st *)parameter;
    int rc = RT_EOK;
    fd_set readfds;
    int maxfd;
    uint8_t id;
    struct timeval timeout;
    int client_sock;
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    uint8_t tcp_client_count = 0;

    RT_ASSERT(lan != RT_NULL);

    lan->tcp_server = -1;
    lan->udp_server = -1;
    lan->local_sock = -1;

    while(1)
    {
        if(gagent_create_tcp_socket(lan) < 0)
        {
            rt_thread_delay(rt_tick_from_millisecond(3000));
            continue;
        }

        if(gagent_create_udp_socket(lan) < 0)
        {
            rt_thread_delay(rt_tick_from_millisecond(3000));
            continue;
        }

        if(gagent_create_localudp_socket(lan) < 0)
        {
            rt_thread_delay(rt_tick_from_millisecond(3000));
            continue;
        }

        for(id = 0; id < MAX_CLIENT; id ++)
        {
            lan->client_fd[id] = -1;
        } 

        while(1)
        {
            FD_ZERO(&readfds);
            FD_SET(lan->tcp_server, &readfds);
            FD_SET(lan->udp_server, &readfds);
            FD_SET(lan->local_sock, &readfds);

            maxfd = lan->tcp_server;
            if(lan->udp_server > maxfd)
                maxfd = lan->udp_server;

            if(lan->local_sock > maxfd)
                maxfd = lan->local_sock;

            for(id = 0; id < MAX_CLIENT; id ++)
            {
                if(lan->client_fd[id] == -1)
                    continue;

                FD_SET(lan->client_fd[id], &readfds);
                if(lan->client_fd[id] > maxfd)
                    maxfd = lan->client_fd[id];
            }

            timeout.tv_sec = GAGENT_LAN_TIMEOUT;
            timeout.tv_usec = 0;

            rc = lwip_select(maxfd + 1, &readfds, 0, 0, &timeout);
            if(rc < 0)
            {
                gagent_err("socket select failed!\n");
                break;
            }
            else if(rc == 0)
            {
                if(tcp_client_count >= MAX_CLIENT)
                    continue;

                //broadcast    
                rc = lan_send_device_info(lan, UDP_SEND_TYPE_BOARDCAST);
                if(rc == RT_EOK && lan->send_len > 0)
                {
                    rc = lwip_sendto(lan->udp_server, lan->send_buf, lan->send_len, 0,
                                        (struct sockaddr *)&lan->broadcast_to, (socklen_t)sizeof(lan->broadcast_to));
                    if(rc <= 0)
                    {
                        gagent_err("udp socket broadcast failed! errno:%d\n", errno);
                        break;
                    }
                }   

                continue;
            }

            //local
            if(FD_ISSET(lan->local_sock, &readfds))
            {
                //locak socket can read
                struct sockaddr_in local_client_addr;
                int addr_len = sizeof(local_client_addr);
                rt_uint8_t i;

                lan->recv_len = lwip_recvfrom(lan->local_sock, lan->recv_buf, sizeof(lan->recv_buf), 0,
                                                (struct sockaddr *)&local_client_addr, (socklen_t *)&addr_len);
                if(local_client_addr.sin_addr.s_addr != *(uint32_t *)&netif_default->ip_addr)
                    continue;

                if(lan->recv_len <= 0)
                {
                    gagent_err("local udp socket recvfrom failed!\n");
                    continue;
                }

                for(i = 0; i < MAX_CLIENT; i ++)
                {
                    if(lan->client_fd[i] == -1)
                        continue;

                    rc = lwip_send(lan->client_fd[i], lan->recv_buf, lan->recv_len, 0);
                    if(rc <= 0)
                    {
                        gagent_err("send clnt %d failed! errno:%d\n", i, errno);
                        lwip_close(lan->client_fd[i]);
                        lan->client_fd[i] = -1;
                        if(tcp_client_count > 0)
                        tcp_client_count --;
                    }
                    gagent_dbg("send client send len:%d\n", rc);
                }
            }

            if(FD_ISSET(lan->udp_server, &readfds))
            {
                //udp socket can read
                addr_len = sizeof(client_addr);
                lan->recv_len = lwip_recvfrom(lan->udp_server, lan->recv_buf, sizeof(lan->recv_buf), 0,
                        (struct sockaddr *)&client_addr, (socklen_t *)&addr_len);
                if(lan->recv_len <= 0)
                {
                    gagent_err("udp socket recv from failed! errno:%d\n", errno);
                    break;
                }

#ifdef LAN_RECV_DEBUG
                {
                    int i;
                    gagent_dbg("udp client:%s port:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    rt_kprintf("recv len:%d ", lan->recv_len);
                    for(i = 0; i < lan->recv_len; i ++)
                        rt_kprintf("%02x ", lan->recv_buf[i]);

                    rt_kprintf("\r\n");
                }
#endif

                rc = lan_udp_do_packet(lan);
                if(rc == RT_EOK && lan->send_len > 0)
                {
#ifdef LAN_RECV_DEBUG
                    gagent_dbg("udp client:%d send_len:%d\n", id, lan->send_len);
                    {
                        int t;
                        for(t = 0; t < lan->send_len; t ++)
                            rt_kprintf("%02x ", lan->send_buf[t]);

                        rt_kprintf("\r\n");
                    }
#endif
                    rc = lwip_sendto(lan->udp_server, lan->send_buf, lan->send_len, 0,
                                        (struct sockaddr *)&client_addr, (socklen_t)sizeof(client_addr));
                    if(rc <= 0)
                    {
                        gagent_err("udp socket sendto failed! errno:%d\n", errno);
                        break;
                    }
                    gagent_dbg("udp socket sendto len:%d\n", rc);
                }
            }

            if(FD_ISSET(lan->tcp_server, &readfds))
            {
                //tcp socket can read
                addr_len= sizeof(struct sockaddr);
                client_sock = lwip_accept(lan->tcp_server, (struct sockaddr *)&client_addr, (socklen_t *)&addr_len);
                if(client_sock < 0)
                {
                    gagent_err("tcp socket accept failed! errno:%d\n", errno);
                    break;
                }
                gagent_dbg("client_sock:%d\n", client_sock);

                for(id = 0; id < MAX_CLIENT; id ++)
                {
                    if(lan->client_fd[id] == -1)
                        break;
                }

                if(id >= MAX_CLIENT)
                {
                    gagent_dbg("max client!\n");
                    lwip_close(client_sock);
                    continue;
                }

                lan->client_fd[id] = client_sock;
                tcp_client_count ++;
                gagent_dbg("new client: %d %s port:%d\n", id, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            }

            for(id = 0; id < MAX_CLIENT; id ++)
            {
                if(lan->client_fd[id] == -1)
                    continue;

                if(FD_ISSET(lan->client_fd[id], &readfds))
                {
                    memset(lan->recv_buf, 0, sizeof(lan->recv_buf));
                    lan->recv_len = lwip_recv(lan->client_fd[id], lan->recv_buf, sizeof(lan->recv_buf), 0);
                    if(lan->recv_len <= 0)
                    {
                        gagent_err("lan:%d lan->recv_len:%d errno:%d\n", id, lan->recv_len, errno);

                        lwip_close(lan->client_fd[id]);
                        lan->client_fd[id] = -1;
                        if(tcp_client_count > 0)
                            tcp_client_count --;
                        continue;
                    }

#ifdef LAN_RECV_DEBUG
                    {
                        int t;
                        gagent_dbg("lan %d recv len:%d\n", id, lan->recv_len);
                        for(t = 0; t < lan->recv_len; t ++)
                            rt_kprintf("%02x ", lan->recv_buf[t]);

                        rt_kprintf("\r\n");
                    }
#endif
                    rc = lan_tcp_do_packet(lan);
                    //
                    if(rc == RT_EOK && lan->send_len > 0)
                    {
#ifdef LAN_RECV_DEBUG
                        gagent_dbg("tcp client:%d send_len:%d\n", id, lan->send_len);
                        {
                            int t;
                            for(t = 0; t < lan->send_len; t ++)
                                rt_kprintf("%02x ", lan->send_buf[t]);

                            rt_kprintf("\r\n");
                        }
#endif

                        lan->send_len = lwip_send(lan->client_fd[id], lan->send_buf, lan->send_len, 0);
                        if(lan->send_len <= 0)
                        {
                            gagent_err("tcp client [%d] send failed! errno:%d\n", id, errno);

                            lwip_close(lan->client_fd[id]);
                            lan->client_fd[id] = -1;
                            if(tcp_client_count > 0)
                                tcp_client_count --;
                            continue;
                        }
                    }
                }
            }
        }//end while

        for(id = 0; id < MAX_CLIENT; id ++)
        {
            if(lan->client_fd[id] != -1)
            {
                lwip_close(lan->client_fd[id]);
                lan->client_fd[id] = -1;
            }
        }

        tcp_client_count = 0;
        gagent_close_tcp_socket(lan);
        gagent_close_udp_socket(lan);
        gagent_close_localudp_socket(lan);
    }
}

static int lan_local_send(lan_st *lan, void *data, int len)
{
    struct sockaddr_in server_addr = {0};
    int send_len;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = lan->local_port;
    server_addr.sin_addr = *((const struct in_addr *)&netif_default->ip_addr);
    memset(&server_addr.sin_zero, 0, sizeof(server_addr.sin_zero));

    send_len = lwip_sendto(lan->local_sock, data, len, MSG_DONTWAIT,
                            (struct sockaddr *)&server_addr, sizeof(server_addr));
    return send_len;
}

int gagent_lan_send_packet(lan_st *lan, rt_uint8_t action, rt_uint8_t *buf, rt_uint16_t buf_len)
{
    int rc = RT_EOK;

    memset(lan->send_buf, 0, sizeof(lan->send_buf));
    lan->send_len = 0;

    lan->send_len = gagent_set_one_packet(lan->send_buf, action, buf, buf_len);

#ifdef LAN_SEND_DEBUG
    {
        uint32_t i;
        rt_kprintf("lan send_len:%d\n", lan->send_len);
        for(i = 0; i < lan->send_len; i ++)
            rt_kprintf("%02x ", lan->send_buf[i]);

        rt_kprintf("\r\n");
    }
#endif

    rc = lan_local_send(lan, lan->send_buf, lan->send_len);
    if (rc == lan->send_len)
        rc = RT_EOK;
    else
        rc = -RT_ERROR;

    return rc;
}

int gagent_lan_init(lan_st *lan)
{   
    lan_thread = rt_thread_create("gagent_lan", 
                                    gagent_lan_thread, 
                                    lan, 
                                    4096, 
                                    15, 
                                    20);
    if(lan_thread)
        rt_thread_startup(lan_thread);
    else
    {
        gagent_err("gagent_lan thread startup failed!\n");
        rt_free(lan);
        return -RT_ERROR;
    }

    return RT_EOK;
}
