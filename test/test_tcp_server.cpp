#include <TcpServer.hpp>
#include <BindConfig.hpp>
#include <unistd.h>   // close()
#include <sys/stat.h> // struct stat
#include <libgen.h>
#include <stdexcept>
#include <iostream>

#define READ_BUFFER_LEN 2048

int main(int argc, char **argv)
{
    TcpServer server(BindConfig::BindConfig4Any(8080), SOCK_STREAM);
    int servSock = server.serv_sock();

    static int cliSock;
    static struct sockaddr_in cliSockAddr;    // client internet socket address
    static char read_buffer[READ_BUFFER_LEN]; // read buffer
    static unsigned int clientLen = sizeof(cliSockAddr);
    ; // client internet socket address length
    static int acceptLen;

    // timeout
    static struct timeval timeout;
    static int rv;
    static fd_set set;
    int i;
    timeout.tv_sec = 0;
    timeout.tv_usec = 900;

    while (1)
    {
        FD_ZERO(&set);          /* clear the set */
        FD_SET(servSock, &set); /* add our file descriptor to the set */
        rv = select(servSock + 1, &set, NULL, NULL, &timeout);
        if (rv <= 0)
            continue;

        if ((cliSock = accept(servSock, (struct sockaddr *)&cliSockAddr, &clientLen)) < 0)
        {
            throw new std::logic_error("accept failed");
        }
        std::cout << "connected from " << inet_ntoa(cliSockAddr.sin_addr) << "." << std::endl;
        acceptLen = read(cliSock, read_buffer, READ_BUFFER_LEN);
        if(acceptLen <= 0) {
            std::cout << "Closed: " << std::endl;
            close(cliSock);
            continue;
        }
        read_buffer[acceptLen] = 0;
        std::cout << "Message: " << read_buffer << " acceptLen: " << acceptLen << std::endl;
    }
    return 0;
}