/*
 * File      : gagent_tool.c
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
#include <rtthread.h>

#include "gagent_cloud.h"

#ifdef RT_USING_FINSH
#include <finsh.h>
#endif

#define     DEMO_PRODUCT_KEY                    "1371df627fa64e849f32fe17ebd5fd38"
#define     DEMO_PRODUCT_KEY_SECRET             "067a83b650544d979bb3d4d147f32034"   
#define     DEMO_MAC                            "\xBC\x12\x34\x56\x78\x23"

static gagent_cloud_param gagent_param;

int gagent_read_param(void *param, rt_uint32_t len)
{
    /* read param */
    
    return RT_EOK;
}

int gagent_write_param(void *param, rt_uint32_t len)
{
    /* write param */
    
    return RT_EOK;
}

int gagent_recv_packet(rt_uint8_t from, rt_uint8_t action, rt_uint8_t *kv, rt_uint16_t kv_len)
{
    /* please read product protocol */
    uint8_t power;
    
    switch(action)
    {
        case ACTION_CONTROL: 
            rt_kprintf("ACTION_CONTROL\r\n");
            power = *(kv + 1);
            rt_kprintf("power:%d\n", power);
            gagent_cloud_send_packet(ACTION_REPORT_STATUS, &power, 1);
        break;

        case ACTION_READ_STATUS:
            rt_kprintf("ACTION_READ_STATUS\r\n");
//            gagent_cloud_send_packet(ACTION_READ_STATUS_ACK, buf, buf_len);
        break;

        case ACTION_TRANS_RECV:
            rt_kprintf("this is your raw data from app\r\n");
        break;
        
        case ACTION_PUSH_OTA:
            rt_kprintf("ACTION_PUSH_OTA\r\n");
        break;
    }
    
    return RT_EOK;
}

int gagent_cloud(void)
{
    int rc = RT_EOK;

    rt_memset(&gagent_param, 0, sizeof(gagent_param));
    //
    strcpy(gagent_param.product_key, DEMO_PRODUCT_KEY);
    strcpy(gagent_param.product_secret, DEMO_PRODUCT_KEY_SECRET);
    strcpy(gagent_param.mac, DEMO_MAC);
    gagent_param.read_param_callback = gagent_read_param;
    gagent_param.write_param_callback = gagent_write_param;
    gagent_param.recv_packet_callback = gagent_recv_packet;
    //
    gagent_cloud_start(&gagent_param);
    
    return rc;
}
#ifdef RT_USING_FINSH
MSH_CMD_EXPORT(gagent_cloud, gagent cloud demo);

FINSH_FUNCTION_EXPORT(gagent_cloud, "gagent cloud test");
#endif

