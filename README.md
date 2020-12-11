# RtspServer

基于muduo（多线程网络服务框架）在应用层实现了RTSP服务器，支持传输H264视频格式文件，并在VLC上进行播放测试

- 网络库使用Reactor模式，基于非阻塞IO+IO复用，采用epoll的LT模式
- 使用了异步日志、应用层Buffer、线程池技术，采用eventfd进行线程同步，基于timefd和小根堆实现定时
- 主线程accept请求，采用RR轮询分发给IO线程，计算线程解析H264文件数据并转发给IO线程
- 支持基于UDP和TCP的单播功能，同时支持RTSP摘要认证

### 使用

- 服务器使用监听端口554，因为0~1024为公认端口，所以需要root权限运行程序
- VLC测试时用真正的服务器IP地址替换掉`rtsp://0.0.0.0:554/live`的IP地址

### muduo

#### 事件和线程

epoll会监听三种类型的事件，一种是**网络IO事件**，一种是**定时器事件**，还有一种是**自身线程唤醒事件**

多线程服务器中的线程一般分为几类：

- **IO线程**：负责网络IO
- **计算线程**：负责复杂计算
- **其它线程**：日志线程，视频数据转发线程等

#### 核心类

- **Channel**

  每一个Channel对象负责一个文件描述符的IO事件，在Channel对象中保存着**IO事件的类型以及相应的回调函数**，程序中的文件描述符一般都会和一个Channel对象关联，包括eventfd，listenfd，timefd等

- **EventLoop**

  每个线程只有一个EventLoop对象，线程运行loop函数，每次从epoll获得活跃事件，并通过Channel的回调函数进行处理

- **EventLoopThreadPool**

  线程池，可设置线程数并创建对应数量的EventLoopThread对象，可通过round-robin或hash两种策略获取某个线程使用

- **EventLoopThread**

  创建线程，包含一个EventLoop对象，线程运行loop函数

#### One Loop per Thread模型

![](https://gitee.com/ouweibin/PictureBed/raw/master/image/并发模型.png)

#### 主流程

- 主线程负责accept请求，建立连接后采用**Round-Robin轮询方式**分配给线程池中的某个线程，每个线程都运行EventLoop来维护一定数量的连接，管理这些连接的IO事件和定时器事件，每个线程初始时对自己的eventfd进行监听，便于主线程进行异步唤醒
- 当主线程把新连接分配给某个线程后，该线程可能阻塞在epoll_wait中，通过eventfd异步唤醒线程，线程得到活跃事件进行处理
- **queueInLoop是跨线程调用的精髓所在**，使用了异步唤醒

#### 定时器事件

- 以最小堆管理定时器组（以每个定时器的发生时间在最小堆中排序）
- 以timerfd作为通知方式，交给epoll监听，将超时事件转为IO事件
- timerfd所设置的时间总是最小堆的堆顶定时器的发生时间

**timerfd**是Linux为用户程序提供的一个定时器接口。这个接口基于文件描述符，通过文件描述符的可读事件进行超时通知，**读取的数据是超时的次数**，所以能够被用于select/epoll的应用场景

```c
#include <sys/timerfd.h>
int timerfd_create(int clockid, int flags);   // 创建一个定时器描述符timerfd
// clockid:
// 	CLOCK_REALTIME 		相对时间，从1970.1.1到目前的时间
// 	CLOCK_MONOTONIC 	绝对时间，获取的时间为系统重启到现在的时间
// flags:
// 	TFD_NONBLOCK/TFD_CLOEXEC
// 返回值:
//	timerfd（文件描述符）
```

```c
int timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value);       // 启动或关闭fd指定的定时器
// fd: timerfd（文件描述符）
// flags: 0表示设置的是相对时间，1表示设置的是绝对时间
// new_value:
// 	指定新的超时时间，设定new_value.it_value非零则启动定时器，否则关闭定时器，如果
// 	new_value.it_interval为0，则定时器只定时一次，即初始那次，否则之后每隔设定时间超时一次
// old_value:
// 	不为null则返回定时器这次设置之前的超时时间
int timerfd_gettime(int fd, struct itimerspec *curr_value);
// 此函数用于获得定时器距离下次超时还剩下的时间
// 如果调用时定时器已经到期，并且该定时器处于循环模式，那么调用此函数之后定时器重新开始计时

struct itimerspec {
    struct timespec it_interval;  /* Interval for periodic timer */
    struct timespec it_value;     /* Initial expiration */
};
struct timespec {
    time_t tv_sec;                /* Seconds */
    long   tv_nsec;               /* Nanoseconds */
};
```

#### 异步唤醒事件

**eventfd用于异步唤醒**

```c
int eventfd(unsigned int initval, int flags)
// 创建一个eventfd对象，该对象是一个内核维护的无符号的64位整型计数器。初始化为initval的值
// flags可以是以下三个标志位的OR结果:
// 	EFD_CLOEXEC		fork子进程时不继承
// 	EFD_NONBLOCK	设置成非阻塞
//	EFD_SEMAPHORE	（2.6.30以后支持）支持semophore语义的read，简单说就值递减1
read()
// 读操作就是将counter值置0，如果是semophore就减1
write()
// 设置counter的值
```

#### 文件描述符使用

```c
// stdin, stdout, and stderr are 0, 1,and 2
// EpollPoller epoll_create1() fd = 3 - EpollPoller.cc:36
// createTimerfd createTimerfd() fd = 4 - TimerQueue.cc:35
// createEventfd createEventfd() fd = 5 - EventLoop.cc:36
// createNonblockingOrDie sockets::createNonblockingOrDie() fd = 6 - SocketUtil.cc:44
```

#### gtest的安装和使用

```shell
# 在目录/usr/src/下生成gtest目录存放源码
sudo apt-get install libgtest-dev

# 编译源码
cd /usr/src/gtest
sudo mkdir build
cd build
sudo cmake ..
sudo make

# 将编译生成的库拷贝到系统目录下
sudo cp libgtest*.a /usr/local/lib
```



### RTSP服务器

在网络库的基础上，增加一个线程负责视频数据的转发

#### 主线程

- 创建RtspServer，创建MediaSession，添加具体的MediaSource到MediaSession，添加MediaSession到RTSPServer，启动loop循环
- Acceptere接收新连接，创建RtspConnection对象处理RTSP请求
- 在处理DESCRIBE请求时，创建RtpConnection，同时给MediaSession添加RtpConnection
- 在处理PLAY请求时，调用RtpConnection的play函数，设置isPlay标志

#### 视频数据转发线程

- 调用RTSPServer的pushFrame函数
- 调用MediaSession的handleFrame函数
- 调用了具体的MediaSource的handleFrame函数
- MediaSource的handleFrame对帧进行封包，然后调用RtpConnection的sendRtpPacket函数
- sendRtpPacket判断isPlay标志开始发送数据

#### H264文件操作

H264码流由一系列NAL单元组成，NAL单元由**起始码**，**NALU头部**和**NALU载荷**组成。起始码一般为"00 00 01"或"00 00 00 01"，它用于标志一个NAL单元的开始，NALU头部占一个字节，后面都是NALU载荷的内容

![](https://gitee.com/ouweibin/PictureBed/raw/master/image/NAL单元.jpg)

#### RTSP协议

![](https://gitee.com/ouweibin/PictureBed/raw/master/image/RTSP方法.jpg)

![](https://gitee.com/ouweibin/PictureBed/raw/master/image/RTSP协议流程.jpg)

#### RTP协议

- RTP/RTCP协议是传输层协议
- RTP协议对流媒体数据进行封包和传输，RTCP协议用于数据传输过程的控制和同步
- 每个RTP数据报由头部和载荷组成，头部固定为12个字节

```
                  0                   1                   2                   3
                  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                 |V=2|P|X|  CC   |M|     PT      |       sequence number         |
                 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                 |                           timestamp                           |
                 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                 |           synchronization source (SSRC) identifier            |
                 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
                 |            contributing source (CSRC) identifiers             |
                 |                             ....                              |
                 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

载荷则规定了三种不同的结构模式：**单一封包模式、 组合封包模式和分片封包模式**

RTP协议的封包规则是：如果NAL单元小于最大传输单元MTU，就采用单一封包模式，否则采用分片封包模式  

```
                  refer to RFC 3984
                  0                   1                   2                   3
                  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                 |F|NRI|  type   |                                               |
                 +-+-+-+-+-+-+-+-+                                               |
                 |                                                               |
                 |             one or more aggregation units                     |
                 |                                                               |
                 |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                 |                               :...OPTIONAL RTP padding        |
                 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

                  0                   1                   2                   3
                  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                 | FU indicator  |   FU header   |                               |
                 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
                 |                                                               |
                 |                         FU payload                            |
                 |                                                               |
                 |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                 |                               :...OPTIONAL RTP padding        |
                 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```


#### 带认证的RTSP协议流程

```
OPTIONS rtsp://192.168.1.142:554/live RTSP/1.0
CSeq: 2
User-Agent: LibVLC/2.2.6 (LIVE555 Streaming Media v2016.02.22)
```

    RTSP/1.0 200 OK
    CSeq: 2
    Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY


    DESCRIBE rtsp://192.168.1.142:554/live RTSP/1.0
    CSeq: 3
    User-Agent: LibVLC/2.2.6 (LIVE555 Streaming Media v2016.02.22)
    Accept: application/sdp


    RTSP/1.0 401 Unauthorized
    CSeq: 3
    WWW-Authenticate: Digest realm="-_-", nonce="b15ab645fd3d9d17d0905f45527e95e6"


    DESCRIBE rtsp://192.168.1.142:554/live RTSP/1.0
    CSeq: 4
    Authorization: Digest username="admin", realm="-_-", nonce="b15ab645fd3d9d17d0905f45527e95e6", uri="rtsp://192.168.1.142:554/live", response="848666d183dd367fa613e3bd8670bf69"
    User-Agent: LibVLC/2.2.6 (LIVE555 Streaming Media v2016.02.22)
    Accept: application/sdp


    RTSP/1.0 200 OK
    CSeq: 4
    Content-Length: 129
    Content-Type: application/sdp
    
    v=0
    o=- 91574916875 1 IN IP4 192.168.1.142
    t=0 0
    a=control:*
    m=video 0 RTP/AVP 96
    a=rtpmap:96 H264/90000
    a=control:track0


    SETUP rtsp://192.168.1.142:554/live/track0 RTSP/1.0
    CSeq: 5
    Authorization: Digest username="admin", realm="-_-", nonce="b15ab645fd3d9d17d0905f45527e95e6", uri="rtsp://192.168.1.142:554/live", response="1d8f3068153ff01df9c5fe079160dfa4"
    User-Agent: LibVLC/2.2.6 (LIVE555 Streaming Media v2016.02.22)
    Transport: RTP/AVP;unicast;client_port=60780-60781


    RTSP/1.0 200 OK
    CSeq: 5
    Transport: RTP/AVP;unicast;client_port=60780-60781;server_port=1618-1619
    Session: 6880


    PLAY rtsp://192.168.1.142:554/live RTSP/1.0
    CSeq: 6
    Authorization: Digest username="admin", realm="-_-", nonce="b15ab645fd3d9d17d0905f45527e95e6", uri="rtsp://192.168.1.142:554/live", response="575e53b58f49b5a377a9bf32e6077f0f"
    User-Agent: LibVLC/2.2.6 (LIVE555 Streaming Media v2016.02.22)
    Session: 6880
    Range: npt=0.000-


    RTSP/1.0 200 OK
    CSeq: 6
    Range: npt=0.000-
    Session: 6880; timeout=60

摘要认证的加密方式使用**MD5**，同时使用nonce解决重放攻击问题：nonce是由服务器生成的一个随机数，客户端收到nonce后，与用户名，密码一起加密后发送给服务器（**response字段**），服务器根据用户名在数据库搜索密码后对response进行验证，由于nonce只使用1次，所以防止了重放攻击



#### 参考

1. https://github.com/chenshuo/muduo
2. https://github.com/PHZ76/RtspServer



