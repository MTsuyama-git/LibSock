#include <UdpServer.hpp>
#include <BindConfig.hpp>
#include <unistd.h>   // close()
#include <sys/stat.h> // struct stat
#include <libgen.h>
#include <stdexcept>
#include <iostream>

#define READ_BUFFER_LEN 2048

int main(int argc, char **argv)
{
    try
    {
        UdpServer server(BindConfig::BindConfig4Any(8080), SOCK_DGRAM);
        int servSock = server.serv_sock();
        static int cliSock;
        static struct sockaddr_in cliSockAddr;    // client internet socket address
        static char read_buffer[READ_BUFFER_LEN]; // read buffer
        static unsigned int clientLen = sizeof(cliSockAddr);
        ; // client internet socket address length
        static int acceptLen;
        socklen_t sin_size;

        // timeout
        static struct timeval timeout;
        static int rv;
        static fd_set set;
        int i;

        while (1)
        {
            timeout.tv_sec = 0;
            timeout.tv_usec = 900;
            FD_ZERO(&set);          /* clear the set */
            FD_SET(servSock, &set); /* add our file descriptor to the set */
            rv = select(servSock + 1, &set, NULL, NULL, &timeout);
            if (rv <= 0)
                continue;

            /* if ((cliSock = accept(servSock, (struct sockaddr *)&cliSockAddr, &clientLen)) < 0)
            {
                throw new std::logic_error("accept failed");
            } */
            acceptLen = recvfrom(servSock, read_buffer, sizeof(read_buffer), 0, (struct sockaddr *)&cliSockAddr, &sin_size);
            /*         acceptLen = read(cliSock, read_buffer, READ_BUFFER_LEN); */
            /*         read_buffer[acceptLen] = 0; */
            std::cout << "connected from " << inet_ntoa(cliSockAddr.sin_addr) << "." << std::endl;
            for (int i = 0; i < acceptLen; ++i)
            {
                printf("%02X ", read_buffer[i] & 0xFF);
            }
            printf("\n");
            // std::cout << read_buffer << std::endl;
            sendto(servSock, "Hello", 5, 0, (struct sockaddr *)&cliSockAddr, sin_size);
            close(cliSock);
            /* break; */
        }
    }
    catch (std::logic_error *e)
    {
        std::cerr << "ERROR:" << e->what() << std::endl;
    }

    return 0;
}