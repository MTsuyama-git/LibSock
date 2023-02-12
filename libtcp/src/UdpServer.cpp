#include <UdpServer.hpp>

#include <iostream>   // cout, cerr
#include <fstream>    // ifstream, ofstream
#include <sstream>    // istringstream
#include <cstdlib>    // atoi(), exit(), EXIT_FAILURE, EXIT_SUCCESS
#include <cstring>    // memset()
#include <unistd.h>   // close()
#include <sys/stat.h> // struct stat
#include <libgen.h>
#include <cmath>
#include <sys/socket.h> //// socket(), bind(), accept(), listen()
#include <arpa/inet.h>  // struct sockaddr_in, struct sockaddr, inet_ntoa()

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

int UdpServer::serv_sock(void)
{
    return servSock;
}