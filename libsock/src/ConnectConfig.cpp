#include <ConnectConfig.hpp>
#include <cstring>

struct sockaddr_in ConnectConfig::ConnectConfig4(uint32_t port, uint32_t sock_addr)
{
    struct sockaddr_in addr;
    memset(&(addr), 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(sock_addr);
    addr.sin_port = htons(port);
    return addr;
}

struct sockaddr_in6 ConnectConfig::ConnectConfig6(uint32_t port, in6_addr sock_addr)
{
    struct sockaddr_in6 addr;
    memset(&(addr), 0, sizeof(struct sockaddr_in6));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = sock_addr;
    addr.sin6_port = htons(port);
    return addr;
}