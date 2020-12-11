
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : rtp_connection.h
*   Last Modified : 2020-12-10 21:01
*   Describe      :
*
*******************************************************/

#ifndef  _RTP_CONNECTION_H
#define  _RTP_CONNECTION_H

#include "media.h"
#include "rtp.h"

#include "muduo/net/Callbacks.h"

#include <memory>
#include <string>

#include <arpa/inet.h>

class RtpConnection {
public:
    RtpConnection(const muduo::net::TcpConnectionPtr& tcpConn);
    ~RtpConnection();

    void setClockRate(MediaChannelId channelId, uint32_t clockRate) {
        _mediaChannelInfo[channelId].clockRate = clockRate;
    }
    void setPayloadType(MediaChannelId channelId, uint32_t payload) {
        _mediaChannelInfo[channelId].rtpHeader.payload = payload;
    }

    bool setupRtpOverTcp(MediaChannelId channelId, uint16_t rtpChannel, uint16_t rtcpChannel);
    bool setupRtpOverUdp(MediaChannelId channelId, uint16_t rtpPort, uint16_t rtcpPort);

    uint32_t getRtpSessionId() const {
        return static_cast<uint32_t>(reinterpret_cast<size_t>(this));
    }
    uint16_t getRtpPort(MediaChannelId channelId) const {
        return _localRtpPort[channelId];
    }
    uint16_t getRtcpPort(MediaChannelId channelId) const {
        return _localRtcpPort[channelId];
    }
    int getRtcpFd(MediaChannelId channelId) const {
        return _rtcpfd[channelId];
    }
    bool isSetup(MediaChannelId channelId) const {
        return _mediaChannelInfo[channelId].isSetup;
    }

    void play();
    void record();
    void teardown();

    std::string getRtpInfo(const std::string& rtspUrl);
    int sendRtpPacket(MediaChannelId channelId, RtpPacket pkt);

    bool isClosed() const { return _isClosed; }
    bool hasIDRFrame() const { return _hasIDRFrame; }

    const std::string getId() const;

private:
    void setFrameType(uint8_t frameType = 0);
    void setRtpHeader(MediaChannelId channelId, RtpPacket pkt);
    int sendRtpOverTcp(MediaChannelId channelId, RtpPacket pkt);
    int sendRtpOverUdp(MediaChannelId channelId, RtpPacket pkt);


private:
    std::weak_ptr<muduo::net::TcpConnection> _tcpConn;
    
    bool _isClosed = false;
    bool _hasIDRFrame = false;

    TransportMode _transportMode;
    uint8_t _frameType = 0;
    uint16_t _localRtpPort[MAX_MEDIA_CHANNEL];
    uint16_t _localRtcpPort[MAX_MEDIA_CHANNEL];
    int _rtpfd[MAX_MEDIA_CHANNEL];
    int _rtcpfd[MAX_MEDIA_CHANNEL];

    struct sockaddr_in _peerAddr;
    struct sockaddr_in _peerRtpAddr[MAX_MEDIA_CHANNEL];
    struct sockaddr_in _peerRtcpAddr[MAX_MEDIA_CHANNEL];
    MediaChannelInfo _mediaChannelInfo[MAX_MEDIA_CHANNEL];
};

#endif // _RTP_CONNECTION_H

