
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : h264_file.h
*   Last Modified : 2020-12-10 21:24
*   Describe      :
*
*******************************************************/

#ifndef _H264_FILE_H
#define _H264_FILE_H

#include <stdio.h>

class H264File {
public:
    H264File(int bufSize = 500000);
    ~H264File();

    bool open(const char* path);
    void close();
    bool isOpend() const { return _fp != nullptr; }

    int readFrame(char* inBuf, size_t inBufSize, bool* bEndOfFrame);

private:
    FILE* _fp;
    char* _buffer;
    int _bufSize;
    int _bytesUsed;
    int _count;
};

#endif // _H264_FILE_H

