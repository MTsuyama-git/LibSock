#pragma once
#include <arpa/inet.h> // struct sockaddr_in, struct sockaddr, inet_ntoa()


class ConnectConfig
{
public:
static struct sockaddr_in ConnectConfig4(uint32_t port, uint32_t sock_addr);
static struct sockaddr_in6 ConnectConfig6(uint32_t port, in6_addr sock_addr);
};