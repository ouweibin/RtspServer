
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : h264_source.h
*   Last Modified : 2020-12-10 20:59
*   Describe      :
*
*******************************************************/

#ifndef  _H264_SOURCE_H
#define  _H264_SOURCE_H

#include "media_source.h"

#include <string>

class H264Source : public MediaSource {
public:
    static H264Source* createNew(uint32_t frameRate = 25);  // 帧率25
    ~H264Source();

    void setFrameRate(uint32_t frameRate) {
        _frameRate = frameRate;
    }
    uint32_t getFrameRate() const { return _frameRate; }
    static uint32_t getTimestamp();

    virtual std::string getMediaDescription(uint16_t port);
    virtual std::string getAttribute();
    virtual bool handleFrame(MediaChannelId channelId, AVFrame frame);

private:
    H264Source(uint32_t frameRate);


private:
    uint32_t _frameRate;
};

#endif // _H264_SOURCE_H

