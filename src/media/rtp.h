
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : Rtp.h
*   Last Modified : 2020-12-10 20:43
*   Describe      :
*
*******************************************************/

#ifndef  _RTP_H
#define  _RTP_H

#include <memory>

#define RTP_VERSION             2
#define RTP_HEADER_SIZE         12
#define RTP_TCP_HEAD_SIZE       4
#define MAX_RTP_PAYLOAD_SIZE    1420 // TODO

enum TransportMode {
    RTP_OVER_TCP = 1,
    RTP_OVER_UDP = 2,
    RTP_OVER_MULTICAST = 3
};

//
// The RTP header has the following format:
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |V=2|P|X|  CC   |M|     PT      |       sequence number         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                           timestamp                           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |           synchronization source (SSRC) identifier            |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |            contributing source (CSRC) identifiers             |
// |                             ....                              |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
struct RtpHeader {
    unsigned char cc:4;
    unsigned char extension:1;
    unsigned char padding:1;
    unsigned char version:2;
    unsigned char payload:7;
    unsigned char marker:1;

    unsigned short seq;
    unsigned int ts;
    unsigned int ssrc;
};

struct MediaChannelInfo {
    RtpHeader rtpHeader;

    // tcp
    uint16_t rtpChannel;
    uint16_t rtcpChannel;

    // udp
    uint16_t rtpPort;
    uint16_t rtcpPort;
    uint16_t packetSeq;
    uint32_t clockRate;

    //rtcp
    uint64_t packetCount;
    uint64_t octetCount;
    uint64_t lastRtcpNtpTime;

    bool isSetup;
    bool isPlay;
    bool isRecord;
};

struct RtpPacket {
    std::shared_ptr<uint8_t> data;
    uint32_t size;
    uint32_t timestamp;
    uint8_t type;
    uint8_t last;

    RtpPacket() : 
        data(new uint8_t[1600]) {
        size = 0;
        timestamp = 0;
        type = 0;
        last = 0;
    }
};

#endif // _RTP_H

