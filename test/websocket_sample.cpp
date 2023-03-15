#include <TcpServer.hpp>
#include <BindConfig.hpp>
#include <unistd.h>   // close()
#include <sys/stat.h> // struct stat
#include <libgen.h>
#include <stdexcept>
#include <iostream>

#define SERVER_SIGNATURE "tiny-server 1.0.0"
#define HEADER_200 "HTTP/1.1 200 OK\r\nContent-Type: %s; charset=UTF-8\nConnection: keep-alive\nServer: " SERVER_SIGNATURE "\n"
#define BODY_200 "Content-Length: %ld\n\n"

#define READ_BUFFER_LEN 2048

#define PORT_NUMBER 8080

void servWS(void);
void servHTML(int servSock);

char read_buffer[READ_BUFFER_LEN];

int main(int argc, char **argv)
{
    TcpServer server(BindConfig::BindConfig4Any(PORT_NUMBER), SOCK_STREAM);
    int servSock = server.serv_sock();

    while (1)
    {
        servWS();
        servHTML(servSock);
    }

    return 0;
}

void servWS(void)
{
}

void servHTML(int servSock)
{
    static int cliSock;
    static size_t acceptLen;
    static struct sockaddr_in cliSockAddr;               // client internet socket address
    static unsigned int clientLen = sizeof(cliSockAddr); // client internet socket address length
    // timeout
    static struct timeval timeout;
    static int rv;
    static fd_set set;
    int i;
    timeout.tv_sec = 0;
    timeout.tv_usec = 900;

    FD_ZERO(&set);          /* clear the set */
    FD_SET(servSock, &set); /* add our file descriptor to the set */
    rv = select(servSock + 1, &set, NULL, NULL, &timeout);
    if (rv <= 0)
    {
        return;
    }
    if ((cliSock = accept(servSock, (struct sockaddr *)&cliSockAddr, &clientLen)) < 0)
    {
        std::cerr << "accept() failed." << std::endl;
        return;
    }
    std::cout << "connected from " << inet_ntoa(cliSockAddr.sin_addr) << "." << std::endl;
    acceptLen = read(cliSock, read_buffer, READ_BUFFER_LEN);
    std::cout << read_buffer << std::endl;
    if (!strncmp(read_buffer, "GET", 3))
    {
    }
    write(cliSock, "HTTP/1.1 200 OK \nContent-Type: text/html; charset=UTF-8\nConenction: keep-alive\nServer: tiny-server 1.0.0\n\n", strlen("HTTP/1.1 200 OK \nContent-Type: text/html; charset=UTF-8\nConenction: keep-alive\nServer: tiny-server 1.0.0\n\n"));
    write(cliSock, "It works!\n\n", 11);
    close(cliSock);
}