@[toc]
> UIP协议栈是属于极简版本的TCP/IP协议栈，比LWIP协议栈还精简，本文章只提供关键部分的参考历程还有很多东西，需要你自己去看文章去学习的，不过有个前提你得懂TCP/IP协议栈和计算机网络知识，比如学过LWIP啥的，要不然你看不懂的

# 这里边有相当好的 [UIP 文档资料，文档位置在仓库的UIP/uip doc ，括号内是仓库地址（https://gitee.com/free-people-in-time-and-space/net-work-learn-note.git ）
# 一：TCP Server
## 1.1 UIP与底层MAC硬件外设的接口

```c
#include "tapdev.h"
#include "uip.h"
#include "uip_arp.h"

/*MX6800 Ethernet MAC RX TX BUF*/
__attribute__((aligned(4))) char ob_eth_rx_buf[1518];
__attribute__((aligned(4))) char ob_eth_tx_buf[1518];

/*MX6800 Ethernet Variable*/
struct eth_device *ob_ethernet_dev = NULL;





/**
 * @brief config ethernet mac
 * 
 * @param mac_addr 
 * @return unsigned char 0:OK
 *                       1:Failed!
 */
unsigned char tapdev_init(unsigned char *mac_addr)
{

    /*---------------------------Ethernet MAC + PHY chip Init-------------------------*/

    eth_init();
    ob_ethernet_dev = eth_get_dev();
    if (!eth_is_active(ob_ethernet_dev))
    {
        UART1_Send_String("[tapdev_init] eth gmac not initiated\r\n");
        return 1;
    }

    /*set Ethernet mac address*/
    if (eth_write_hwaddr(ob_ethernet_dev, mac_addr) == -1)
    {
        UART1_Send_String("[tapdev_init] eth_write_hwaddr set mac addr ob_ethernet_dev failed! \r\n");
    }

    /*---------------------------Ethernet MAC + PHY chip Init-------------------------*/

    return 0;
}



/**
 * @brief from MX6800 Ethernet MAC read data
 *
 * @return uint16_t data
 */
uint16_t tapdev_read(void)
{
    int eth_rxlen = 0;

    eth_rxlen = eth_recv(ob_ethernet_dev, ob_eth_rx_buf); /* by runsheng */
    memcpy(uip_buf, ob_eth_rx_buf, eth_rxlen);
    memset(ob_eth_rx_buf, 0, 1518);

    return eth_rxlen;
}



/**
 * @brief from MX6800 Ethernet MAC send data
 *
 */
void tapdev_send(void)
{
    eth_send(ob_ethernet_dev, (void *)uip_buf, uip_len);
}
```

## 1.2 UIP协议栈初始化配置（包含UDP广播配置，TCP server，TCPclinet 删掉uip_liseten（）就可以了）

```c
/**
 * @brief uip config
 *      set ip netmask router's IP address. mac addr and config ethernet mac
 *
 */
void uip_config(void)
{

    unsigned char mac_addr[6] = {0xB8, 0xAE, 0x1D, 0x00, 0x01, 0x00};

    /*config UIP MAC ADDR*/
    for (unsigned char i = 0; i < 6; i++)
       uip_ethaddr.addr[i] = mac_addr[i];


    uip_ipaddr_t ipaddr, remote_ip;

    /*config ethernet mac*/
    while (tapdev_init(mac_addr))
    {
        UART1_Send_String("MX6800 Ehthernet MAC Init Error! \r\n");

        mdelay(200);
    }

    // uIP初始化
    uip_init();

    /* ARP table initialize. */
    uip_arp_init();

    /*-----------UIP-TCP-Server---------------------*/
    // 设置本地设置IP地址
    uip_ipaddr(ipaddr, 192, 168, 1, 137);
    uip_sethostaddr(ipaddr);

    // 设置网关IP地址(其实就是你路由器的IP地址) (可设置，可不设置)
//    uip_ipaddr(ipaddr, 192, 168, 1, 1);
//    uip_setdraddr(ipaddr);

    // 设置网络掩码
    uip_ipaddr(ipaddr, 255, 255, 255, 0);
    uip_setnetmask(ipaddr);

    // 设置 远程客户端IP为192.168.1.100
    uip_ipaddr(&remote_ip, 192, 168, 1, 100);
    uip_connect(&remote_ip, htons(7090)); /*连接到远程客户端的通信端口为7090 */

    /* We start to listen for connections on TCP port 8090. */
    uip_listen(HTONS(8090));

    /*-----------UIP-TCP-Server---------------------*/

#if UIP_UDP

   /*Broadcast IP ADDR*/
   uip_ipaddr_t Broadcast_addr;
   
   struct uip_udp_conn *udp_conn;

   // UDP广播地址，本局域网中的所有设备，修改成特定IP就可以是UDP单点通信
   uip_ipaddr(&Broadcast_addr, 255, 255, 255, 255);

    /*UDP 广播端口1212*/
   udp_conn = uip_udp_new(&Broadcast_addr, HTONS(1212));

   if (udp_conn == NULL)
   {
       UART1_Send_String("Failed to create UDP connection! \r\n");
   }
   else
   {
        uip_udp_bind(udp_conn, HTONS(3956));

   }
   

#endif

}

```

## 1.3 TCP数据处理中心UIP_APPCALL定义的函数中
	//定义应用程序回调函数 
	#ifndef UIP_APPCALL
	#define UIP_APPCALL tcp_demo_appcall //定义回调函数为 tcp_demo_appcall 
	#endif
```c
char tcp_server_databuf[1500]; // 发送数据缓存


/**
 * @brief 这是一个TCP 服务器应用回调函数。
 *  该函数通过UIP_APPCALL(tcp_demo_appcall)调用,实现Web Server的功能.
 * 当uip事件发生时，UIP_APPCALL函数会被调用,根据所属端口(1200),确定是否执行该函数。
 * 例如 : 当一个TCP连接被创建时、有新的数据到达、数据已经被应答、数据需要重发等事件
 */
void tcp_server_demo_appcall(void)
{

    // 连接终止
    if (uip_aborted())
    {
        uip_log("TCP_Server Aborted!\r\n"); // 打印log
    }

    // 连接超时
    if (uip_timedout())
    {

        uip_log("TCP_Server Timeout!\r\n"); // 打印log
    }

    // 连接关闭
    if (uip_closed())
    {

        uip_log("TCP_server CLOSED!\r\n"); // 打印log
    }

    // 连接成功
    if (uip_connected())
    {
        uip_log("TCP_Server Connected OK!\r\n"); // 打印log

    }

    // 发送的数据成功送达
    if (uip_acked())
    {
        // uip_log("TCP_Server ACK OK!\r\n");

    }
    else
    {
        /*数据发送失败*/
        // uip_log("TCP_Server NOT ACK!\r\n");
    }

    /* 接收到一个新的TCP数据包，收到客户端发过来的数据*/
    if (uip_newdata())
    {

        #if (NET_UPGRADE == 1)
            // memset(&vendor_response, 0, sizeof(vendor_response));
            // memset(&vendor_request, 0, sizeof(vendor_request));

            memcpy(&vendor_request, uip_appdata, uip_len);

            if (PROTOCOL_REQUEST_MAGIC == vendor_request.header.nMagic)
            {

                protocol_handle_request(&vendor_request, &vendor_response);

            }
            else
            {
                uip_log("Recive APP Bin data ERROR! \r\n");
            }

            uip_len = 0;
        #else
            memset(tcp_server_databuf, 0, 1500);

            memcpy((char *)tcp_server_databuf,
                   (char *)uip_appdata,
                   uip_len);
            UART1_Send_String("[TCP RX]:");
            UART1_Send_String(tcp_server_databuf);
            UART1_Send_String("\r\n");
        #endif

    }


    // 当需要重发、新数据到达、数据包送达、连接建立时，通知uip发送数据
    if (uip_rexmit() || uip_newdata() || uip_acked() || uip_connected() || uip_poll())
    {
        
        #if (NET_UPGRADE == 1)
            uip_send(&vendor_response, vendor_response.buf.len);
            while ( !uip_acked() )
            {
                /* wait sen data ok! */
            }
        #else
            // 发送TCP数据包
            uip_send("WLS TCP_client Connected Successfully!\r\n", 
                sizeof("WLS TCP_client Connected Successfully!\r\n")); 
        #endif
    }
}

/**
 * @brief TCP应用接口函数(UIP_APPCALL)
 *  完成TCP服务(包括server和client)
 */
void tcp_demo_appcall(void)
{

    switch (uip_conn->lport) // 本地监听端口80和1200
    {
        case HTONS(8090):
            tcp_server_demo_appcall();
            // uip_log("tcp_demo_appcall Local 8080 TCP Server!! \r\n");
    }

#ifdef TCP_Client

    /*tcp client use */
    switch (uip_conn->rport) // 远程连接8080端口
    {
        case HTONS(8080): // 一旦有来自远程主机的信息，就调用此函数
            // tcp_client_demo_appcall();
            // uip_log("tcp_demo_appcall Remote 8080 TCP Client!! \r\n");
            break;
    }

#endif

}

```

# 二：UDP数据处理
>#define UIP_UDP_APPCALL udp_appcall

```c
char message[] = "UDP Broadcast 6666! \r\n";

char udp_server_databuf[1500]; // 发送数据缓存


/**
 * @brief 定义应用层的 UDP 回调函数
 *      被UIP_UDP_APPCALL调用
 * 
 */
void udp_appcall(void)
{

    /*udp rx data*/
    if (uip_newdata())
    {
        #if(USE_GVCP==1)
            gvcp_discover_callback();
        #else
            char *data = (char *)uip_appdata;
            int len = uip_datalen();

            memset(udp_server_databuf, 0, 1500);

            memcpy((char *)udp_server_databuf,
                    (char *)uip_appdata,
                    uip_len);

            // 打印接收到的数据
            UART1_Send_String("[UDP_RX]:");
            UART1_Send_String(udp_server_databuf);
            UART1_Send_String("\r\n");
        #endif
    }


    // 当需要重发、新数据到达、数据包送达,通知uip发送数据
    if (uip_rexmit() || uip_newdata() || uip_poll())
    {
        #if(USE_GVCP==1)

        #else

            // 将数据复制到 uIP 的应用层数据缓冲区
            memcpy(uip_appdata, message, sizeof(message));

            // 发送数据
            uip_udp_send(sizeof(message));  // 发送的字节数

        #endif
        
    }

}

```
---
# Tips : **以上只是实际应用例程（基于我们自研芯片移植的UIP协议栈，其他芯片用需要修改），前提基础是了解计算机网络基础知识和TCP/IP协议栈的知识，还有UIP协议栈，文章开头有UIP协议栈文档，学协议栈一定要看看那个文章（强烈推荐先看看这两篇论文uIP+TCP_IP协议栈在51系列单片机上的应用.pdf，uIP协议栈在DSP声信号采集阵列上的应用.pdf，基于51的有点老不过可以对UIP有个大概的了解，后边的基于DSP的虽然是核技术研究所的但是写的不比计算机信息化研究所差！ 比好多计算机专业的研究生写的都好！）**
----
通过百度网盘分享的文件：UIP协议栈
链接：https://pan.baidu.com/s/16tV_mU5NX5lqvXaFEUy2iw?pwd=1234 
提取码：1234