#pragma once
#ifdef _MSC_VER
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h> // struct sockaddr_in, struct sockaddr, inet_ntoa()
#endif
#include <cstdint>

class BindConfig
{
public:
    static struct sockaddr_in BindConfig4(uint32_t port, uint32_t sock_addr = INADDR_ANY);
    static struct sockaddr_in6 BindConfig6(uint32_t port, in6_addr sock_addr = IN6ADDR_ANY_INIT);
    static struct sockaddr_in BindConfig4Any(uint32_t port);
    static struct sockaddr_in6 BindConfig6Any(uint32_t port);
};