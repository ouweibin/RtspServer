
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : h264_source.cc
*   Last Modified : 2020-12-10 21:26
*   Describe      :
*
*******************************************************/

#include "h264_source.h"

#include <chrono>
#include <string.h>

H264Source* H264Source::createNew(uint32_t frameRate) {
    return new H264Source(frameRate);
}

H264Source::H264Source(uint32_t frameRate) :
    _frameRate(frameRate) {
    _payload = 96;
    _mediaType = H264;
    _clockRate = 90000;
}

H264Source::~H264Source() { }

uint32_t H264Source::getTimestamp() {
    auto timePoint = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now());
    return static_cast<uint32_t>((timePoint.time_since_epoch().count() + 500) / 1000 * 90);  // TODO
}


std::string H264Source::getMediaDescription(uint16_t port) {
    char buf[64] = { 0 };
    sprintf(buf, "m=video %hu RTP/AVP 96", port);
    return std::string(buf);
}

std::string H264Source::getAttribute() {
    return std::string("a=rtpmap:96 H264/90000");
}
//
// refer to RFC 3984
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |F|NRI|  type   |                                               |
// +-+-+-+-+-+-+-+-+                                               |
// |                                                               |
// |             one or more aggregation units                     |
// |                                                               |
// |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                               :...OPTIONAL RTP padding        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// | FU indicator  |   FU header   |                               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
// |                                                               |
// |                         FU payload                            |
// |                                                               |
// |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                               :...OPTIONAL RTP padding        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
bool H264Source::handleFrame(MediaChannelId channelId, AVFrame frame) {
    uint8_t* frameBuf = frame.buffer.get();
    uint32_t frameSize = frame.size;

    if(frame.timestamp == 0)
        frame.timestamp = getTimestamp();

    if(frameSize <= MAX_RTP_PAYLOAD_SIZE) { // 单一封包
        RtpPacket rtpPkt;
        rtpPkt.type = frame.type;
        rtpPkt.timestamp = frame.timestamp;
        rtpPkt.size = frame.size + 4 + RTP_HEADER_SIZE;  // RTP_OVER_TCP需要预留4个字节
        rtpPkt.last = 1;
        memcpy(rtpPkt.data.get() + 4 + RTP_HEADER_SIZE, frameBuf, frameSize);
    
        if(_sendFrameCallback) {
            if(!_sendFrameCallback(channelId, rtpPkt))
                return false;
        }
    }
    else {   // 分片封包
        char FU_A[2] = { 0 };
        FU_A[0] = (frameBuf[0] & 0xE0) | 28;    // FU indiactor: 保留前3位 + 28
        FU_A[1] = 0x80 | (frameBuf[0] & 0x1f);  // FU header: 开始标志 + 结束标志 + 0 + 保留后5位

        frameBuf += 1;
        frameSize -= 1;

        while(frameSize + 2 > MAX_RTP_PAYLOAD_SIZE) {
            RtpPacket rtpPkt;
            rtpPkt.type = frame.type;
            rtpPkt.timestamp = frame.timestamp;
            rtpPkt.size = 4 + RTP_HEADER_SIZE + MAX_RTP_PAYLOAD_SIZE;
            rtpPkt.last = 0;

            rtpPkt.data.get()[RTP_HEADER_SIZE+4] = FU_A[0];
            rtpPkt.data.get()[RTP_HEADER_SIZE+5] = FU_A[1];
            memcpy(rtpPkt.data.get()+4+RTP_HEADER_SIZE+2, frameBuf, MAX_RTP_PAYLOAD_SIZE-2);

            if(_sendFrameCallback) {
                if(!_sendFrameCallback(channelId, rtpPkt))
                    return false;
            }

            frameBuf += MAX_RTP_PAYLOAD_SIZE - 2;
            frameSize -= MAX_RTP_PAYLOAD_SIZE - 2;

            FU_A[1] &= ~0x80;   // 开始标志置0
        }

        {
            RtpPacket rtpPkt;
            rtpPkt.type = frame.type;
            rtpPkt.timestamp = frame.timestamp;
            rtpPkt.size = 4 + RTP_HEADER_SIZE + 2 + frameSize;
            rtpPkt.last = 1;

            FU_A[1] |= 0x40;   // 结束标志置1
            rtpPkt.data.get()[RTP_HEADER_SIZE+4] = FU_A[0];
            rtpPkt.data.get()[RTP_HEADER_SIZE+5] = FU_A[1];
            memcpy(rtpPkt.data.get()+4+RTP_HEADER_SIZE+2, frameBuf, frameSize);

            if(_sendFrameCallback) {
                if(!_sendFrameCallback(channelId, rtpPkt))
                    return false;
            }
        }
    }
    return true;
}

