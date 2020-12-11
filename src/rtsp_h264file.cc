
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : rtsp_h264file.cc
*   Last Modified : 2020-12-10 21:25
*   Describe      :
*
*******************************************************/

#include "h264_file.h"
#include "h264_source.h"
#include "rtsp_server.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

#include <iostream>
#include <thread>
#include <chrono>

void sendFrameThread(RtspServer* rtspServer,
                     MediaSessionId sessionId, 
                     H264File* h264File) {
    LOG_INFO << "sendFrameThread start";
    size_t bufSize = 500000;
    uint8_t* frameBuf = new uint8_t[bufSize];

    while(1) {
        bool endOfFrame;
        int frameSize = h264File->readFrame((char*)frameBuf, bufSize, &endOfFrame);
        if(frameSize > 0) {
            AVFrame videoFrame = { 0 };
            videoFrame.size = frameSize;
            videoFrame.timestamp = H264Source::getTimestamp();
            videoFrame.buffer.reset(new uint8_t[videoFrame.size]);
            memcpy(videoFrame.buffer.get(), frameBuf, videoFrame.size);

            rtspServer->pushFrame(sessionId, channel_0, videoFrame);
        }
        else {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
    delete[] frameBuf;
}

int main(int argc, char* argv[]) {
    if(argc != 2) {
        printf("Usage: %s test.264\n", argv[0]);
        return 0;
    }

    H264File h264File;
    if(!h264File.open(argv[1])) {
        printf("Open %s failed\n", argv[1]);
        return 0;
    }

    std::string ip = "0.0.0.0";
    muduo::net::InetAddress serverAddr(ip, 554);
    muduo::net::EventLoop loop;
    RtspServer server(&loop, serverAddr);

    MediaSession* session = MediaSession::creatNew("live");
    const std::string rtspUrl = "rtsp://" + ip + ":554/" + session->getRtspUrlSuffix();

    session->addMediaSource(channel_0, H264Source::createNew());
    MediaSessionId sessionId = server.addMediaSession(session);

    std::thread t(sendFrameThread, &server, sessionId, &h264File);
    t.detach();

    std::cout << "Play URL: " << rtspUrl << std::endl;

    server.start();
    loop.loop();
    return 0;
}

