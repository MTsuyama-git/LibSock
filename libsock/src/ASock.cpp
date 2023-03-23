#include <ASock.hpp>
#include <exception>
#include <stdexcept>
#ifdef _MSC_VER
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#endif
#ifdef __DEBUG
#include <iostream>
#endif
ASock::ASock(SocketConfig config)
{
    this->servSock = socket(config.protocolFamily(), config.connectionType(), config.protocol());
    if (config.protocol() == IPPROTO_TCP)
    {
        int yes = 1;
        if (setsockopt(this->servSock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes)) < 0)
        {
            throw new std::logic_error("sockopt() failed");
        }
    }
}

ASock::~ASock()
{
#ifdef __DEBUG
    std::cerr << "close" << std::endl;
#endif
#ifdef _MSC_VER
    closesocket(this->servSock);
#else
    close(this->servSock);
#endif
}