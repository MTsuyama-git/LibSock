#include <TcpClient.hpp>
#include <ConnectConfig.hpp>
#include <exception>
#include <iostream>
#include <unistd.h>

int main(int argc, char **argv)
{
    try
    {
        TcpClient client(ConnectConfig::ConnectConfig4(8080, inet_addr("127.0.0.1")));
        int servSock = client.serv_sock();
        write(servSock, "Hello\r\n", 7);
        close(servSock);
    }
    catch (std::logic_error e)
    {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}