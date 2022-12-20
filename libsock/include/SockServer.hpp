#pragma once
#include <arpa/inet.h> // struct sockaddr_in, struct sockaddr, inet_ntoa()

#include <SocketConfig.hpp>

class ASockServer
{
protected:
    int servSock;

public:
    ASockServer(SocketConfig config = SocketConfig::TcpConfig());
    ~ASockServer();
};