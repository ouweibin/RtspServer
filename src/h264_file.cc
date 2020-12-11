
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : h264_file.cc
*   Last Modified : 2020-12-10 21:26
*   Describe      :
*
*******************************************************/

#include "h264_file.h"

#include <string.h>


H264File::H264File(int bufSize) :
    _fp(NULL),
    _bufSize(bufSize),
    _bytesUsed(0),
    _count(0) {
    _buffer = new char[bufSize];
}

H264File::~H264File() {
    delete _buffer;
}

bool H264File::open(const char* path) {
    _fp = fopen(path, "rb");
    if(_fp == NULL) {
        return false;
    }
    return true;
}

void H264File::close() {
    if(_fp) {
        fclose(_fp);
        _fp = NULL;
        _count = 0;
        _bytesUsed = 0;
    }
}

int H264File::readFrame(char* buffer, size_t bufSize, bool* endOfFrame) {
    if(_fp == NULL) {
        return -1;
    }

    size_t bytesRead = fread(_buffer, 1, _bufSize, _fp);
    if(bytesRead == 0) {
        fseek(_fp, 0, SEEK_SET);   // 文件指针指向文件的开头
        _count = 0;
        _bytesUsed = 0;
        bytesRead = fread(_buffer, 1, _bufSize, _fp);
        if(bytesRead == 0) {       // 两次都返回零，读取出错
           this->close();
           return -1;
        }
    }
    
    *endOfFrame = false;
    
    bool findStart = false;
    bool findEnd = false;
    int startCode = 3;
    size_t i = 0;
    // (findStart
    for(; i < bytesRead - 5; ++i) {
        if(_buffer[i] == 0 && _buffer[i+1] == 0 && _buffer[i+2] == 1) {
            startCode = 3;      // 起始码为"00 00 01"
        }
        else if(_buffer[i] == 0 && _buffer[i+1] == 0 && _buffer[i+2] == 0 && _buffer[i+3] == 1) {
            startCode = 4;      // 起始码为"00 00 00 01"
        }
        else {
            continue;
        }

        if(((_buffer[i+startCode]&0x1f) == 0x5 || (_buffer[i+startCode]&0x1f) == 0x1) &&
           ((_buffer[i+startCode+1]&0x80) == 0x80)) {
            findStart = true;
            i += 4;
            break;
        }
    }

    // findEnd
    for(; i < bytesRead - 5; ++i) {
        if(_buffer[i] == 0 && _buffer[i+1] == 0 && _buffer[i+2] == 1) {
            startCode = 3;
        }
        else if(_buffer[i] == 0 && _buffer[i+1] == 0 && _buffer[i+2] == 0 && _buffer[i+3] == 1) {
            startCode = 4;
        }
        else {
            continue;
        }

        if((_buffer[i+startCode]&0x1f) == 0x6 || 
           (_buffer[i+startCode]&0x1f) == 0x7 ||
           (_buffer[i+startCode]&0x1f) == 0x8 || 
           (((_buffer[i+startCode]&0x1f) == 0x5 || (_buffer[i+startCode]&0x1f) == 0x1) && 
            (_buffer[i+startCode+1]&0x80) == 0x80)) {
            findEnd = true;
            break;
        }
    }

    bool flag = false;
    if(findStart && !findEnd && _count > 0) {
        flag = findEnd = true;
        i = bytesRead;
        *endOfFrame = true;
    }

    // 可能是_bufSize太小
    if(!findStart || !findEnd) {
        this->close();
        return -1;
    }

    size_t size = (i <= bufSize? i : bufSize);
    memcpy(buffer, _buffer, size);

    if(!flag) {
        _count += 1;
        _bytesUsed += i;
    }
    else {
        _count = 0;
        _bytesUsed = 0;
    }

    fseek(_fp, _bytesUsed, SEEK_SET);
    return size;
}


