
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : rtsp_connection.h
*   Last Modified : 2020-12-10 21:02
*   Describe      :
*
*******************************************************/

#ifndef  _RTSP_CONNECTION_H
#define  _RTSP_CONNECTION_H

#include "media.h"

#include "muduo/net/Callbacks.h"

#include <memory>

class Buffer;
class Rtsp;
class RtspRequest;
class RtspResponse;
class RtpConnection;

class RtspConnection {
public:
    enum ConnectionMode {
        RTSP_SERVER,
        RTSP_PUSHER
    };

    enum ConnectionState {
        START_CONNECT,
        START_PLAY,
        START_PUSH
    };

    RtspConnection(Rtsp* rtsp, const muduo::net::TcpConnectionPtr& conn);
    ~RtspConnection();

    MediaSessionId getMediaSessionId() const { return _sessionId; }
    bool isPlay() const { return _connState == START_PLAY; }
    bool isRecord() const { return _connState == START_PUSH; }

    void handleRtspMessage(muduo::net::Buffer* buf);

private:
    void handleRtcp(muduo::net::Buffer* buf);
    void sendMessage(std::shared_ptr<char> buf, int size);
    
    bool handleRtspRequest(muduo::net::Buffer* buf);
    void handleRtspResponse(muduo::net::Buffer* buf);

    void handleCmdOption();
    void handleCmdDescribe();
    void handleCmdSetup();
    void handleCmdPlay();
    void handleCmdTeardown();
    void handleCmdGetParamter();

    void sendOptions(ConnectionMode mode = RTSP_SERVER);
    void sendDescribe();
    void sendAnnounce();
    void sendSetup();
    void handleRecord();


private:
    Rtsp* _rtsp = nullptr;
    const muduo::net::TcpConnectionPtr _tcpConn;
    ConnectionMode _connMode = RTSP_SERVER;
    ConnectionState _connState = START_CONNECT;
    MediaSessionId _sessionId = 0;

    std::shared_ptr<RtspRequest> _rtspRequestPtr;
    std::shared_ptr<RtspResponse> _rtspResponsePtr;
    std::shared_ptr<RtpConnection> _rtpConnPtr;
//    std::shared_ptr<Channel> _rtcpChannels[MAX_MEDIA_CHANNEL];
};

#endif // _RTSP_CONNECTION_H

