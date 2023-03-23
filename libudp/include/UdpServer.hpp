#pragma once
#include <ASock.hpp>
#include <SocketConfig.hpp>
#include <BindConfig.hpp>

#define QUEUE_LIMIT 5

class UdpServer : ASock
{
private:
public:
    UdpServer(sockaddr_in bind_addr4, int connection = SOCK_DGRAM, int queue_limit = QUEUE_LIMIT);
    UdpServer(sockaddr_in6 bind_addr6, int connection = SOCK_DGRAM, int queue_limit = QUEUE_LIMIT);
    SOCKET serv_sock(void);
};