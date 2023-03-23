#include <UdpServer.hpp>

#include <iostream>   // cout, cerr
#include <fstream>    // ifstream, ofstream
#include <sstream>    // istringstream
#include <cstdlib>    // atoi(), exit(), EXIT_FAILURE, EXIT_SUCCESS
#include <cstring>    // memset()
#include <sys/stat.h> // struct stat
#include <cmath>
#ifdef _MSC_VER
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h> // close()
#include <libgen.h>
#include <sys/socket.h> //// socket(), bind(), accept(), listen()
#include <arpa/inet.h>  // struct sockaddr_in, struct sockaddr, inet_ntoa()
#endif

#include <stdexcept>

UdpServer::UdpServer(sockaddr_in bind_addr4, int connection, int queue_limit) : ASock(SocketConfig::UdpConfig(PF_INET, connection))
{
    if (bind(this->servSock, (struct sockaddr *)&bind_addr4, sizeof(bind_addr4)) < 0)
    {
        throw new std::logic_error("bind() failed");
    }
}

UdpServer::UdpServer(sockaddr_in6 bind_addr6, int connection, int queue_limit) : ASock(SocketConfig::UdpConfig(PF_INET6, connection))
{
    if (bind(this->servSock, (struct sockaddr *)&bind_addr6, sizeof(bind_addr6)) < 0)
    {
        throw new std::logic_error("bind() failed");
    }
}

SOCKET UdpServer::serv_sock(void)
{
    return servSock;
}