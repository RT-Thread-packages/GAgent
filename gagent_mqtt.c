/*
 * File      : gagent_mqtt.c
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


static MQTTClient gagent_mqtt;

#ifdef PKG_GAGENT_CLOUD_DEBUG
#define MQTT_RECV_DEBUG
#define MQTT_SEND_DEBUG
#endif

static int mqtt_push_ota(cloud_st *cloud, char *packet)
{
    return RT_EOK;
}

static int mqtt_trans_data(cloud_st *cloud, char *packet)
{
    uint16_t len, cmd;
    char *index, *kv;
    uint16_t kv_len;
    uint8_t length_len, action;

    action = 0;
    kv = 0;
    kv_len = 0;
    
    index = packet;
    len = gagent_parse_rem_len((const uint8_t *)index + 4);
    length_len = gagent_num_rem_len_bytes((const uint8_t *)index + 4);

    index += (HEAD_LEN + length_len);

    rt_memcpy(&cmd, index, 2);
    index += 2;
    
    cmd = ntohs(cmd);
    gagent_dbg("cmd:%x\n", cmd);

    if(cmd == 0x90)
    {
        action = *index ++;
        kv = index;
        kv_len = len - 4;
        cloud->sn = -1;
    }
    else if(cmd == 0x93)
    {
        memcpy(&cloud->sn, index, 4);
        index += 4;
        gagent_dbg("mqtt_sn:%d\n", cloud->sn);
        //
        action = *index ++;
        kv = index;
        kv_len = len - 8;
    }
    else
        return -RT_ERROR;
    
    return gagent_cloud_recv_packet(CMD_FROM_MQTT, action, (rt_uint8_t *)kv, kv_len);
}

static void gagent_mqtt_callback(MQTTClient *c, MessageData *msg_data)
{
    extern cloud_st *cloud;
    char *one_packet;
    rt_uint32_t data_len;
    rt_uint32_t total_len;
    rt_uint16_t cmd;
    rt_uint8_t len_num;
    int rc = RT_EOK;

#ifdef MQTT_RECV_DEBUG
	{
    size_t len;
    char *data = (char *)msg_data->message->payload;
    gagent_dbg("mqtt_callback topic_name: %d %s\n", msg_data->topicName->lenstring.len, msg_data->topicName->lenstring.data);

    rt_kprintf("mqtt recv_len:%d\n", msg_data->message->payloadlen);
    for(len = 0; len < msg_data->message->payloadlen; len ++)
    {
        rt_kprintf("%02x ", data[len]);
    }
    rt_kprintf("\r\n");
	}
#endif

    one_packet = (char *)msg_data->message->payload;
    total_len = msg_data->message->payloadlen;
    while(gagent_get_one_packet(one_packet, (int *)&data_len, &len_num, total_len) == RT_EOK)
    {
        memcpy(&cmd, one_packet + 6, 2);
        cmd = ntohs(cmd);
        
        gagent_dbg("mqtt_cmd:%x\n", cmd);
        switch(cmd)
        {
            case 0x10:                     //log on/off
            break;
            
            case 0x90:                     //trans data
            case 0x93:
                rc = mqtt_trans_data(cloud, one_packet);
            break;

            case 0x210:                 //app number change

            break;

            case 0x211:                 //push ota
                rc = mqtt_push_ota(cloud, one_packet);
            break;

            default:
            break;
        }
        one_packet += (data_len + len_num + HEAD_LEN);
        total_len -= (data_len + len_num + HEAD_LEN);

        if(rc < RT_EOK)
            break;
    }
}


static void gagent_mqtt_connect_callback(MQTTClient *c)
{
    gagent_dbg("%s\n", __FUNCTION__);
}

static void gagent_mqtt_online_callback(MQTTClient *c)
{
    gagent_dbg("%s\n", __FUNCTION__);

}

static void gagent_mqtt_offline_callback(MQTTClient *c)
{
    gagent_dbg("%s\n", __FUNCTION__);

}

static int gagent_mqtt_client_publish(const char *topic, const char *msg, size_t msg_len)
{
    int rc = RT_EOK;
    MQTTMessage message;

    message.qos = QOS0;
    message.retained = 0;
    message.payload = (void *)msg;
    message.payloadlen = msg_len;

    rc = MQTTPublish(&gagent_mqtt, topic, &message);

    return rc;
}

int gagent_mqtt_send_packet(cloud_st *cloud, rt_uint8_t action, rt_uint8_t *buf, rt_uint16_t buf_len)
{
    int rc = RT_EOK;
    char topic[128];
    
    memset(topic, 0, sizeof(topic));
    rt_snprintf(topic, sizeof(topic), "dev2app/%s", cloud->con->did);
    gagent_dbg("pub_topic:%s\n", topic);

    memset(cloud->send_buf, 0, sizeof(cloud->send_buf));
    cloud->send_len = 0;

    cloud->send_len = gagent_set_one_packet(cloud->send_buf, action, buf, buf_len);

#ifdef MQTT_SEND_DEBUG
	{
    uint32_t i;
    rt_kprintf("mqtt send_len:%d\n", cloud->send_len);
    for(i = 0; i < cloud->send_len; i ++)
    {
        rt_kprintf("%02x ", cloud->send_buf[i]);
    }
    rt_kprintf("\r\n");
	}
#endif

    rc = gagent_mqtt_client_publish(topic, cloud->send_buf, cloud->send_len);
    return rc;
}

int gagent_mqtt_init(cloud_st *cloud)
{
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
 
		RT_ASSERT(cloud != RT_NULL);

    memset(&gagent_mqtt, 0, sizeof(gagent_mqtt));
    
    gagent_mqtt.host = cloud->mqtt_server;
    gagent_mqtt.port = cloud->mqtt_port;

    
    memcpy(&gagent_mqtt.condata, &data, sizeof(MQTTPacket_connectData));
    gagent_mqtt.condata.keepAliveInterval = 30;
    gagent_mqtt.condata.MQTTVersion = 3;
    gagent_mqtt.condata.cleansession = 1;
    gagent_mqtt.condata.clientID.cstring = cloud->con->did;
    gagent_mqtt.condata.username.cstring = cloud->con->did;
    gagent_mqtt.condata.password.cstring = cloud->con->passcode;
    
    gagent_mqtt.buf_size = gagent_mqtt.readbuf_size = 1024;
    gagent_mqtt.buf = malloc(gagent_mqtt.buf_size);
    gagent_mqtt.readbuf = malloc(gagent_mqtt.readbuf_size);
    if(!(gagent_mqtt.readbuf && gagent_mqtt.readbuf))
    {
        gagent_err("mqtt malloc failed!\n");
        return -RT_ENOMEM;
    }

    gagent_mqtt.connect_callback = gagent_mqtt_connect_callback;
    gagent_mqtt.online_callback = gagent_mqtt_online_callback;
    gagent_mqtt.offline_callback = gagent_mqtt_offline_callback;

    memset(cloud->sub_topic[0], 0, sizeof(cloud->sub_topic[0]));
    snprintf(cloud->sub_topic[0], 128, "ser2cli_res/%s", cloud->con->did);
    gagent_mqtt.messageHandlers[0].topicFilter = cloud->sub_topic[0];
    gagent_mqtt.messageHandlers[0].callback = gagent_mqtt_callback;

    memset(cloud->sub_topic[1], 0,sizeof(cloud->sub_topic[1]));
    snprintf(cloud->sub_topic[1], 128, "app2dev/%s/#", cloud->con->did);
    gagent_mqtt.messageHandlers[1].topicFilter = cloud->sub_topic[1];
    gagent_mqtt.messageHandlers[1].callback = gagent_mqtt_callback;

    gagent_mqtt.defaultMessageHandler = gagent_mqtt_callback;
    
    gagent_dbg("host:%s port:%d\n", gagent_mqtt.host, gagent_mqtt.port);
    gagent_dbg("clientID:%s username:%s password:%s\n", 
                    gagent_mqtt.condata.clientID.cstring, 
                    gagent_mqtt.condata.username.cstring,
                    gagent_mqtt.condata.password.cstring);
    gagent_dbg("topic:%s\n", gagent_mqtt.messageHandlers[0].topicFilter);
		
    return paho_mqtt_start(&gagent_mqtt);
}

