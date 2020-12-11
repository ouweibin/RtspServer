
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : rtp_connection.cc
*   Last Modified : 2020-12-11 19:42
*   Describe      :
*
*******************************************************/

#include "rtp_connection.h"

#include "muduo/net/TcpConnection.h"
#include "muduo/net/EventLoop.h"

#include <chrono>
#include <random>

#include <unistd.h>

RtpConnection::RtpConnection(const muduo::net::TcpConnectionPtr& tcpConn) :
    _tcpConn(tcpConn) {
    std::random_device rd;
    for(int chn = 0; chn < MAX_MEDIA_CHANNEL; ++chn) {
        _rtpfd[chn] = 0;
        _rtcpfd[chn] = 0;
        memset(&_mediaChannelInfo[chn], 0, sizeof(_mediaChannelInfo[chn]));
        _mediaChannelInfo[chn].rtpHeader.version = RTP_VERSION;
        _mediaChannelInfo[chn].packetSeq = rd()%0xffff;
        _mediaChannelInfo[chn].rtpHeader.seq = 0;
        _mediaChannelInfo[chn].rtpHeader.ts = htonl(rd());
        _mediaChannelInfo[chn].rtpHeader.ssrc = htonl(rd());
    }
}
RtpConnection::~RtpConnection() {
    for(int chn = 0; chn < MAX_MEDIA_CHANNEL; ++chn) {
        if(_rtpfd[chn] > 0)
            ::close(_rtpfd[chn]);
        if(_rtcpfd[chn] > 0)
            ::close(_rtcpfd[chn]);
    }
}

bool RtpConnection::setupRtpOverTcp(MediaChannelId channelId, uint16_t rtpChannel, uint16_t rtcpChannel) {
    auto tcpConn = _tcpConn.lock();
    if(tcpConn == nullptr) {
        return false;
    }

    _mediaChannelInfo[channelId].rtpChannel = rtpChannel;
    _mediaChannelInfo[channelId].rtcpChannel = rtcpChannel;
    _rtpfd[channelId] = tcpConn->getFd();
    _rtcpfd[channelId] = tcpConn->getFd();
    _mediaChannelInfo[channelId].isSetup = true;
    _transportMode = RTP_OVER_TCP;
    return true;
}

bool RtpConnection::setupRtpOverUdp(MediaChannelId channelId, uint16_t rtpPort, uint16_t rtcpPort) {
    auto tcpConn = _tcpConn.lock();
    if(tcpConn == nullptr) {
        return false;
    }

    socklen_t addrlen = sizeof(struct sockaddr_in);
    getpeername(tcpConn->getFd(), (struct sockaddr *)&_peerAddr, &addrlen);

    _mediaChannelInfo[channelId].rtpPort = rtpPort;
    _mediaChannelInfo[channelId].rtcpPort = rtcpPort;

    std::random_device rd;
    for(int n = 0; n <= 10; ++n) {
        if(n == 10)
            return false;

        _localRtpPort[channelId] = rd() & 0xfffe;
        _localRtcpPort[channelId] = _localRtpPort[channelId] + 1;

        _rtpfd[channelId] = ::socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in rtpAddr = { 0 };
        rtpAddr.sin_family = AF_INET;
        rtpAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        rtpAddr.sin_port = htons(_localRtpPort[channelId]);
        if(bind(_rtpfd[channelId], (struct sockaddr*)&rtpAddr, sizeof rtpAddr) == -1) {
            close(_rtpfd[channelId]);
            continue;
        }

        _rtcpfd[channelId] = ::socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in rtcpAddr = { 0 };
        rtcpAddr.sin_family = AF_INET;
        rtcpAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        rtcpAddr.sin_port = htons(_localRtcpPort[channelId]);
        if(bind(_rtcpfd[channelId], (struct sockaddr*)&rtcpAddr, sizeof rtcpAddr) == -1) {
            close(_rtcpfd[channelId]);
            close(_rtpfd[channelId]);
            continue;
        }

        break;
    }

    //设置发送缓冲区大小

    _peerRtpAddr[channelId].sin_family = AF_INET;
    _peerRtpAddr[channelId].sin_addr.s_addr = _peerAddr.sin_addr.s_addr;
    _peerRtpAddr[channelId].sin_port = htons(_mediaChannelInfo[channelId].rtcpPort);

    _peerRtcpAddr[channelId].sin_family = AF_INET;
    _peerRtcpAddr[channelId].sin_addr.s_addr = _peerAddr.sin_addr.s_addr;
    _peerRtcpAddr[channelId].sin_port = htons(_mediaChannelInfo[channelId].rtcpPort);
    
    _mediaChannelInfo[channelId].isSetup = true;
    _transportMode = RTP_OVER_UDP;

    return true;
}

void RtpConnection::play() {
    for(int chn = 0; chn < MAX_MEDIA_CHANNEL; ++chn) {
        if(_mediaChannelInfo[chn].isSetup)
            _mediaChannelInfo[chn].isPlay = true;
    }
}

void RtpConnection::record() {
    for(int chn = 0; chn < MAX_MEDIA_CHANNEL; ++chn) {
        if(_mediaChannelInfo[chn].isSetup) {
            _mediaChannelInfo[chn].isRecord = true;
            _mediaChannelInfo[chn].isPlay = true;
        }
    }
}

void RtpConnection::teardown() {
    if(!_isClosed) {
        _isClosed = true;
        for(int chn = 0; chn < MAX_MEDIA_CHANNEL; ++chn) {
            _mediaChannelInfo[chn].isRecord = false;
            _mediaChannelInfo[chn].isPlay = false;
        }
    }
}

std::string RtpConnection::getRtpInfo(const std::string& rtspUrl) {
	char buf[1024] = { 0 };
    snprintf(buf, 1024, "RTP-Info: ");

    int numChannel = 0;

    auto timePoint = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now());
    auto ts = timePoint.time_since_epoch().count();

    for(int chn = 0; chn < MAX_MEDIA_CHANNEL; ++chn) {
        uint32_t rtpTime = (uint32_t)(ts * _mediaChannelInfo[chn].clockRate / 1000);
        if(_mediaChannelInfo[chn].isSetup) {
            if(numChannel != 0) {
                snprintf(buf+strlen(buf), sizeof buf - strlen(buf), ",");
            }

            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
                     "url=%s/track%d;seq=0;rtptime=%u",
                     rtspUrl.c_str(), chn, rtpTime);
            numChannel++;
        }
    }
	return std::string(buf);
}

void RtpConnection::setFrameType(uint8_t frameType) {
    _frameType = frameType;
    if(!_hasIDRFrame && (_frameType == 0 || _frameType == VIDEO_FRAME_I)) {
		_hasIDRFrame = true;
    }
} 

void RtpConnection::setRtpHeader(MediaChannelId channelId, RtpPacket pkt) {
    if((_mediaChannelInfo[channelId].isPlay || _mediaChannelInfo[channelId].isRecord) && _hasIDRFrame) {
        _mediaChannelInfo[channelId].rtpHeader.marker = pkt.last;
        _mediaChannelInfo[channelId].rtpHeader.ts = htonl(pkt.timestamp);
        _mediaChannelInfo[channelId].rtpHeader.seq = htons(_mediaChannelInfo[channelId].packetSeq++);
        memcpy(pkt.data.get()+4, &_mediaChannelInfo[channelId].rtpHeader, RTP_HEADER_SIZE);
    }
}

int RtpConnection::sendRtpPacket(MediaChannelId channelId, RtpPacket pkt) {
    if(_isClosed) {
        return -1;
    }

    auto tcpConn = _tcpConn.lock();
    if(tcpConn == nullptr) {
        return -1;
    }

    tcpConn->getLoop()->runInLoop(
        std::bind([this](MediaChannelId channelId, RtpPacket pkt) {
            this->setFrameType(pkt.type);
            this->setRtpHeader(channelId, pkt);

            if((_mediaChannelInfo[channelId].isPlay || 
			    _mediaChannelInfo[channelId].isRecord) && _hasIDRFrame) {            
                if(_transportMode == RTP_OVER_TCP) {
                    sendRtpOverTcp(channelId, pkt);
                }
                else {
                    sendRtpOverUdp(channelId, pkt);
                }
	        } 
        }, channelId, pkt));

    return 1;
}


int RtpConnection::sendRtpOverTcp(MediaChannelId channelId, RtpPacket pkt) {
    auto tcpConn = _tcpConn.lock();
    if(tcpConn == nullptr) {
        return -1;
    }

    uint8_t* rtpPktPtr = pkt.data.get();
    rtpPktPtr[0] = '$';
    rtpPktPtr[1] = (char)_mediaChannelInfo[channelId].rtpChannel;
    rtpPktPtr[2] = (char)(((pkt.size-4)&0xFF00)>>8);
    rtpPktPtr[3] = (char)((pkt.size -4)&0xFF);

	tcpConn->send((char*)rtpPktPtr, pkt.size);
    return pkt.size;
}

int RtpConnection::sendRtpOverUdp(MediaChannelId channelId, RtpPacket pkt) {
    int ret = sendto(_rtpfd[channelId], (const char*)pkt.data.get()+4, pkt.size-4, 0, 
					 (struct sockaddr *)&(_peerRtpAddr[channelId]),
                     sizeof(struct sockaddr_in));
    if(ret < 0) {        
        teardown();
        return -1;
    }
    return ret;
}

const std::string RtpConnection::getId() const {
    auto tcpConn = _tcpConn.lock();
    if(tcpConn == nullptr) {
        return "";
    }
    return tcpConn->name();	
}

