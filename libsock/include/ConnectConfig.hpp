#pragma once
#ifdef _MSC_VER
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h> // struct sockaddr_in, struct sockaddr, inet_ntoa()
#endif
#include <cstdint>

class ConnectConfig
{
public:
static struct sockaddr_in ConnectConfig4(uint32_t port, uint32_t sock_addr);
static struct sockaddr_in6 ConnectConfig6(uint32_t port, in6_addr sock_addr);
};