/*
 * File      : gagent_httpc.c
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
#include "aes.h"

#define     MAX_HTTPC_URL_LEN           1024
#define     MAX_HTTPC_RECV_LEN          2048

struct cloud_httpc
{
    char *recv_buf;
    int recv_len;
};

static struct cloud_httpc httpc;

typedef int (*WEBCLIENT_CUSTOM_RESPONSE_CB)(size_t total, size_t offset, char *from, size_t from_len);


static int webclient_common(int method, const char *URI, 
                            const char * data, size_t data_sz,
                            WEBCLIENT_CUSTOM_RESPONSE_CB webclient_custom_response_cb)
{
    struct webclient_session *session = RT_NULL;
    char *header = RT_NULL, *header_ptr;
    size_t header_sz = 0;
    int rc = WEBCLIENT_OK;
    int length = 0;
    size_t total = 0, offset = 0;
    
    rt_uint8_t *buffer = RT_NULL;

    header = (char *)rt_malloc(WEBCLIENT_HEADER_BUFSZ);
    if(RT_NULL == header)
    {
        gagent_err("malloc failed!\n");
        rc = -WEBCLIENT_NOMEM;
        goto __exit;
    }
    rt_memset(header, 0, WEBCLIENT_HEADER_BUFSZ);

    header_ptr = header;
    header_ptr += rt_snprintf(header_ptr,
                            WEBCLIENT_HEADER_BUFSZ - (header_ptr - header),
                            "Content-Type: application/x-www-form-urlencoded;charset:UTF-8\r\n");
                
    if(data)
    {
        header_ptr += rt_snprintf(header_ptr, 
                                WEBCLIENT_HEADER_BUFSZ - (header_ptr - header), 
                                "Content-Length:%d\r\n", strlen(data));
    }
                            
    header_sz = header_ptr - header;
    session = webclient_open_custom(URI, method, header, header_sz, data, data_sz);
    if(RT_NULL == session)
    {
        gagent_err("webclient_open_custom failed!\n");
        rc = -WEBCLIENT_OK;
        goto __exit;
    }
    gagent_dbg("response:%d\n",session->response);

    buffer = (rt_uint8_t *)rt_malloc(WEBCLIENT_RESPONSE_BUFSZ);
    if(RT_NULL == buffer)
    {
        gagent_err("malloc failed!\n");
        rc = -WEBCLIENT_NOMEM;
        goto __exit;
    }

    if(session->chunk_sz > 0)
    {
        gagent_dbg("chunk_sz:%d\n", session->chunk_sz);
        total = session->chunk_sz;
    }
    else if(session->content_length > 0)
    {
        gagent_dbg("content_length:%d content_length_remainder:%d\n", session->content_length, session->content_length_remainder);
        total = session->content_length;
    }
    
    while(total != offset)
    {
        memset(buffer, 0, WEBCLIENT_RESPONSE_BUFSZ);
        length = webclient_read(session, buffer, WEBCLIENT_RESPONSE_BUFSZ);
        if(length <= 0)
        {
            rc = length;
            break;
        }
        
        if(webclient_custom_response_cb)
            webclient_custom_response_cb(total, offset, (char *)buffer, (int)length);

        offset += length;
    }

__exit:
    if(RT_NULL != buffer)
    {
        rt_free(buffer);
        buffer = RT_NULL;
    }

    if(RT_NULL != header)
    {
        rt_free(header);
        header = RT_NULL;
    }

    if(RT_NULL != session)
    {
        webclient_close(session);
        session = RT_NULL;
    }
    return rc; 
}

int webclient_get(const char *URI, WEBCLIENT_CUSTOM_RESPONSE_CB webclient_custom_response_cb)
{
    return webclient_common(WEBCLIENT_GET, URI, NULL, 0, webclient_custom_response_cb);
}

int webclient_post(const char *URI, const char * data, size_t data_sz,
                            WEBCLIENT_CUSTOM_RESPONSE_CB webclient_custom_response_cb)
{
    return webclient_common(WEBCLIENT_POST, URI, data, data_sz, webclient_custom_response_cb);
}

int gagent_cloud_httpc_cb(size_t total, size_t offset, char *from, size_t from_len)
{
    extern cloud_st *cloud;
    
    gagent_dbg("total:%d offset:%d len:%d\n", total, offset, from_len);
    gagent_dbg("buf:%s\n", from);
    
    rt_memset(httpc.recv_buf, 0, MAX_HTTPC_RECV_LEN);
    rt_memcpy(httpc.recv_buf, from, from_len);
    httpc.recv_len = from_len;

    return RT_EOK;
}

int gagent_cloud_register(cloud_st *cloud)
{
    int rc = RT_EOK;
    int content_len, aes_len, i;
    //
    char *content = RT_NULL;
    char *url = RT_NULL;
    char *ptr = RT_NULL;
    
    uint8_t aes_key[16];
    char *aes_buf = RT_NULL;
    aes_context *aes_ctx = RT_NULL;

    content = (char *)rt_malloc(256);
    if(RT_NULL == content)
    {
        gagent_err("malloc faield!\n");
        rc = -RT_ENOMEM;
        goto __exit;
    }

    aes_buf = (char *)rt_malloc(256);
    if(RT_NULL == aes_buf)
    {
        rc = -RT_ENOMEM;
        goto __exit;
    }

    //data
    rt_memset(content, 0, 256);
    rt_snprintf((char *)content, 256, "mac=%02x%02x%02x%02x%02x%02x&passcode=%s&type=%s", \
                cloud->con->mac[0], cloud->con->mac[1], cloud->con->mac[2], cloud->con->mac[3], cloud->con->mac[4], cloud->con->mac[5], \
                cloud->con->passcode, "normal");

    content_len = strlen(content);

    rt_memset(aes_key, 0, sizeof(aes_key));
    gagent_strtohex((char *)aes_key, cloud->con->pk_secret, strlen(cloud->con->pk_secret) > 32 ? 32 : strlen(cloud->con->pk_secret));
    gagent_dbg("content:%s, pk_secret:%s\n", content, cloud->con->pk_secret);

    aes_ctx = (aes_context *)rt_malloc(sizeof(aes_context));
    if(RT_NULL == aes_ctx)
    {
        rc = -RT_ENOMEM;
        goto __exit;
    }

    rt_memset(aes_ctx, 0, sizeof(aes_context));
    aes_setkey_enc(aes_ctx, aes_key, 128);
    //
    rt_memset(aes_buf, 0, 256);
    
    content_len = strlen(content);
    if(content_len % 16)
        content_len = gagent_add_pkcs(content, content_len);
    
    aes_len = 0;
    for(i = 0; i < content_len; i += 16)
    {
        aes_crypt_ecb(aes_ctx, AES_ENCRYPT, (uint8_t *)content + i, (uint8_t *)aes_buf + i);
        aes_len += 16;
    }

    ptr = content;
    rt_memset(ptr, 0, 256);
    
    strcpy(ptr, "data=");
    ptr += strlen("data=");
    
    if(aes_len > 0)
    {
        for(i = 0; i < aes_len; i ++)
        {
            ptr += rt_snprintf(ptr, 256 - (ptr - content), "%02x", aes_buf[i]);
        }
    }

    gagent_dbg("content:%s\n", content);
    
    url = (char *)rt_malloc(MAX_HTTPC_URL_LEN);
    if(RT_NULL == url)
    {
        rc = -RT_ENOMEM;
        goto __exit;
    }

    ptr = url;
    memset(ptr, 0, MAX_HTTPC_URL_LEN);
    
    //url
    ptr += rt_snprintf(ptr, MAX_HTTPC_URL_LEN - (ptr - url), "http://%s:%s", G_SERVICE_DOMAIN, G_SERVICE_PORT);
    ptr += rt_snprintf(ptr, MAX_HTTPC_URL_LEN - (ptr - url), "/dev/%s/device", cloud->con->pk);
    
    gagent_dbg("url:%s\n", url);

    httpc.recv_buf = RT_NULL;
    httpc.recv_buf = (char *)rt_malloc(MAX_HTTPC_RECV_LEN);
    if(RT_NULL == httpc.recv_buf)
    {
       rc = -RT_ENOMEM;
       goto __exit;
    }
    
    rc = webclient_post((const char *)url, (const char *)content, strlen(content), gagent_cloud_httpc_cb);
    if(rc != RT_EOK)
    {
        gagent_err("weblient_post failed!\n");
        goto __exit;
    }

    //
    rt_memset(content, 0, 256);
    aes_len = gagent_strtohex((char *)content, (char *)httpc.recv_buf, httpc.recv_len);

    rt_memset(aes_ctx, 0, sizeof(aes_context));
    aes_setkey_dec(aes_ctx, aes_key, 128);
    
    rt_memset(aes_buf, 0, 256);
    for(i = 0; i < aes_len; i += 16)
        aes_crypt_ecb(aes_ctx, AES_DECRYPT, (uint8_t *)content + i, (uint8_t *)aes_buf + i);

    gagent_dbg("%s\n", aes_buf);

    if(strstr(aes_buf, "did=") != NULL)
        rt_memcpy(cloud->con->did, aes_buf + strlen("did="), DID_LENGTH);
    
__exit:
    if(RT_NULL != httpc.recv_buf)
    {
        rt_free(httpc.recv_buf);
        httpc.recv_buf = RT_NULL;
    }
    
    if(RT_NULL != url)
    {
        rt_free(url);
        url = RT_NULL;
    }

    if(RT_NULL != aes_ctx)
    {
        rt_free(aes_ctx);
        aes_ctx = RT_NULL;
    }

    if(RT_NULL != aes_buf)
    {
        rt_free(aes_buf);
        aes_buf = RT_NULL;
    }
    
    if(RT_NULL != content)
    {
        rt_free(content);
        content = RT_NULL;
    }

    return rc;
}

int gagent_cloud_provision(cloud_st *cloud)
{
    int rc = RT_EOK;
    int content_len, aes_len, i;

    char *url = RT_NULL;
    char *content = RT_NULL;
    char *ptr = RT_NULL;
    char *ptr_tail = RT_NULL;

    char aes_key[16];
    char *aes_buf = RT_NULL;
    aes_context *aes_ctx = RT_NULL;

    content = (char *)rt_malloc(256);
    if(RT_NULL == content)
    {
        rc = -RT_ENOMEM;
        goto __exit;
    }

    aes_buf = (char *)rt_malloc(256);
    if(RT_NULL == aes_buf)
    {
        rc = -RT_ENOMEM;
        goto __exit;
    }

    //data    
    rt_memset(content, 0, 256);
    strcpy(content, cloud->con->did);

    rt_memset(aes_key, 0, sizeof(aes_key));
    gagent_strtohex((char *)aes_key, cloud->con->pk_secret, strlen(cloud->con->pk_secret) > 32 ? 32 : strlen(cloud->con->pk_secret));

    gagent_dbg("content:%s, pk_secret:%s\n", content, cloud->con->pk_secret);

    aes_ctx = (aes_context *)rt_malloc(sizeof(aes_context));
    if(RT_NULL == aes_ctx)
    {
        rc = -RT_ENOMEM;
        goto __exit;
    }

    rt_memset(aes_ctx, 0, sizeof(aes_context));
    aes_setkey_enc(aes_ctx, (uint8_t *)aes_key, 128);
    //
    rt_memset(aes_buf, 0, 256);

    content_len = strlen(content);
    if(content_len % 16)
        content_len = gagent_add_pkcs(content, content_len);

    aes_len = 0;
    for(i = 0; i < content_len; i += 16)
    {
        aes_crypt_ecb(aes_ctx, AES_ENCRYPT, (uint8_t *)content + i, (uint8_t *)aes_buf + i);
        aes_len += 16;
    }

    ptr = content;
    if(aes_len > 0)
    {
        for(i = 0; i < aes_len; i ++)
        {
            ptr += rt_snprintf(ptr, 256 - (ptr - content), "%02x", aes_buf[i]);
        }
    }
    gagent_dbg("content:%s\n", content);

    url = (char *)rt_malloc(MAX_HTTPC_URL_LEN);
    if(RT_NULL == url)
    {
        rc = -RT_ENOMEM;
        return rc;
    }

    ptr = url;
    rt_memset(url, 0, MAX_HTTPC_URL_LEN);

    //url
    ptr += rt_snprintf(ptr, MAX_HTTPC_URL_LEN - (ptr - url), "http://%s:%s", G_SERVICE_DOMAIN, G_SERVICE_PORT);
    ptr += rt_snprintf(ptr, MAX_HTTPC_URL_LEN - (ptr - url), "/dev/%s/device?", cloud->con->pk);
    ptr += rt_snprintf(ptr, MAX_HTTPC_URL_LEN - (ptr - url), "did=%s", content);
    gagent_dbg("url:%s\n", url);

    //
    rt_memset(cloud->mqtt_server, 0, sizeof(cloud->mqtt_server));
    rt_memcpy(cloud->mqtt_server, G_M2M_DOMAIN, sizeof(G_M2M_DOMAIN));
    //
    cloud->mqtt_port = 0;
    cloud->mqtt_port = atoi(G_M2M_PORT);

    httpc.recv_buf = RT_NULL;
    httpc.recv_buf = (char *)rt_malloc(MAX_HTTPC_RECV_LEN);
    if(RT_NULL == httpc.recv_buf)
    {
        rc = -RT_ENOMEM;
        goto __exit;
    }

    rc = webclient_get((const char *)url, gagent_cloud_httpc_cb);
    if(rc != RT_EOK)
    {
        rt_kprintf("weblient_post failed!\n");
        goto __exit;
    }

    rt_memset(content, 0, 256);
    aes_len = gagent_strtohex(content, httpc.recv_buf, httpc.recv_len);

    rt_memset(aes_ctx, 0, sizeof(aes_context));
    aes_setkey_dec(aes_ctx, (uint8_t *)aes_key, 128);

    rt_memset(aes_buf, 0, 256);
    for(i = 0; i < aes_len; i += 16)
    aes_crypt_ecb(aes_ctx, AES_DECRYPT, (uint8_t *)content + i, (uint8_t *)aes_buf + i);

    gagent_dbg("%s\n", aes_buf);

    //mqtt_server
    ptr = strstr(aes_buf, "host=");
    if(ptr > 0)
    {
        ptr += strlen("host=");
        ptr_tail = strchr(ptr, '&');
        if(ptr_tail > 0)
        {
            rt_memset(cloud->mqtt_server, 0, sizeof(cloud->mqtt_server));
            rt_memcpy(cloud->mqtt_server, ptr, (ptr_tail - ptr));
        }
    }

    //mqtt_port
    ptr = strstr(aes_buf, "port=");
    if(ptr > 0)
    {
        ptr += strlen("port=");

        ptr_tail = strchr(ptr, '&');
        if(ptr_tail > 0)
        {
            *ptr_tail = '\0';

            cloud->mqtt_port = 0;
            cloud->mqtt_port = atoi(ptr);
        }
    }

    gagent_dbg("mqtt_server:%s port:%d\n", cloud->mqtt_server, cloud->mqtt_port);

__exit:
    if(RT_NULL != httpc.recv_buf)
    {
        rt_free(httpc.recv_buf);
        httpc.recv_buf = RT_NULL;
    }

    if(RT_NULL != url)
    {
        rt_free(url);
        url = RT_NULL;
    }

    if(RT_NULL != aes_ctx)
    {
        rt_free(aes_ctx);
        aes_ctx = RT_NULL;
    }

    if(RT_NULL != aes_buf)
    {
        rt_free(aes_buf);
        aes_buf = RT_NULL;
    }

    if(RT_NULL != content)
    {
        rt_free(content);
        content = RT_NULL;
    }

    return rc;
}


int gagent_cloud_check_ota(cloud_st *cloud)
{
    int rc = RT_EOK;
    char *url = RT_NULL;
    char *content = RT_NULL;
    char *ptr = RT_NULL;

    content = (char *)rt_malloc(256);
    if(RT_NULL == content)
    {
        gagent_err("malloc faield!\n");
        rc = -RT_ENOMEM;
        goto __exit;
    }

    //data
    rt_memset(content, 0, 256);
    rt_snprintf((char *)content, 256, "passcode=%s&type=%d&hard_version=%s&soft_version=%s", \
    cloud->con->passcode, GAGENT_HARD_SOC, cloud->con->hard_version, cloud->con->soft_version);

    gagent_dbg("content:%s\n", content);

    url = (char *)rt_malloc(MAX_HTTPC_URL_LEN);
    if(RT_NULL == url)
    {
        rc = -RT_ENOMEM;
        return rc;
    }

    ptr = url;
    rt_memset(url, 0, MAX_HTTPC_URL_LEN);

    //url
    ptr += rt_snprintf(ptr, MAX_HTTPC_URL_LEN - (ptr - url), "http://%s:%s", G_SERVICE_DOMAIN, G_SERVICE_PORT);
    ptr += rt_snprintf(ptr, MAX_HTTPC_URL_LEN - (ptr - url), "/dev/ota/v4.1/update_and_check/%s", cloud->con->did);

    gagent_dbg("url:%s\n", url);

    httpc.recv_buf = RT_NULL;
    httpc.recv_buf = (char *)rt_malloc(MAX_HTTPC_RECV_LEN);
    if(RT_NULL != httpc.recv_buf)
    {
        rc = -RT_ENOMEM;
        goto __exit;
    }

    rc = webclient_post((const char *)url, (const char *)content, strlen(content), gagent_cloud_httpc_cb);
    if(rc != RT_EOK)
    {
        gagent_err("weblient_post failed!\n");
        goto __exit;
    }

__exit:
    if(RT_NULL != httpc.recv_buf)
    rt_free(httpc.recv_buf);

    if(RT_NULL != url)
    rt_free(url);

    if(RT_NULL != content)
    rt_free(content);

    return rc;
}

