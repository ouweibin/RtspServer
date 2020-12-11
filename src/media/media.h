
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : media.h
*   Last Modified : 2020-12-10 21:04
*   Describe      :
*
*******************************************************/

#ifndef  _MEDIA_H
#define  _MEDIA_H

#include <memory>

#define MAX_MEDIA_CHANNEL 2

enum MediaType {
    H264 = 96,
    NONE
};

enum FrameType {
    VIDEO_FRAME_I = 0x01,
    VODEO_FRAME_P = 0x02,
    VIDEO_FRAME_B = 0x03
};

enum MediaChannelId {
    channel_0,
    channel_1
};

using MediaSessionId = uint32_t;

struct AVFrame {
    std::shared_ptr<uint8_t> buffer;     // 帧数据
    uint32_t size;                       // 帧大小
    uint8_t type;                        // 帧类型
    uint32_t timestamp;                  // 时间戳

    AVFrame(uint32_t size = 0) :
        buffer(new uint8_t[size]) {
        this->size = size;
        type = 0;
        timestamp = 0;
    }
};

#endif // _MEDIA_H

