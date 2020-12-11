include(CheckFunctionExists)

check_function_exists(accept4 HAVE_ACCEPT4)
if(NOT HAVE_ACCEPT4)
  set_source_files_properties(SocketsOps.cc PROPERTIES COMPILE_FLAGS "-DNO_ACCEPT4")
endif()

set(NET_SOURCES
    Acceptor.cc
    Buffer.cc
    Channel.cc
    Connector.cc
    EventLoop.cc
    EventLoopThread.cc
    EventLoopThreadPool.cc
    InetAddress.cc
    Poller.cc
    poller/DefaultPoller.cc
    poller/EPollPoller.cc
    poller/PollPoller.cc
    Socket.cc
    SocketsOps.cc
    TcpClient.cc
    TcpConnection.cc
    TcpServer.cc
    Timer.cc
    TimerQueue.cc
   )

add_library(muduo_net ${NET_SOURCES})

install(TARGETS muduo_net DESTINATION ${PROJECT_SOURCE_DIR}/lib)

