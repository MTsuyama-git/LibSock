#include <SockServer.hpp>
#include <exception>
#include <stdexcept>
#include <unistd.h>
#ifdef __DEBUG
#include <iostream>
#endif
ASockServer::ASockServer(SocketConfig config)
{
    this->servSock = socket(config.protocolFamily(), config.connectionType(), config.protocol());
    int yes = 1;
    if (setsockopt(this->servSock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes)) < 0)
    {
        throw new std::logic_error("sockopt() failed");
    }
}

ASockServer::~ASockServer()
{
#ifdef __DEBUG
    std::cerr << "close" << std::endl;
#endif
    close(this->servSock);
}