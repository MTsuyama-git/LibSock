#pragma once
#include <ASock.hpp>
#include <SocketConfig.hpp>
#include <BindConfig.hpp>

class TcpClient : ASock
{
private:
public:
    TcpClient(sockaddr_in conn_addr4, int connection = SOCK_STREAM);
    TcpClient(sockaddr_in6 conn_addr6, int connection = SOCK_STREAM);
    int serv_sock(void);
};