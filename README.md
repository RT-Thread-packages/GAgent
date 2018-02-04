# GAgent #
## GAgent of Gizwits in RT-Thread ##
## 概述 ##
GAgent是机智云物联网整体解决方案中可入网设备（如WiFi模组、GPRS模组）接入部分，是一套运行在可入网模组（如WiFi模组、GPRS模组）上，使用机智云协议接入机智云平台，并与手机APP通信的程序。  

GAgent of Gizwits in RT-Thread并非直接移植机智云开源SDK，而是基于机智云开源协议编写，并结合RT-Thread开源webclient、Paho MQTT、TinyCrypt等packages，完成机智云物联接入。当前已支持有线、无线WiFi等网络设备使用，其他功能将会不断更新维护。  

开发者使用GAgent of Gizwits in RT-Thread，配合RT-Thread的env工具和menuconfig，可快速将设备接入移植机智云平台。该packages与硬件设备无关，开发者只需完成RT-Thread移植，将设备连接互联网后即可使用。应用GAgent of Gizwits in RT-Thread后，开发者可更多关注设备本身的功能开发，而无需关心设备与机智云平台通讯交互过程。  

## 许可证 ##
GAgent of Gizwits in RT-Thread与RT-Thread一致，遵循GPLv2+许可证，详细请参照RT-Thread。

## 使用示例 ##
GAgent使用示例请参考example目录下gagent_cloud_demo.c文件，详情可阅读[《GAgent of Gizwits in RT-Thread使用手册》](./docs/README.md)。  

## 相关链接 ##
1. 机智云开发者中心：  
[https://dev.gizwits.com/zh-cn/developer/](https://dev.gizwits.com/zh-cn/developer/)  

2. 机智云开源SDK：  
[https://github.com/gizwits/Gizwits-GAgent](https://github.com/gizwits/Gizwits-GAgent)  

3. Env工具获取：`RT-Thread官网`->`资源`->`下载`  
[https://www.rt-thread.org/page/download.html](https://www.rt-thread.org/page/download.html)  

4. Env工具使用手册：`RT-Thread官网`->`文档`->`用户手册`->`RT-Thread工具手册`  
[https://www.rt-thread.org/document/site/zh/5chapters/01-chapter_env_manual/#env](https://www.rt-thread.org/document/site/zh/5chapters/01-chapter_env_manual/#env)  