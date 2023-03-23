#include <TcpClient.hpp>
#include <ConnectConfig.hpp>
#include <exception>
#include <iostream>
#ifdef _MSC_VER
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h> // close()
#endif

int main(int argc, char **argv)
{
    try
    {
        TcpClient client(ConnectConfig::ConnectConfig4(8080, inet_addr("127.0.0.1")), SOCK_STREAM);
        int servSock = client.serv_sock();
        ssize_t length = write(servSock, "Hello\r\n", 7);
        close(servSock);
    }
    catch (std::logic_error e)
    {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}