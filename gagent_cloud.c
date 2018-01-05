/*
 * File      : gagent_cloud.c
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

#include "gagent_def.h"
#include "gagent_cloud.h"
//

#if !defined(PKG_PAHOMQTT_SUBSCRIBE_HANDLERS) || (PKG_PAHOMQTT_SUBSCRIBE_HANDLERS < 2)
#error "must defined PKG_PAHOMQTT_SUBSCRIBE_HANDLERS >= 2 in menuconfig"
#endif


static con_st con;        //config info
static gagent_cloud_param con_param = {0};
//
cloud_st *cloud = RT_NULL;     //cloud handle
lan_st *lan = RT_NULL;       //lan handle



int gagent_cloud_send_packet(rt_uint8_t action, rt_uint8_t *buf, rt_uint16_t buf_len)
{
    int rc = RT_EOK;

    RT_ASSERT(action != 0);

    rc = gagent_mqtt_send_packet(cloud, action, buf, buf_len);
    if(rc != RT_EOK)
    {
        gagent_err("gagent_mqtt_send_packet failed:%d\n", rc);
    }

    rc = gagent_lan_send_packet(lan, action, buf, buf_len);
    if(rc != RT_EOK)
    {
        gagent_err("gagent_lan_send_packet failed:%d\n", rc);
    }
    
    return rc;
}

int gagent_cloud_recv_packet(rt_uint8_t from, rt_uint8_t action, rt_uint8_t *kv, rt_uint16_t kv_len)
{
    int rc = RT_EOK;
    gagent_dbg("from:%s\n", (from == CMD_FROM_LAN) ? "lan packet" : "mqtt packet");
    
    if(con_param.recv_packet_callback != RT_NULL)
        rc = con_param.recv_packet_callback(from, action, kv, kv_len);
        
    return rc;
}

static int gagent_cloud_parse_config(con_st *con)
{
    rt_bool_t write_flag = RT_FALSE;

    RT_ASSERT(con != RT_NULL);

    //read file
    memset(con, 0, sizeof(con_st));
            
    if(con_param.read_param_callback(con, sizeof(con_st)) != RT_EOK)
        gagent_err("read param failed!\n");
    
    if(con->mac[0] == 0 || memcmp(con->mac, con_param.mac, sizeof(con->mac)) != 0)
    {
        rt_memset(con->did, 0, sizeof(con->did));
        //
        rt_memset(con->mac, 0, sizeof(con->mac));
        rt_memcpy(con->mac, con_param.mac, sizeof(con->mac) - 1);
        write_flag = RT_TRUE;
        gagent_dbg("con->mac changed!\n");
    }

    if(con->pk[0] == 0 || strcmp(con->pk, con_param.product_key) != 0)
    {
        rt_memset(con->did, 0, sizeof(con->did));
        //
        rt_memset(con->pk, 0, sizeof(con->pk));
        rt_strncpy(con->pk, con_param.product_key, sizeof(con->pk) - 1);
        write_flag = RT_TRUE;
        gagent_dbg("con->pk changed!\n");
    }
    
    if(con->pk_secret[0] == 0 || strcmp(con->pk_secret, con_param.product_secret) != 0)
    {
        rt_memset(con->did, 0, sizeof(con->did));
        //
        rt_memset(con->pk_secret, 0, sizeof(con->pk_secret));
        rt_strncpy(con->pk_secret, con_param.product_secret, sizeof(con->pk_secret) - 1);
        write_flag = RT_TRUE;
        gagent_dbg("product secret changed!\n");
    }
    
    if(con->hard_version[0] == 0 || strcmp(con->hard_version, HARD_VERSION) != 0)
    {
        rt_memset(con->hard_version, 0, sizeof(con->hard_version));
        rt_strncpy(con->hard_version, HARD_VERSION, sizeof(con->hard_version) - 1);
        write_flag = RT_TRUE;
        gagent_dbg("hard_version changed!\n");
    }
    
    if(con->soft_version[0] == 0 || strcmp(con->soft_version, SOFT_VERSION) != 0)
    {
        rt_memset(con->soft_version, 0, sizeof(con->soft_version));
        rt_strncpy(con->soft_version, SOFT_VERSION, sizeof(con->soft_version) - 1);
        write_flag = RT_TRUE;
        gagent_dbg("soft_verson changed!\n");
    }

    if(con->passcode[0] == 0)
    {
        //passcode is empty
        rt_memset(con->passcode, 0, sizeof(con->passcode));
        rt_memcpy(con->passcode, con->pk, 10);
        write_flag = RT_TRUE;
        gagent_dbg("passcode empty!\n");
    }

#if (PKG_GAGENT_CLOUD_DEBUG == 1)
	{
		rt_uint8_t i;
        rt_kprintf("mac: ");
        for(i = 0; i < MAX_MAC_LEN; i ++)
        {
            rt_kprintf("%02x ", con->mac[i]);
        }
        rt_kprintf("\r\n");
        rt_kprintf("did:%s\n", con->did);
        rt_kprintf("passcode:%s\n", con->passcode);
        rt_kprintf("pk:%s\n", con->pk);
        rt_kprintf("pk_secret:%s\n", con->pk_secret);
        rt_kprintf("hard_version:%s\n", con->hard_version);
        rt_kprintf("soft_version:%s\n", con->soft_version);
	}
#endif
	
    if(write_flag)
    {
        gagent_dbg("write param!\n");
         con_param.write_param_callback(con, sizeof(con_st));
    }
    
    return RT_EOK;
}

static int gagent_cloud_init(cloud_st *cloud)
{
    int rc = RT_EOK;

    if(cloud->con->did[0] == 0)
    {
        rt_memset(cloud->con->did, 0, sizeof(cloud->con->did));

        rc = gagent_cloud_register(cloud);
        if(rc != RT_EOK)
        {
            gagent_err("gagent_cloud_register failed! errno:%d\n", rc);
            return rc;
        }
        //write param
        con_param.write_param_callback(cloud->con, sizeof(con_st));
    }

    rc = gagent_cloud_provision(cloud);

    return RT_EOK;

}

void gagent_cloud_thread(void *parameter)
{
    int rc = RT_EOK;

    rc = gagent_cloud_parse_config(&con);
    if(rc != RT_EOK)
    {
        gagent_err("gagent_cloud_parse_config failed!\n");
        goto __exit;
    }

    rc = gagent_cloud_init(cloud);
    if(cloud == RT_NULL)
    {
        gagent_err("gagent_cloud_init failed!\n");
        goto __exit;
    }

    rc = gagent_lan_init(lan);
    if(rc != RT_EOK)
    {
        gagent_err("gagent_cloud_lan_init failed!\n");
        goto __exit;
    }
		
    rc = gagent_mqtt_init(cloud);
    if(rc != RT_EOK)
    {
        gagent_err("gagent_cloud_mqtt_init failed!\n");
        goto __exit;
    }

    rc = gagent_cloud_check_ota(cloud);

__exit:
    return;
}

int gagent_cloud_start(gagent_cloud_param *param)
{
    int rc = RT_EOK;
    rt_thread_t thread = RT_NULL;
    
    RT_ASSERT(param != RT_NULL);
    RT_ASSERT(param->product_key[0] != RT_NULL);
    RT_ASSERT(param->product_secret[0] != RT_NULL);
    RT_ASSERT(param->mac[0] != RT_NULL);
    RT_ASSERT(param->read_param_callback != RT_NULL);
    RT_ASSERT(param->write_param_callback != RT_NULL);

    memset(&con_param, 0, sizeof(con_param));
    memcpy(&con_param, param, sizeof(con_param));

    cloud = (cloud_st *)rt_malloc(sizeof(cloud_st));
    if(RT_NULL == cloud)
    {
        gagent_err("malloc failed!\n");
        rc = -RT_ENOMEM;
        goto __exit;
    }
    rt_memset(cloud, 0, sizeof(cloud_st));
    cloud->con = &con;

    lan = (lan_st *)rt_malloc(sizeof(lan_st));
    if(RT_NULL == lan)
    {
        gagent_err("malloc failed!\n");
        rc = -RT_ENOMEM;
        goto __exit;
    }
    memset(lan, 0, sizeof(lan_st));
    lan->con = &con;

    thread = rt_thread_create("gagent", 
                                gagent_cloud_thread, 
                                RT_NULL, 
                                4096, 
                                RT_THREAD_PRIORITY_MAX / 3, 
                                20);
    if(RT_NULL != thread)
    {
        rt_thread_startup(thread);
    }
    else
    {
        rc = -RT_ERROR;
        goto __exit;
    }
    
    return rc;
    
__exit:
    if(cloud != RT_NULL)
        rt_free(cloud);

    if(lan != RT_NULL)
        rt_free(lan);
        
    return rc;    
}
