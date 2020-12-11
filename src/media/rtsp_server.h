
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : rtsp_server.h
*   Last Modified : 2020-12-11 19:44
*   Describe      :
*
*******************************************************/

#ifndef  _RTSP_SERVER_H
#define  _RTSP_SERVER_H

#include "media.h"
#include "rtsp.h"

#include "muduo/net/TcpServer.h"

#include <memory>
#include <string>
#include <mutex>
#include <unordered_map>

class RtspConnection;

class RtspServer : public Rtsp {
public:
    RtspServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& addr);
    ~RtspServer();

    void start();
    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onMessage(const muduo::net::TcpConnectionPtr conn, muduo::net::Buffer* buf, muduo::Timestamp);
   
    MediaSessionId addMediaSession(MediaSession* session);
    void removeMediaSession(MediaSessionId sessionId);

    bool pushFrame(MediaSessionId sessionId, MediaChannelId channelId, AVFrame frame);
 
private:
    virtual MediaSessionPtr lookMediaSession(const std::string& suffix);
    virtual MediaSessionPtr lookMediaSession(MediaSessionId sessionId);

private:
    using RtspConnectionMap = std::unordered_map<std::string, std::shared_ptr<RtspConnection>>;

    muduo::net::TcpServer _server;
    RtspConnectionMap _rtspConnMap;

    std::mutex _sessionMutex;
    std::unordered_map<MediaSessionId, std::shared_ptr<MediaSession>> _mediaSessions;
    std::unordered_map<std::string, MediaSessionId> _rtspSuffixMap;
};

#endif // _RTSP_SERVER_H

