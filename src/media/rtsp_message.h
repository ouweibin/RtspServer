
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : rtsp_message.h
*   Last Modified : 2020-12-10 21:03
*   Describe      :
*
*******************************************************/

#ifndef  _RTSP_MESSAGE_H
#define  _RTSP_MESSAGE_H

#include "media.h"
#include "rtp.h"

#include "muduo/net/Buffer.h"

#include <memory>
#include <string>
#include <unordered_map>

class RtspRequest {
public:
    enum Method {
        OPTIONS = 0,
        DESCRIBE,
        SETUP,
        PLAY,
        TEARDOWN,
        GET_PARAMETER,
        RTCP,
        NONE
    };

    enum RtspRequestParseState {
        kParseRequestLine,    // 请求行
        kParseHeadersLine,    // 首部行，可能有多行
        kGotAll               // 完成
    };
    
    bool gotAll() const { return _state == kGotAll; }
    void reset() { 
        _state = kParseRequestLine; 
        _requestLineParam.clear();
        _headerLineParam.clear();
    }

    bool parseRequest(muduo::net::Buffer* buffer);

    Method getMethod() const { return _method; }

    uint32_t getCSeq() const;
    std::string getRtspUrl() const;
    std::string getRtspUrlSuffix() const;
    std::string getIp() const;
    
    TransportMode getTransportMode() const {
        return _transport;
    }

    MediaChannelId getChannelId() const {
        return _channelId;
    }

    uint8_t getRtpChannel() const;
    uint8_t getRtcpChannel() const;
    uint16_t getRtpPort() const;
    uint16_t getRtcpPort() const;

    int buildOptionRes(const char* buf, int bufSize);
    int buildDescribeRes(const char* buf, int bufSize, const char* strSdp);
    int buildSetupMulticastRes(const char* buf, int bufSize, const char* strMulticastIp, uint16_t port, uint32_t sessionId);
    int buildSetupTcpRes(const char* buf, int bufSize, uint16_t rtpChn, uint16_t rtcpChn, uint32_t sessionId);
    int buildSetupUdpRes(const char* buf, int bufSize, uint16_t rtpChn, uint16_t rtcpChn, uint32_t sessionId);
    int buildPlayRes(const char* buf, int bufSize, const char* rtpInfo, uint32_t sessionId);
    int buildTeardownRes(const char* buf, int bufSize, uint32_t sessionId);
    int buildGetParamterRes(const char* buf, int bufSize, uint32_t sessionId);
    int buildNotFoundRes(const char* buf, int bufSize);
    int buildServerErrorRes(const char* buf, int bufSize);
    int buildUnsupportedRes(const char* buf, int bufSize);

private:
    bool parseRequestLine(const char* begin, const char* end);
    bool parseHeadersLine(const char* begin, const char* end);
    bool parseCSeq(std::string& message);
    bool parseAccept(std::string& message);
    bool parseTransport(std::string& message);
    bool parseSessionId(std::string& message);
    bool parseMediaChannel(std::string& message);

private:
    Method _method;
    MediaChannelId _channelId;
    TransportMode _transport;

    std::unordered_map<std::string, std::pair<std::string, uint32_t>> _requestLineParam;
    std::unordered_map<std::string, std::pair<std::string, uint32_t>> _headerLineParam;

    RtspRequestParseState _state = kParseRequestLine;
};


class RtspResponse {
public:
    enum Method {
        OPTIONS = 0,
        DESCRIBE,
        ANNOUNCE,
        SETUP,
        RECORD,
        RTCP,
        NONE
    };

    bool parseResponse(muduo::net::Buffer* buffer);

    Method getMethod() const { return _method; }
    uint32_t getCSeq() const { return _cseq; }
    std::string getSession() const { return _session; }

    void setUserAgent(const char* userAgent) 
    { _userAgent = std::string(userAgent); }

    void setRtspUrl(const char* url)
    { _rtspUrl = std::string(url); }

    int buildOptionReq(const char* buf, int bufSize);
    int buildDescribeReq(const char* buf, int bufSize);
    int buildAnnounceReq(const char* buf, int bufSize, const char* strSdp);
    int buildSetupTcpReq(const char* buf, int bufSize, int trackId);
    int buildRecordReq(const char* buf, int bufSize);

private:
    Method _method;
    uint32_t _cseq = 0;
    std::string _userAgent;
    std::string _rtspUrl;
    std::string _session;
};

#endif // _RTSP_MESSAGE_H

