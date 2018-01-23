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
#include "gagent_def.h"

int gagent_add_pkcs(char *src, int len)
{    
    char pkcs[16];
    int i, cs = 16 - len % 16;    

    rt_memset(pkcs, 0, sizeof(pkcs));   

    for(i = 0; i < cs; i ++ )
        pkcs[i] = cs;    

    rt_memcpy(src + len, pkcs, cs);
    
    return (len + cs);
}

rt_uint16_t gagent_parse_rem_len(const rt_uint8_t *buf)
{
    rt_uint16_t multiplier = 1;
    rt_uint16_t value = 0;
    rt_uint8_t digit;

    do
    {
        digit = *buf;
        value += (digit & 0x7F) * multiplier;
        multiplier *= 0x80;
        buf++;
    }while((digit & 0x80) != 0);

    return value;
}

rt_uint8_t gagent_num_rem_len_bytes(const uint8_t* buf)
{
    rt_uint8_t num_bytes = 1;

    if((buf[0] & 0x80) == 0x80)
    {
        num_bytes++;
        if((buf[1] & 0x80) == 0x80)
        {
            num_bytes ++;
            if ((buf[2] & 0x80) == 0x80)
                num_bytes ++;
        }
    }
    return num_bytes;
}

rt_uint8_t gagent_get_rem_len(int length, char *buf)
{
    rt_uint8_t cnt = 0, digit;

    do
    {
        digit = length % 128;
        length /= 128;

        if (length > 0) 
        digit = digit | 0x80;

        buf[cnt] = digit;
        cnt ++;
    }while(length > 0);

    return cnt;
}

int gagent_get_one_packet(char *packet, int *data_len, rt_uint8_t *len_num, int remain_len)
{
    char *index;
    rt_uint16_t len;
    rt_uint8_t length_len;

    if(packet == NULL || data_len == NULL || len_num == NULL || remain_len <= 0) 
        return -1;

    index = packet;

    len = gagent_parse_rem_len((const uint8_t *)index + 4);
    length_len = gagent_num_rem_len_bytes((const uint8_t*)index + 4);

    *data_len = len;
    *len_num = length_len;

    return RT_EOK;
}

int gagent_set_one_packet(char *packet, uint8_t action, uint8_t *buf, uint32_t buf_len)
{
    char length_bytes[4];
    rt_uint32_t packet_len;
    rt_uint8_t length_num, i;
    char *index = packet;

    //head
    *index ++ = 0x00;
    *index ++ = 0x00;
    *index ++ = 0x00;
    *index ++ = 0x03;

    //
    packet_len = buf_len + 3;           //flag + cmd * 2
    if(buf && (buf_len > 0))
        packet_len += 1;                //action

    memset(length_bytes, 0, sizeof(length_bytes));
    length_num = gagent_get_rem_len(packet_len, length_bytes);
    for(i = 0; i < length_num; i ++)
        *index ++ = length_bytes[i];

    //flag
    *index ++ = 0x00;

    //
    *index ++ = 0x00;
    *index ++ = 0x91;

    *index ++ = action;

    if(buf && buf_len > 0)
    {
        memcpy(index, buf, buf_len);
        index += buf_len;
    }

    return (index - packet);
}

int gagent_strtohex(char *dst, char *src, int len)
{
    char h1,h2;
    int i;

    for(i = 0; i< len; i += 2)
    {   
        h1 = src[i];
        if((h1 >= 'A') && (h1 <= 'F'))
            h1 = h1 - 'A' + 10;
        else if((h1 >= 'a') && (h1 <= 'f'))
            h1 = h1 - 'a' + 10;
        else
            h1 = h1 - '0';

        h2 = src[i + 1];
        if((h2 >= 'A') && (h2 <= 'F'))
            h2 = h2 - 'A' + 10;
        else if((h2 >= 'a') && (h2 <= 'f'))
            h2 = h2 - 'a' + 10;
        else
            h2 = h2 - '0';

        dst[i / 2] = ((h1 << 4) & 0xf0) + (h2 & 0x0f);
    }

    return len / 2;
}


