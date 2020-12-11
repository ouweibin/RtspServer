
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : media_session.cc
*   Last Modified : 2020-12-11 19:39
*   Describe      :
*
*******************************************************/

#include "address.h"
#include "media_source.h"
#include "media_session.h"
#include "rtp_connection.h"

#include <forward_list>
#include <string>

std::atomic_uint MediaSession::_lastMediaSessionId(1);  // 初始为1

MediaSession* MediaSession::creatNew(const std::string& rtspUrlSuffix) {
    return new MediaSession(rtspUrlSuffix);
}

MediaSession::MediaSession(const std::string& rtspUrlSuffix) :
    _suffix(rtspUrlSuffix),
    _mediaSources(2),  // 2个MediaSource
    _buffer(2) {       // 2个RingBuffer
    _sessionId = ++_lastMediaSessionId;
}

MediaSession::~MediaSession() { }

bool MediaSession::addMediaSource(MediaChannelId channelId, MediaSource* source) {
    // 该回调函数在具体的MediaSource起作用，即具体的MediaSource把帧封包后，执行该回调函数
    // 对每个RtpConnection都拷贝一份packet，用空间换时间，这样sendRtpPacket就可以并发执行
    source->setSendFrameCallback([this](MediaChannelId channelId, RtpPacket pkt) {
        std::forward_list<std::shared_ptr<RtpConnection>> rtpConnList;
        std::map<std::string, RtpPacket> packets;
        {
            std::lock_guard<std::mutex> lg(_clientMutex);
            for(auto iter = _clients.begin(); iter != _clients.end(); ) {
                auto rtpConn = iter->second.lock();   // 创建shared_ptr对象
                if(rtpConn == nullptr) {           // 该RtpConnection已析构
                    _clients.erase(iter++);
                }
                else {
                    const std::string id = rtpConn->getId();   // id联系packet到rtpConn
                    if(!id.empty()) {
                        if(packets.find(id) == packets.end()) {
                            RtpPacket tmpPkt;
                            memcpy(tmpPkt.data.get(), pkt.data.get(), pkt.size);
                            tmpPkt.size = pkt.size;
                            tmpPkt.last = pkt.last;
                            tmpPkt.timestamp = pkt.timestamp;
                            tmpPkt.type = pkt.type;
                            packets.emplace(id, tmpPkt);
                        }
                        rtpConnList.emplace_front(rtpConn);
                    }
                    iter++;
                }
            }
        }

        for(auto iter1 : rtpConnList) {
            const std::string id = iter1->getId();
            if(!id.empty()) {
                auto iter2 = packets.find(id);
                if(iter2 != packets.end()) {
                    iter1->sendRtpPacket(channelId, iter2->second);  // RtpConnection
                }
            }
        }
        return true;
    });

    _mediaSources[channelId].reset(source);
    return true;
}

bool MediaSession::removeMediaSource(MediaChannelId channelId) {
    _mediaSources[channelId] = nullptr;
    return true;
}

std::string MediaSession::getSdpMessage(const std::string& sessionName) {
    if(!_sdp.empty())
        return _sdp;

    std::string ip = getLocalIpAddress();
    char buf[1024] = { 0 };

    snprintf(buf, sizeof(buf),
             "v=0\r\n"
             "o=- 9%ld 1 IN IP4 %s\r\n"
             "t= 0 0\r\n"
             "a= control:*\r\n",
             (long)std::time(NULL), ip.c_str());

    if(!sessionName.empty()) {
        snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
                "s=%s\r\n",
                sessionName.c_str());
    }

    for(uint32_t chn = 0; chn < _mediaSources.size(); ++chn) {
        if(_mediaSources[chn]) {
            snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
                     "%s\r\n",
                     _mediaSources[chn]->getMediaDescription(0).c_str());
        
            snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
                     "%s\r\n",
                     _mediaSources[chn]->getAttribute().c_str());

            snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
                     "a=control:track%d\r\n", chn);
        }
    }
    _sdp = buf;
    return _sdp;
}

MediaSource* MediaSession::getMediaSource(MediaChannelId channelId) {
    if(_mediaSources[channelId]) {
        return _mediaSources[channelId].get();
    }
    return nullptr;
}

bool MediaSession::handleFrame(MediaChannelId channelId, AVFrame frame) {
    std::lock_guard<std::mutex> lg(_mediaSourceMutex);  // TODO
    if(_mediaSources[channelId]) {
        _mediaSources[channelId]->handleFrame(channelId, frame);
    }
    else {
        return false;
    }
    return true;
}

// handleCmdDescribe会进行addClient操作
bool MediaSession::addClient(int rtspfd, std::shared_ptr<RtpConnection> rtpConnPtr) {
    std::lock_guard<std::mutex> lg(_clientMutex);
    auto iter = _clients.find(rtspfd);
    if(iter == _clients.end()) {
        std::weak_ptr<RtpConnection> rtpConnWeakPtr = rtpConnPtr;   // 要使用std::weak_ptr
        _clients.emplace(rtspfd, rtpConnWeakPtr);
        if(_notifyCallback) {
            _notifyCallback(_sessionId, (uint32_t)_clients.size());
        }
        return true;
    }
    return false;
}

// 关闭rtsp连接时会进行removeClient操作
void MediaSession::removeClient(int rtspfd) {
    std::lock_guard<std::mutex> lock(_clientMutex);
    if(_clients.find(rtspfd) != _clients.end()) {
        _clients.erase(rtspfd);
        if(_notifyCallback) {
            _notifyCallback(_sessionId, (uint32_t)_clients.size());
        }
    }
}

