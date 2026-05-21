@[toc]

> UIP_APPCALL **TCP的数据**都在这个宏定义的函数里进行数据处理的 
> UDP 数据在#define UIP_UDP_APPCALL udp_appcall 中处理

# 1. TCP通信 客户端（关键配置）

```c

	// 定义 MAC 地址（6 字节）
	static const uint8_t mac_address[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
	// 设置 MAC 地址
	memcpy(ethaddr.addr, mac_address, sizeof(mac_address));
	uip_setethaddr(ethaddr);
    
	uip_ipaddr_t ipaddr;
	uip_init();                          // uIP初始化
	uip_ipaddr(ipaddr, 192, 168, 1, 137); // 设置本地设置IP地址
	uip_sethostaddr(ipaddr);
	uip_ipaddr(ipaddr, 192, 168, 1, 1); // 设置网关IP地址(其实就是你路由器的IP地址)
	uip_setdraddr(ipaddr);
	uip_ipaddr(ipaddr, 255, 255, 255, 0); // 设置网络掩码
	uip_setnetmask(ipaddr);
	
    uip_ipaddr_t ipaddr;
    uip_ipaddr(&ipaddr, 192, 168, 1, 100); // 设置TCP Server IP为192.168.1.100
    uip_connect(&ipaddr, htons(8080));     // 端口为1400
```

# 2. TCP 服务端配置 

```c
    uip_ipaddr_t ipaddr, remote_ip;

	// 定义 MAC 地址（6 字节）
	static const uint8_t mac_address[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
	// 设置 MAC 地址
	memcpy(ethaddr.addr, mac_address, sizeof(mac_address));
	uip_setethaddr(ethaddr);
	
    // uIP初始化
    uip_init();

    /* ARP table initialize. */
    uip_arp_init();

    /*-----------UIP-TCP-Server---------------------*/
    // 设置本地设置IP地址
    uip_ipaddr(ipaddr, 192, 168, 1, 137);
    uip_sethostaddr(ipaddr);

    // 设置网关IP地址(其实就是你路由器的IP地址)
    uip_ipaddr(ipaddr, 192, 168, 1, 1);
    uip_setdraddr(ipaddr);

    // 设置网络掩码
    uip_ipaddr(ipaddr, 255, 255, 255, 0);
    uip_setnetmask(ipaddr);

    // 设置 远程客户端IP为192.168.1.100
    uip_ipaddr(&remote_ip, 192, 168, 1, 100);
    uip_connect(&remote_ip, htons(7090)); /*连接到远程客户端的通信端口为7090 */

    /* We start to listen for connections on TCP port 8090. */
    uip_listen(HTONS(8090));

    /*-----------UIP-TCP-Server---------------------*/
```

# 3. UDP 点播通信

```c

   uip_ipaddr_t remote_addr;
   struct uip_udp_conn *udp_conn;

   // 设置远程主机的 IP 地址为 192.168.1.100
   uip_ipaddr(&remote_addr, 192, 168, 1, 100);

   // 创建到 remote 192.168.1.100:1215 的 UDP 连接
   udp_conn = uip_udp_new(&remote_addr, HTONS(1215));

   if (udp_conn == NULL)
   {
       UART1_Send_String("Failed to create UDP connection! \r\n");
   }
   else
   {
       uip_udp_bind(udp_conn, HTONS(1234)); // 绑定本地端口 1234
   }

```


# 4. UDP 广播通信

UDP 广播需要打开这个宏
> #ifndef UIP_CONF_BROADCAST
> #define UIP_BROADCAST 1
> #else /* UIP_CONF_BROADCAST */
> #define UIP_BROADCAST UIP_CONF_BROADCAST
> #endif /* UIP_CONF_BROADCAST */

```c
   /*Broadcast IP ADDR*/
   uip_ipaddr_t Broadcast_addr;
   
   struct uip_udp_conn *udp_conn;

   // UDP广播地址，本局域网中的所有设备
   uip_ipaddr(&Broadcast_addr, 255, 255, 255, 255);

    /*UDP 广播端口1212*/
   udp_conn = uip_udp_new(&Broadcast_addr, HTONS(0));

   if (udp_conn == NULL)
   {
       UART1_Send_String("Failed to create UDP connection! \r\n");
   }
   else
   {
        uip_udp_bind(udp_conn, HTONS(3956));

   }
```


# 5. UIP_UDP_APPCALL 里边的处理example

```c
    /*udp rx data*/
    if (uip_newdata())
    {
        #if (USE_GVCP==1)
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
        #if (USE_GVCP==1)
    
        #else
    
        // 将数据复制到 uIP 的应用层数据缓冲区
        memcpy(uip_appdata, message, sizeof(message));

        // 发送数据
        uip_udp_send(sizeof(message));  // 发送的字节数
        #endif

    }
```


# 6. TCP数据处理 ，UIP_APPCALL调用的函数

```c
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

        memcpy(&request, uip_appdata, uip_len);

        if (PROTOCOL_REQUEST_MAGIC == request.header.nMagic)
        {
            // uip_log("Recive APP SDK Update!\r\n");
            protocol_handle_request(&request, &response);
        }
        else
        {
            uip_log("Recive APP Bin data ERROR! \r\n");
        }

        uip_send(&response, response.buf.len);

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
