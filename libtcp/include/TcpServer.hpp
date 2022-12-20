#pragma once
#include <SockServer.hpp>
#include <SocketConfig.hpp>
#include <BindConfig.hpp>

#define QUEUE_LIMIT 5

class TcpServer : ASockServer
{
private:
public:
    TcpServer(sockaddr_in bind_addr4, int connection = SOCK_STREAM, int queue_limit = QUEUE_LIMIT);
    TcpServer(sockaddr_in6 bind_addr6, int connection = SOCK_STREAM, int queue_limit = QUEUE_LIMIT);
    
};