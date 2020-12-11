
/******************************************************
*   Copyright (C)2020 All rights reserved.
*
*   Author        : ouweibin
*   Email         : 2478644416@qq.com
*   File Name     : address.h
*   Last Modified : 2020-12-11 19:40
*   Describe      :
*
*******************************************************/

#ifndef  _ADDRESS_H
#define  _ADDRESS_H

#include <string>
#include <string.h>

#include <unistd.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <net/if.h>

inline std::string getLocalIpAddress() {                                                                                                                                          
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        close(sockfd);
        return "0.0.0.0";
    }
    
    struct ifconf ifconf;
    char buf[512] = { 0 };
    ifconf.ifc_len = sizeof buf;
    ifconf.ifc_buf = buf;
    if(ioctl(sockfd, SIOCGIFCONF, &ifconf) == -1) {
        close(sockfd);
        return "0.0.0.0";
    }

    close(sockfd);

    struct ifreq* ifreq = ifconf.ifc_req;
    for(int i = (ifconf.ifc_len / sizeof(struct ifreq)); i > 0; --i) {
        if (ifreq->ifr_flags == AF_INET) {
            if(strcmp(ifreq->ifr_name, "lo") != 0) {
                return inet_ntoa(((struct sockaddr_in*)&(ifreq->ifr_addr))->sin_addr); // 取第一个
            }
            ifreq++;
        }
    }
    return "0.0.0.0";
}

#endif // _ADDRESS_H

