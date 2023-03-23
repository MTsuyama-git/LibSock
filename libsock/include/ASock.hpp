#pragma once
#ifdef _MSC_VER
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h> // struct sockaddr_in, struct sockaddr, inet_ntoa()
#endif

#ifndef _MSC_VER
#define SOCKET int
#endif

#include <SocketConfig.hpp>

class ASock
{
protected:
    SOCKET servSock;
public:
    ASock(SocketConfig config = SocketConfig::TcpConfig());
    ~ASock();
};