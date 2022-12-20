#pragma once
#include <arpa/inet.h> // struct sockaddr_in, struct sockaddr, inet_ntoa()

#include <SocketConfig.hpp>

class ASock
{
protected:
    int servSock;

public:
    ASock(SocketConfig config = SocketConfig::TcpConfig());
    ~ASock();
};