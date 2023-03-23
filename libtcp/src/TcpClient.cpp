#include <TcpClient.hpp>
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

TcpClient::TcpClient(sockaddr_in conn_addr4, int connection) : ASock(SocketConfig::TcpConfig(PF_INET, connection))
{
    if (connect(this->servSock, (struct sockaddr *)&conn_addr4, sizeof(conn_addr4)) < 0)
    {
        throw new std::logic_error("connect() failed");
    }
}

TcpClient::TcpClient(sockaddr_in6 conn_addr6, int connection) : ASock(SocketConfig::TcpConfig(PF_INET, connection))
{
    if (connect(this->servSock, (struct sockaddr *)&conn_addr6, sizeof(conn_addr6)) < 0)
    {
        throw new std::logic_error("connect() failed");
    }
}

SOCKET TcpClient::serv_sock(void) {
    return servSock;
}