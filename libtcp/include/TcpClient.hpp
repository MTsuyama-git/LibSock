#pragma once
#include <ASock.hpp>
#include <SocketConfig.hpp>
#include <BindConfig.hpp>

#define QUEUE_LIMIT 5

class TcpClient : ASock
{
private:
public:
    TcpClient(sockaddr_in conn_addr4, int connection = SOCK_STREAM, int queue_limit = QUEUE_LIMIT);
    TcpClient(sockaddr_in6 conn_addr6, int connection = SOCK_STREAM, int queue_limit = QUEUE_LIMIT);
    int serv_sock(void);
};