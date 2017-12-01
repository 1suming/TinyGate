[TOC]

----------

## 测试目的

----------

#### Gate设计

Gate是目前整个游戏服务端的reverse proxy中间件的infrastructure abstraction。它的职责是接受clients和yworldserver的连接，保持通讯，使得client和yworldserver只保持**O(1)**条连接，是当前架构中唯一的静态部分(static part)。

同时，针对路由client消息的需求和组播(multicast)后端yworldserver的需求，Gate也尽力保证以**最低的成本**构建消息流模型。

目前服务端Gate拓扑如下图所示：

![topology](http://ozcwwsfj9.bkt.clouddn.com/topology.png)

其中Gate和clients的连接引入**TCP/UDP(RUDP)**协议，对于**性能敏感**的数据包 (如 `TOCLIENT_ACTIVE_OBJECT_MESSAGES` 这种移动同步协议等) 可以走UDP(RUDP)协议通讯(目前yworldgateway的是用[kcp](https://github.com/skywind3000/kcp)实现RUDP)，其他数据包则可以走TCP协议通讯。

**NOTE:** 目前假定通信模型的实现是建立在对底层协议一无所知的前提之上的，这样设计的时候只要引入Adaptor去适配各种协议，到时候就能按需切换.

#### Gateway测试基准

Gateway有两个测试需求

- 存在一定丢包率(Loss rate)的情况下，TCP和UDP(KCP)的性能

- Gateway能承载的最大玩家数、最大包转发率等

前者的基准，可以参照[kcp的技术特性](https://github.com/skywind3000/kcp)和[benchmark](https://github.com/libinzhangyuan/reliable_udp_bench_mark/blob/master/bench_mark.md):

> KCP是一个快速可靠协议，能以比 TCP浪费10%-20%的带宽的代价，换取平均延迟降低 30%-40%，且最大延迟 降低三倍的传输效果。

后者的基准，有一个 ***C100k*** 的指标，即单核单线程的最大并发连接为 **100k个长连接** (目前的Gateway是单线程实现)。 由于受限于业务逻辑处理时间，网卡的吞吐量和并发性能，最大带宽等原因， ***C100k*** 是一个理想值。

根据 ***C100k*** 的基准，可以推算出其他测试基准。 例如承载最大玩家数。如果每个玩家平均每秒交互10个数据包，那么最大可容纳的玩家数即 100k / 10 = 10k 个，其他基准以此类推。

在业界， ***C100k*** 只能说是几年前的基础基准，现在主流的服务器可以跑到 ***C10M*** 甚至 ***C100M***。然而这需要服务器的性能支撑（尤其是内存），我们没有高性能的服务器，而且也没有要跑到 ***C100M*** 的需求， ***C100k*** 可以满足线上需求。

## 准备工作

----------

#### 测试环境

测试环境为两台阿里云KVM架构服务器(天津机房，平均ping延迟为40ms)，gateway部署在`60.28.167,66`的机器上, yworldserver部署在`60.28.167,67`的机器上。

CPU信息如下(8核CPU, 下列给出cpu0的信息)：

```
    yanming@rise-prod-common-172-30-162-189$cat /proc/cpuinfo
    processor   : 0
    vendor_id   : GenuineIntel
    cpu family  : 6
    model   : 6
    model name  : QEMU Virtual CPU version 2.5+
    stepping: 3
    microcode   : 0x1
    cpu MHz : 2095.142
    cache size  : 4096 KB
    physical id : 0
    siblings: 1
    core id : 0
    cpu cores   : 1
    apicid  : 0
    initial apicid  : 0
    fpu : yes
    fpu_exception   : yes
    cpuid level : 13
    wp  : yes
    flags   : fpu de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pse36 clflush mmx fxsr sse sse2 syscall nx lm rep_good nopl pni vmx cx16 x2apic hypervisor lahf_lm tpr_shadow vnmi flexpriority ept vpid
    bogomips: 4190.28
    clflush size: 64
    cache_alignment : 64
    address sizes   : 40 bits physical, 48 bits virtual
    power management:
```

内存信息如下：

```
    yanming@rise-prod-common-172-30-162-189$ cat /proc/meminfo 
    MemTotal:        8176208 kB
    MemFree:         1699348 kB
    Buffers:          183592 kB
    Cached:          2195624 kB
    SwapCached:            0 kB
    Active:          4600896 kB
    Inactive:         455276 kB
    Active(anon):    2680796 kB
    Inactive(anon):     2140 kB
    Active(file):    1920100 kB
    Inactive(file):   453136 kB
    Unevictable:         472 kB
    Mlocked:             472 kB
    SwapTotal:             0 kB
    SwapFree:              0 kB
    Dirty:                56 kB
    Writeback:             0 kB
    AnonPages:       2677428 kB
    Mapped:            20364 kB
    Shmem:              5588 kB
    Slab:             240804 kB
    SReclaimable:     175620 kB
    SUnreclaim:        65184 kB
    KernelStack:        1448 kB
    PageTables:        10544 kB
    NFS_Unstable:          0 kB
    Bounce:                0 kB
    WritebackTmp:          0 kB
    CommitLimit:     4088104 kB
    Committed_AS:    2913480 kB
    VmallocTotal:   34359738367 kB
    VmallocUsed:       22264 kB
    VmallocChunk:   34359660556 kB
    HardwareCorrupted:     0 kB
    AnonHugePages:   2568192 kB
    HugePages_Total:       0
    HugePages_Free:        0
    HugePages_Rsvd:        0
    HugePages_Surp:        0
    Hugepagesize:       2048 kB
    DirectMap4k:       65404 kB
    DirectMap2M:     8323072 kB
```

网卡信息如下(virtio-pci为KVM架构下的虚拟机网卡)：

```
00:03.0 Ethernet controller: Red Hat, Inc Virtio network device
        Subsystem: Red Hat, Inc Device 0001
        Physical Slot: 3
        Control: I/O+ Mem+ BusMaster+ SpecCycle- MemWINV- VGASnoop- ParErr- Stepping- SERR+ FastB2B- DisINTx+
        Status: Cap+ 66MHz- UDF- FastB2B- ParErr- DEVSEL=fast >TAbort- <TAbort- <MAbort- >SERR- <PERR- INTx-
        Latency: 0
        Interrupt: pin A routed to IRQ 10
        Region 0: I/O ports at c060 [size=32]
        Region 1: Memory at febd1000 (32-bit, non-prefetchable) [size=4K]
        Expansion ROM at feb80000 [disabled] [size=256K]
        Capabilities: <access denied>
        Kernel driver in use: virtio-pci

eth0      Link encap:Ethernet  HWaddr 52:54:00:e9:08:b9  
  inet addr:172.30.162.189  Bcast:172.30.175.255  Mask:255.255.240.0
  inet6 addr: fe80::5054:ff:fee9:8b9/64 Scope:Link
  UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
  RX packets:97321466 errors:0 dropped:0 overruns:0 frame:0
  TX packets:78845845 errors:0 dropped:0 overruns:0 carrier:0
  collisions:0 txqueuelen:1000 
  RX bytes:12237505597 (12.2 GB)  TX bytes:9396818901 (9.3 GB)
```

#### 系统调优

- Linux内核TCP/IP协议栈优化

在测试中，client和yworldserver的数据包交互带来的性能挑战主要为数据包密集和连接密集，因此针对以下参数调整优化：


| 参数(路径) | 描述 | 配置值  |  
| ---- | ----------------- | -------------------|  
| /proc/sys/net/core/rmem_default | 默认的TCP数据接收窗口大小（字节） | 8388608 |  
| /proc/sys/net/core/rmem_max | 最大的TCP数据接收窗口（字节） | 16777216 |  
| /proc/sys/net/core/wmem_default | 默认的TCP数据发送窗口大小（字节） | 8388608 |  
| /proc/sys/net/core/wmem_max | 最大的TCP数据发送窗口（字节） | 16777216 |  
| /proc/sys/net/core/somaxconn | 系统中每一个端口最大的监听队列的长度 | 18000 |  
| /proc/sys/net/core/optmem_max | 每个套接字所允许的最大缓冲区的大小 | 20480 | 
| /proc/sys/net/ipv4/tcp_rmem | 为自动调优定义socket使用的内存（三个值，自行百度） | 10240   87380   12582912 |  
| /proc/sys/net/ipv4/tcp_wmem | 为自动调优定义socket使用的内存（三个值，自行百度） | 10240   87380   12582912 |
| /proc/sys/net/ipv4/tcp_max_syn_backlog | 对于还未获得对方确认的连接请求，可保存在队列中的最大数目 | 262144 | 
| /proc/sys/net/ipv4/tcp_tw_reuse | 将处于TIME-WAIT状态的socket（TIME-WAIT的端口）用于新的TCP连接 | 1 |   
| /proc/sys/net/ipv4/tcp_low_latency | 允许TCP/IP栈适应在高吞吐量情况下低延时的情况 | 1 |  
| /proc/sys/net/ipv4/tcp_sack | 启用有选择的应答 | 1 | 
| /proc/sys/net/ipv4/tcp_fack | 启用转发应答 | 1 |

- 开启网卡多队列(RPS/RFS)

由于测试环境不支持网卡多队列（**参照上文的网卡信息中的`Capabilities`，不支持MSI-X && Enable+ && TabSize > 1**），此选项调优暂时放弃

**NOTE**：使能了RPS后，会以增加CPU的%soft的代价降低%usr，RPS/RFS适用于网络密集型，以后的环境是否需要开启多队列还需再做评估，详细请参考此文章：[为什么使能RPS/RFS, 或者RSS/网卡多队列后，QPS反而下降？](http://laoar.net/blog/2017/05/07/rps/)
 

#### 监控系统搭建

- 业务监控

监控系统使用[ELK](https://www.elastic.co/products)解决方案，Gateway的日志导入ELK，在**Grafana**出图

- 系统性能监控

使用[netdata](https://github.com/firehol/netdata)实时查看系统信息

#### 部署程序

- 部署监控系统

    **ELK**:

    - Grafana浏览url：http://60.28.167.65:3000

    - Kibana浏览url：http://60.28.167.65:5601/app/kibana#/discover?_g=\()

    **netdata**:

    - netdata浏览url：http://60.28.167.66:19999 http://60.28.167.67:19999

- 部署Server

    在`60.28.167,67`部署10个yworldserver:

```
    yanming@rise-prod-common-172-30-162-189:~/mcworld/mcworld_server/bin$ cat start_all.sh
    #/bin/sh
    echo "Parsing arguments..."
    num=$1
    echo "start server count=${num}"
    offset=$2
    i=1
    echo "Starting yworldserver..."
    while [ $i -lt $num ]; do
        ./yworldserver --server_id $offset --server_type 1 --world_name mainworld --game_name minetang --gateway_ip 60.28.167.67 --gateway_port 31001 /dev/null 2>&1 &
        i=$(($i+1))
        offset=$(($offset+1))
    done
    yanming@rise-prod-common-172-30-162-189:~/mcworld/mcworld_server/bin$sudo sh ./start_all.sh 10 30000
```


  在`60.28.167,66`部署yworldgateway:

- 部署client

    基于`mcworld_client_core`编写了用于做负载测试的机器人程序`core_robot`，目前还不能像真实的AI那样跑打跳，只会在地图上乱跑(**乱跑会产生移动同步的大量数据包**)。在多台机器上启动`core_robot`，其启动参数如下：

```
start core_robot -n3000 -f1 -a60.28.167.67 -p40001
```

其中，`-n`表示启动的机器人数量， `-f`表示机器人每次step的帧数，还有`-s`可选，表示无sleep不间断地发送信息，用于测试收发率上限。`-p`,`-a`表示yworldgateway的地址和端口。

**NOTE:`core_robot`本身就很吃client测试机的CPU和带宽,`-n`参数不宜设置过高**

- 模拟丢包环境

    使用的工具为**Network Emulator Client**，设置上行/下行延迟各为**20ms**, 丢包率为**10%**，丢包算法为Random Loss。

## 运行结果

----------

##### UDP/TCP延迟测试

在上行/下行延迟各为**20ms**, 丢包率为**10%**，丢包算法为Random Loss的环境下，模拟了10分钟的一局游戏，在ELK的延迟监控如下图：

![udp_tcp_delay](http://ozcwwsfj9.bkt.clouddn.com/udp_tcp_delay_total.jpg)

该图的统计方法是：client每隔100ms向gateway发送带有自己时间戳的ping包，gateway收到ping包后立即回包，在client收到回pong包的时候记录现在的时间，与自己发送ping包的时间做diff，再将这个延迟的值在下一个ping包里发送给gateway。gateway的监控系统会记录每次收到ping包的时间并输出log，ELK再**每隔100ms采样当前收到ping包的数目。**

如果网络较稳定的话，ELK的采样会是恒定为1的水平线。**如果出现了网络抖动，当前发送的包经过重传可能会隔数百毫秒甚至数秒才会收到，就会产生波峰波谷**。

下面是udp(kcp)、tcp放在一张图（绿线为udp(kcp)，黄线为tcp），放大精度的结果：

![delay1](http://ozcwwsfj9.bkt.clouddn.com/delay1.png)

![delay2](http://ozcwwsfj9.bkt.clouddn.com/delay2.png)

![delay3](http://ozcwwsfj9.bkt.clouddn.com/delay3.png)

##### 包转发率/承载人数压力测试

包转发率和玩家连接是两个维度的压力测试。转发率测试需要开启`core_robot`的`-s`参数，不间断地向gateway发送数据，直到gateway的CPU或者网卡负载达到上限。承载人数测试需要放慢`core_robot`帧数，尽可能地启动多个机器人，直到gateway的连接上限。

理想状况下，转发率和承载人数存在线性映射关系。

- 包转发率

    **netdata**日志所显示的最大包转发率如下图：

    ![rate](http://ozcwwsfj9.bkt.clouddn.com/sendrate.jpg)

    130k/s已为峰值，多次改变参数测试无法再提升

- 最大玩家数

    **netdata**日志所显示的最大连接如下图：

    ![socket1](http://ozcwwsfj9.bkt.clouddn.com/socket1.png)

    按照包转发率、帧数、每帧包交互与最大连接的关系，这个最大连接还没有达到峰值，但是已经没有继续压测的必要了。

    同一份测试结果，下图是CPU跑到满负荷时的时间点：

    ![cpu](http://ozcwwsfj9.bkt.clouddn.com/cpu.png)

    在CPU跑到满负荷的时间点，对应的socket连接如下图：

    ![socket2](http://ozcwwsfj9.bkt.clouddn.com/socket2.png)

## 结果分析

----------

#### TCP和UDP的适用场景

- UDP(KCP)能够更平滑地应对网络抖动，在丢包的环境下延迟**只有TCP的1/3**，**在10%丢包率**测试中KCP的延迟**不会高于300ms**，**适合对性能敏感数据包传输**

- UDP(KCP)协议头比TCP**多4个字节**，再加上更快的RTO机制，使得占用带宽**比TCP多8%-10%左右**，**对性能不敏感的数据包建议用TCP节省带宽**

####  网关能跑多少玩家

- **转发率**和**承载人数**成相互成**反比**的映射，可以根据两者的数据建立拟合关系曲线，在此省略细节过程

- 在测试结果中，我们取两者的**最差适应条件**，即**4000个**玩家连接(不引入worldserver)**每秒5万包转发率**跑完了所有的CPU，以及**500个**玩家(引入worldserver)每秒60帧跑打跳13万个数据包占用了所有的网络带宽

- 根据上条信息，**在优化移动同步数据包的前提下**，可以预测在生产环境下**最多能容纳2500个玩家同时连接10个以上**的yworldserver**正常游玩**，在**低于1000个**玩家的时候游戏体验最佳


## 2017.11.17 更新日志

Gateway部署到了线上生产环境，阿里云网络增强型主机`118.31.38.234`，[阿里云官方网页](https://promotion.aliyun.com/ntms/act/ecsnetwork.html)的型号基准为

> 最大内网带宽 10 Gbps
> 最大网络收发包能力 450万PPS
> 66%网络延时降低

示例设计参考[阿里云ECS网络增强型实例设计原理](https://yq.aliyun.com/articles/215982)

硬件配置参考[云服务器配置sn1ne](https://cn.aliyun.com/product/ecs)

我们的云服务器型号为sn1ne.xlarge，运维给出的型号基准如下图：

![sn1ne.xlarge](http://ozcwwsfj9.bkt.clouddn.com/sn1nexlarge.jpg)

#### 性能调优

- ECS最佳实践

sn1ne支持虚拟机网卡多队列/ECS实例开启RPS，不支持irqbalance

- 网卡多队列

sn1ne默认关闭网卡多队列:

```
yanming@yworld-games-gateway1:~$ ethtool -l eth0
Channel parameters for eth0:
Pre-set maximums:
RX:             0
TX:             0
Other:          0
Combined:       2
Current hardware settings:
RX:             0
TX:             0
Other:          0
Combined:       1
```

`Pre-set maximums`列表下的`Combined:2`表示最多支持设置2个队列，`Current hardware settings`列表下的`Combined:1`表示当前生效1个队列

通过`ethtool -L eth0 combined 2`命令，设置网卡当前使用最大队列为2

- irqbalance

```
yanming@yworld-games-gateway1:~$ systemctl status irqbalance
irqbalance.service
   Loaded: error (Reason: No such file or directory)
   Active: inactive (dead)
```

通过`systemctl status irqbalance`命令可知，当前系统不支持irqbalance

- RPS/RFS

sn2ne默认关闭RPS:

```
yanming@yworld-games-gateway1:~/rps$ cat /sys/class/net/eth*/queues/rx-*/rps_cpus
00000000,00000000,00000000,00000000,00000000,00000000,00000000,00000000
```

编写脚本`rps_start.sh`，启动RPS：

```
#!/bin/bash
cpu_num=$(grep -c processor /proc/cpuinfo)
quotient=$((cpu_num/8))
if [ $quotient -gt 2 ]; then
    quotient=2
elif [ $quotient -lt 1 ]; then
    quotient=1
fi
for i in $(seq $quotient)
do
    cpuset="${cpuset}f"
done
echo "set rps_cpus = ${cpuset}"
for rps_file in $(ls /sys/class/net/eth*/queues/rx-*/rps_cpus)
do
    echo $cpuset > $rps_file
done
```

**TODO：完成上述调优之后，能明显看到CPU0~3分担了NET_RX中断，忘了截图**

#### 运行结果

- 最大玩家数

**TODO：忘了截图，shit**

测试环境：client为5台测试机，每台机器启动2000个`core_robot.exe`。server启动一个`yworldgateway`实例，不开启`yworldserver`。client和server的数据包交互为每秒10个`ping-pong`包。

在连接数达到7000以上的时候，总`CPUs`负载不到15%，受理网卡中断的`CPU3`负载不到40%

- 收发包

测试环境：client为1台测试机，启动500个`core_robot.exe`，server启动一个`yworldgateway`实例，在`60.28.167.67`启动10个`yworldserver`。client以每秒120帧的频率乱跑，数据包交互主要为移动同步广播`TOCLIENT_ACTIVE_OBJECT_MESSAGES`。

在500个玩家连接稳定建立之后，socket连接和包转发率如下图：

![500](http://ozcwwsfj9.bkt.clouddn.com/500%E4%BA%BA%E5%8E%8B%E6%B5%8B.jpg)

现场CPU使用频率如下图:

![500cpu](http://ozcwwsfj9.bkt.clouddn.com/500%E4%BA%BA%E5%8E%8B%E6%B5%8BCPU.jpg)

#### 2017.11.17 17:50 更新日志

**TODO:`118.31.38.234`需要部署线上环境，没法继续做压测了。等待合适时机再测**

#### 结果分析

根据500人压测的结果，以及人数增长对应负载增长的线性关系，可以看出服务器最先达到阈值的是服务器能承载的最大PPS，根据阿里云的官方文档，sn1ne.xlarge型号最大PPS为50W/s。而Gateway实测的数据为18W转发率情况下CPU跑到40%左右，可以推算出CPU跑满帧的时候能达到`18 % 0.4 = 45(w)`个。*Gateway不是所有包都透传，有一定的业务处理占CPU，所以达不到官方文档给的50W个属正常表现*。

根据上面的**45W PPS**的性能瓶颈指标，能预估**在优化了移动同步数据包的前提下，最多能容纳10000个玩家正常游玩**，在**低于4500个**玩家的时候游戏体验最佳。

**NOTE：为什么500个玩家每秒钟有18W个数据包呢？因为只启动了10台`yworldserver`，平均每局50个玩家，而数据包是随着玩家数呈O(n²)增长的（主要是有广播协议），实际游戏不会有这么多玩家在一局游戏里，因此实际每局每个玩家的发包数需要线上玩一局再统计**

## 2017.12.1 更新日志

#### 更新玩家每秒包转发数据

2017.11.30晚上23点到24点打了几盘内测游戏，这几局游戏可以统计玩家每秒包转发率的指标。我抽了几局游戏的gameserver统计结果：

![gamelog1](http://ozcwwsfj9.bkt.clouddn.com/gamelog1.png)

![gamelog2](http://ozcwwsfj9.bkt.clouddn.com/gamelog2.png)

图中，箭头所指的为每个玩家的总收发包数量和大小。每个玩家每秒的平均包转发率可由`sum(send, recv) / totaltime`得出。可以看出，每个玩家`send`数量都差不多，`recv`各有差异，这取决于玩家是挂机还是频繁跑打跳。在统计中我们**两边均取最大值**。

- 在**333秒**的一局游戏样本中，玩家最大的总包数为`4204+10189=14393`个，最大每秒包转发率为`14393 / 333 ≈ 43`个。

- 在**441秒**的一局游戏样本中，玩家最大的总包数为`4082+11026=15108`个个，最大每秒包转发率为`15108 / 441 ≈ 34`个。

- 两局游戏均为9人局，取包转发率更大的一局，即最大每秒包转发率为**43**个。

#### 网关能承载多少玩家(最终生产环境数据，直接看结果请点这里)

由以上的测试数据，可以得知：

- 目前的生产环境，**每秒包转发率(PPS)**是性能瓶颈

- 实测Gateway的最大PPS为**45万个**

- 9人局最大每秒包转发率为**43**个

最终得出：

- **所有人都打9人局游戏**的情况下，可最多以承载`450000 / 43 = 10465`人，即同时进行`1046`盘游戏

- 每局游戏开局存在一定网络波峰，为了最佳游戏体验，在网关承载`低于5000个`玩家的时候游戏体验最佳