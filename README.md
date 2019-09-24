# PPP device #

## 1. 简介 ##

 PPP(Piont to Piont Protocl) 点对点协议，相对于使用AT命令连接网络，PPP 则免去操作众多的AT命令及其解析。使用 PPP device 软件包，PPP连接成功后，便获取 IP 地址和 DNS ；之后就可以像使用网卡一样使用 2G/3G/4G 模块，剩下的工作内容就是使用 Socket 套接字，结合 TCP 和 UDP 协议实现数据的传输，操作会变得更加灵活轻巧，也可以轻巧的移植，获取更高的传输质量，提高网络吞吐量。PPP 是 lwIP 协议的一部分，原生 lwIP 即可支持，无需太多操作即可实现联网。

目前 PPP 功能仅支持 Luat Air720 模块，后续会接入更多 2G/3G/4G 模块。

### 1.1. 目录结构 ###

| 名称 | 说明 |
| ---- | ---- |
| src | PPP device 实现源码目录 |
| inc | PPP device 头文件目录 |
| sample | 不同设备示例文件目录 |
| class | 不同设备针对 PPP 组件的移植适配目录 |
| class/air720 | Air720 设备针对 PPP 组件的移植目录，实现 PPP拨号上网 功能 |

### 1.2 许可证 ###

ppp_device 软件包遵循 Apache-2.0 许可，详见 LICENSE 文件。

### 1.3 依赖 ###

- RT_Thread 4.0.0+
- lwIP 组件（ ppp 功能）

## 2. 获取方式 ##

**PPP 软件包相关配置选项介绍**


```c
--- PPP DEVICE: lwIP PPP porting for different device
    [ ]   Enable debug log output
    [ ]   Enbale authorize connect feature
    [*]   Enable lin status detect feature
    (1)     Link status detecct timeout
          Select modem type (Luat Air720)  --->
          Select network operator (china mobile)  --->
    [*]   nable air720 ppp device samples
    (a0)    air720 device name
    (uart3) air720 ppp device uart name
          Version (latest)  --->
```
- **Enable debug log output:** 开启调试日志功能
- **Enbale authorize connect feature:** 开启身份认证功能
- **Enable lin status detect feature:** PPP链路连接监控，检测链路连接正常；设置为 0 则不开启链路监控；
- **Select modem type:** 模块选择
- **Select network operator:** 网络运营商选择
- **Enable air720 ppp device samples:**  选择模块后会提示的模块使用示例
- **air720 device name:** 模块名称，注册的网卡名称将会与该名称一致
- **air720 ppp device uart name:** 模块使用的串口
- **Version:** 软件包版本号

## 3. 使用方式

ppp device 软件包初始化函数如下所示：

PPP 功能启动函数，该函数自动调用；没有调用停止函数前，不可再次调用。

```c
int ppp_air720_start(void);
```

* 初始化模块，获取模块基础信息；
* 模块拨号，模块进入 PPP 模式；
* 注册 netdev 设备，接入标准网络框架；

PPP 功能停止函数，该函数可以退出 PPP 模式。

```c
int ppp_air720_stop(void);
```

* 退出 PPP 模式，模块退出拨号模式；
* 解注册 netdev 设备；

模块上电后，自动初始化流程如下：

```c
 \ | /
- RT -     Thread Operating System
 / | \     4.0.2 build Sep 23 2019
 2006 - 2019 Copyright by rt-thread team
lwIP-2.0.2 initialized!
[I/sal.skt] Socket Abstraction Layer initialize success.
[I/ppp.chat] (uart3) has control by modem_chat.
[I/ppp.chat] chat success
[I/ppp.dev] (uart3) is used by ppp_device.
msh />[I/ppp.dev] ppp connect successful.
```

设备上电初始化完成，模块提示拨号成功，然后可以在 FinSH 中输入命令 `ifconfig` 查看设备 IP 地址、MAC 地址等网络信息，如下所示：

```shell
msh />ifconfig
network interface device: a0 (Default)           ## 设备名称
MTU: 1500                                        ## 网络最大传输单元
MAC: 95 45 68 39 68 52                           ## 设备 MAC 地址
FLAGS: UP LINK_UP INTERNET_DOWN DHCP_DISABLE     ## 设备标志
ip address: 10.32.76.151                         ## 设备 IP 地址
gw address: 10.64.64.64                          ## 设备网关地址
net mask  : 255.255.255.255                      ## 设备子网掩码
dns server #0: 114.114.114.114                   ## 域名解析服务器地址0
dns server #1: 120.196.165.7                     ## 域名解析服务器地址1
```

获取 IP 地址成功之后，如果开启 Ping 命令功能，可以在 FinSH 中输入命令 `ping + 域名地址` 测试网络连接状态， 如下所示：

```shell
msh />ping www.baidu.com
60 bytes from 183.232.231.172 icmp_seq=0 ttl=55 time=84 ms
60 bytes from 183.232.231.172 icmp_seq=1 ttl=55 time=78 ms
60 bytes from 183.232.231.172 icmp_seq=2 ttl=55 time=78 ms
60 bytes from 183.232.231.172 icmp_seq=3 ttl=55 time=78 ms
```

`ping` 命令测试正常说明 PPP DEVICE 设备网络连接成功，之后可以使用 SAL（套接字抽象层） 抽象出来的标准 BSD Socket APIs 进行网络开发（MQTT、HTTP、MbedTLS、NTP、Iperf 等）。

## 4. 注意事项

* 一般的SIM卡因为只能从运营商网络获取内网地址，所以不能实现服务器相关功能。
* 现阶段只支持一个设备通过 PPP 连接网络。
* 现阶段只支持使用 UART 方式进行数据传输，后续会添加通过 USB 连接 PPP 的方式。

## 5. 联系方式

xiangxistu

QQ：1254605504

email: liuxianliang@rt-thread.com