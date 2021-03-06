
/******************************************************
*   Copyright (C)2019 All rights reserved.
*
*   Author        : owb
*   Email         : 2478644416@qq.com
*   File Name     : ring_buffer.h
*   Last Modified : 2020-12-11 19:35
*   Describe      :
*
*******************************************************/

#ifndef  _RING_BUFFER_H
#define  _RING_BUFFER_H

#include <vector>
#include <memory>
#include <atomic>
#include <cassert>
#include <string.h>

template <typename T>
class RingBuffer {
public:
    RingBuffer(int capacity = 60) :
        _buffer(capacity),
        _numDatas(0),
        _capacity(capacity),
        _putPos(0),
        _getPos(0) { }
	
    ~RingBuffer() {	}

    void push_back(const T& data) { pushData(std::forward<T>(data)); } 	
    void push_back(T&& data) { pushData(data); } 
    void pop_front() { add(_getPos); --_numDatas; } 

    T front() {
        assert(_numDatas > 0);
        return _buffer[_getPos];
    }

    bool full()  const { return _numDatas == _capacity? true : false; }	
    bool empty() const { return _numDatas == 0? true : false; }
    int size() const { return _numDatas; }
    int capacity() const { return _capacity; }
	
private:		
	template <typename F>
	void pushData(F&& data) {
        assert(_numDatas < _capacity);
        _buffer[_putPos] = std::forward<F>(data);
        add(_putPos);
        ++_numDatas;
	}

    void add(int& pos) {
        pos = (pos+1) == _capacity? 0 : (pos+1);
    }


private:
    std::vector<T> _buffer;
    std::atomic_int _numDatas;
    
    int _capacity;
    int _putPos;  					
    int _getPos;			
};

#endif // _RING_BUFFER_H

