
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : rtsp_connection.cc
*   Last Modified : 2020-12-11 19:43
*   Describe      :
*
*******************************************************/

#include "rtsp.h"
#include "rtsp_message.h"
#include "rtsp_connection.h"
#include "media_source.h"
#include "media_session.h"
#include "rtp_connection.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Buffer.h"
#include "muduo/net/TcpConnection.h"

#include <iostream>

#define MAX_RTSP_MESSAGE_SIZE 2048

RtspConnection::RtspConnection(Rtsp* rtsp, const muduo::net::TcpConnectionPtr& conn) :
    _rtsp(rtsp),
    _tcpConn(conn),
    _rtspRequestPtr(new RtspRequest),
    _rtspResponsePtr(new RtspResponse) { }

RtspConnection::~RtspConnection() { }

void RtspConnection::handleRtspMessage(muduo::net::Buffer* buf) {
    if(_connMode == RTSP_SERVER) {
        handleRtspRequest(buf);
    }
    else if(_connMode == RTSP_PUSHER) {
        handleRtspResponse(buf);
    }
    // rtcp
    if(buf->readableBytes() > MAX_RTSP_MESSAGE_SIZE) {
        buf->retrieveAll();
    }
}

bool RtspConnection::handleRtspRequest(muduo::net::Buffer* buf) {
#ifdef RTSP_DEBUG
    std::string str(buf->peek(), buf->readableBytes());
    if(str.find("rtsp") != std::string::npos || str.find("RTSP") != std::string::npos) {
        std::cout << str << std::endl;
    }
#endif
    if(_rtspRequestPtr->parseRequest(buf)) {
        RtspRequest::Method method = _rtspRequestPtr->getMethod();
        if(method == RtspRequest::RTCP) {
            handleRtcp(buf);
            return true;
        }
        else if(!_rtspRequestPtr->gotAll()) {
            return true;
        }

        switch(method) {
	        case RtspRequest::OPTIONS:
            	handleCmdOption();
	            break;
	        case RtspRequest::DESCRIBE:
	            handleCmdDescribe();
	            break;
	        case RtspRequest::SETUP:
	            handleCmdSetup();
	            break;
	        case RtspRequest::PLAY:
	            handleCmdPlay();
	            break;
	        case RtspRequest::TEARDOWN:
	            handleCmdTeardown();
	            break;
	        case RtspRequest::GET_PARAMETER:
	            handleCmdGetParamter();
	            break;
	        default:
	            break;
		}

		if(_rtspRequestPtr->gotAll()) {
            _rtspRequestPtr->reset();
        }
    }
    else {
        return false;
    }
    return true;
}

void RtspConnection::handleRtcp(muduo::net::Buffer* buf) {
    const char* peek = buf->peek();
    if(peek[0] == '$' && buf->readableBytes() > 4) {
        uint32_t pktSize = peek[2] << 8 | peek[3];
        if(pktSize + 4 >= buf->readableBytes()) {
            buf->retrieve(pktSize+4);    // 不处理RTCP包
        }
    }
}

void RtspConnection::handleCmdOption() {
    LOG_INFO << "RtspConnection::handleCmdOption";
    std::shared_ptr<char> res(new char[1024]);
    int size = _rtspRequestPtr->buildOptionRes(res.get(), 1024);
    sendMessage(res, size);
}

void RtspConnection::handleCmdDescribe() {
    if(_rtpConnPtr == nullptr) {
        _rtpConnPtr.reset(new RtpConnection(_tcpConn));
    }

    int size = 0;
    std::shared_ptr<char> res(new char[1024]);
    MediaSessionPtr mediaSessionPtr = _rtsp->lookMediaSession(_rtspRequestPtr->getRtspUrlSuffix());
    if(!mediaSessionPtr) {
        size = _rtspRequestPtr->buildNotFoundRes(res.get(), 1024);       
    }
    else {
        _sessionId =mediaSessionPtr->getMediaSessionId();
        mediaSessionPtr->addClient(_tcpConn->getFd(), _rtpConnPtr);
        
        for(int chn = 0; chn < MAX_MEDIA_CHANNEL; ++chn) {
            MediaSource* source = mediaSessionPtr->getMediaSource(static_cast<MediaChannelId>(chn));
            if(source != nullptr) {
                _rtpConnPtr->setClockRate(static_cast<MediaChannelId>(chn), source->getClockRate());
                _rtpConnPtr->setPayloadType(static_cast<MediaChannelId>(chn),source->getPayloadType());
            }
        }

        std::string sdp = mediaSessionPtr->getSdpMessage(_rtsp->getVersion());
        if(sdp == "") {
            size = _rtspRequestPtr->buildServerErrorRes(res.get(), 1024);
        }
        else {
            size = _rtspRequestPtr->buildDescribeRes(res.get(), 1024, sdp.c_str());
        }
    }

    sendMessage(res, size);
}

void RtspConnection::handleCmdSetup() {
    std::shared_ptr<char> res(new char[1024]);
    int size = 0;
    MediaChannelId channelId = _rtspRequestPtr->getChannelId();
    MediaSessionPtr mediaSessionPtr = _rtsp->lookMediaSession(_sessionId);
    if(!mediaSessionPtr) {
        goto server_error;
    }

//    if(mediaSessionPtr->isMulticast()) {
//        // 组播
//    }
//    else {
        if(_rtspRequestPtr->getTransportMode() == RTP_OVER_TCP) {
            uint16_t rtpChannel = _rtspRequestPtr->getRtpChannel();
            uint16_t rtcpChannel = _rtspRequestPtr->getRtcpChannel();
            uint16_t sessionId = _rtpConnPtr->getRtpSessionId();

            _rtpConnPtr->setupRtpOverTcp(channelId, rtpChannel, rtcpChannel);
            size = _rtspRequestPtr->buildSetupTcpRes(res.get(), 1024, rtpChannel, rtcpChannel, sessionId);
        }
        else if(_rtspRequestPtr->getTransportMode() == RTP_OVER_UDP) {
            uint16_t cliRtpPort = _rtspRequestPtr->getRtpPort();
            uint16_t cliRtcpPort = _rtspRequestPtr->getRtcpPort();
            uint16_t sessionId = _rtpConnPtr->getRtpSessionId();

            if(_rtpConnPtr->setupRtpOverUdp(channelId, cliRtpPort, cliRtcpPort)) {
                // 监听rtcpfd
            }
            else {
                goto server_error;
            }

            uint16_t serRtpPort = _rtpConnPtr->getRtpPort(channelId);
            uint16_t serRtcpPort = _rtpConnPtr->getRtcpPort(channelId);
            size = _rtspRequestPtr->buildSetupUdpRes(res.get(), 1024, serRtpPort, serRtcpPort, sessionId);
        }
        else {
            goto transport_unsupport;
        }
//    }
    
    sendMessage(res, size);
    return;

transport_unsupport:
    size = _rtspRequestPtr->buildUnsupportedRes(res.get(), 1024);
    sendMessage(res, size);
    return;

server_error:
    size = _rtspRequestPtr->buildServerErrorRes(res.get(), 1024);
    sendMessage(res, size);
    return;
}

void RtspConnection::handleCmdPlay() {
    LOG_INFO << "RtspConnection::handleCmdPlay";
    _connState = START_PLAY;
    _rtpConnPtr->play();

    uint16_t sessionId = _rtpConnPtr->getRtpSessionId();
    
    std::shared_ptr<char> res(new char[1024]);
    int size = _rtspRequestPtr->buildPlayRes(res.get(), 1024, nullptr, sessionId);
    sendMessage(res, size);
}

void RtspConnection::handleCmdTeardown() {
    _rtpConnPtr->teardown();

    uint16_t sessionId = _rtpConnPtr->getRtpSessionId();
    std::shared_ptr<char> res(new char[1024]);
    int size =_rtspRequestPtr->buildTeardownRes(res.get(), 1024, sessionId);
    sendMessage(res, size);
    
    // 关闭连接
    _tcpConn->shutdown();
    _tcpConn->forceCloseWithDelay(3.0);
}

void RtspConnection::handleCmdGetParamter() {
    uint16_t sessionId = _rtpConnPtr->getRtpSessionId();
    std::shared_ptr<char> res(new char[1024]);
    int size =_rtspRequestPtr->buildGetParamterRes(res.get(), 1024, sessionId);
    sendMessage(res, size); 
}

void RtspConnection::handleRtspResponse(muduo::net::Buffer* buf) {
#ifdef RTSP_DEBUG
    std::string str(buf->peek(), buf->readableBytes());
    if(str.find("rtsp") != std::string::npos || str.find("RTSP") != std::string::npos) {
        std::cout << str << std::endl;
    } 
#endif
    
    if(_rtspResponsePtr->parseResponse(buf)) {
        RtspResponse::Method method = _rtspResponsePtr->getMethod();
        switch(method) {
            case RtspResponse::OPTIONS:
                if(_connMode == RTSP_PUSHER)
                    sendAnnounce();
                break;
            case RtspResponse::ANNOUNCE:
            case RtspResponse::DESCRIBE:
                sendSetup();
                break;
            case RtspResponse::SETUP:
                sendSetup();
                break;
            case RtspResponse::RECORD:
                handleRecord();
                break;
            default:
                break;
        }
    }
    else {
        return;
    }
    return;
}

void RtspConnection::sendOptions(ConnectionMode mode) {

}

void RtspConnection::sendDescribe() {

}

void RtspConnection::sendAnnounce() {

}

void RtspConnection::sendSetup() {

}

void RtspConnection::handleRecord() {

}

void RtspConnection::sendMessage(std::shared_ptr<char> buf, int size) {
#ifdef RTSP_DEBUG
    std::cout << buf.get() << std::endl;
#endif
    _tcpConn->send(buf.get(), size);
}
    
