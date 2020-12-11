
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : rtsp.h
*   Last Modified : 2020-12-10 21:03
*   Describe      :
*
*******************************************************/

#ifndef  _RTSP_H
#define  _RTSP_H

#include "media_session.h"

#include "muduo/net/Callbacks.h"

#include <string>

struct RtspUrlInfo {
    std::string url;
    std::string ip;
    uint16_t port;
    std::string suffix;
};

class Rtsp {
public:
    Rtsp() { }
    virtual ~Rtsp() { }

    virtual void setVersion(const std::string& version) {
        _version = version;
    }
    virtual std::string getVersion() const {
        return _version;
    }

    virtual std::string getRtspUrl() const {
        return _rtspUrlInfo.url;
    }

    bool parseRtspUrl(const std::string& url) {
        char ip[16] = { 0 };
        char suffix[128] = { 0 };
        uint16_t port = 0;

        if(sscanf(url.c_str() + 7, "%[^:]:%hu/%s", ip, &port, suffix) == 3) {
            _rtspUrlInfo.port = port;
        }
        else if(sscanf(url.c_str() + 7, "%[^/]/%s", ip, suffix) == 2) {
            _rtspUrlInfo.port = 554;
        }
        else {
            return false;
        }

        _rtspUrlInfo.url = url;
        _rtspUrlInfo.ip = ip;
        _rtspUrlInfo.suffix = suffix;
        return true;
    }
    
    virtual MediaSessionPtr lookMediaSession(const std::string& suffix) {
        return nullptr;
    }
    virtual MediaSessionPtr lookMediaSession(MediaSessionId sessionId) {
        return nullptr;
    }


private:
    std::string _version;
    struct RtspUrlInfo _rtspUrlInfo;
};

#endif // _RTSP_H

