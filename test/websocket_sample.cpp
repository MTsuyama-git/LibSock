#include <TcpServer.hpp>
#include <BindConfig.hpp>
#include <StrUtils.hpp>
#include <unistd.h>   // close()
#include <sys/stat.h> // struct stat
#include <libgen.h>
#include <stdexcept>
#include <iostream>
#include <filesystem>

#define SERVER_SIGNATURE "tiny-server 1.0.0"
#define RESP "HTTP/1.1 %d %s\r\n"
#define CONTENT_TYPE "Content-Type: %s; charset=UTF-8\r\nConnection: keep-alive\nServer: " SERVER_SIGNATURE "\n"

#define READ_BUFFER_LEN 2048
#define RESP_BUFFER_LEN 8192

#define PORT_NUMBER 8080

using namespace std::filesystem;

void servWS(void);
void servHTML(int servSock);

char read_buffer[READ_BUFFER_LEN];
char resp_buffer[RESP_BUFFER_LEN];
const path assets_dir = "./statics";

typedef enum
{
    UNKNOWN,
    GET,
    POST,
    PATCH,
    DELETE
} request_type;

typedef struct
{
    int number;
    const char *message;
} resp_status_code_t;

resp_status_code_t statusCodes[] = {
    {100, "Continue"},
    {101, "Switching Protocols"},
    {102, "Processing"},
    {200, "OK"},
    {201, "Created"},
    {204, "No Content"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {500, "Internal Server Error"},
    {503, "Service Unavailable"},
    {999, "xxxxxxxxxxxxxxxxxxxxx"}};

resp_status_code_t *getResponseStatusCode(int);

int main(int argc, char **argv)
{

    // prepare assets directory
    if(!assets_dir.empty() && !exists(assets_dir))
    {
        create_directories(assets_dir);
    }

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
    request_type rtype = request_type::UNKNOWN;
    int offset = 0;

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
    std::string requestLine = StrUtils::split(read_buffer, "\n").at(0);
    std::cout << read_buffer << std::endl;
    if (!strncmp(read_buffer, "GET", 3))
    {
        rtype = request_type::GET;
        offset = 4;
    }
    else if (!strncmp(read_buffer, "POST", 4))
    {
        rtype = request_type::POST;
        offset = 5;
    }
    else {
        rtype = request_type::UNKNOWN;
    }

    resp_status_code_t *respStatusCode = (rtype == request_type::UNKNOWN)? getResponseStatusCode(405) : getResponseStatusCode(200);

    snprintf(resp_buffer, RESP_BUFFER_LEN, RESP, respStatusCode->number, respStatusCode->message);
    write(cliSock, resp_buffer, strlen(resp_buffer));
    snprintf(resp_buffer, RESP_BUFFER_LEN, CONTENT_TYPE, "text/html");
    write(cliSock, resp_buffer, strlen(resp_buffer));
    write(cliSock, "\r\n\r\n", 4);
    snprintf(resp_buffer, RESP_BUFFER_LEN, "<html><head></head><body><h2>It Works!</h2></body></html>");
    write(cliSock, resp_buffer, strlen(resp_buffer));

    write(cliSock, "\r\n\r\n", 4);
    close(cliSock);
}

resp_status_code_t *getResponseStatusCode(int code)
{
    for (int i = 0; statusCodes[i].number != 999; ++i)
    {
        if (code == statusCodes[i].number)
        {
            return &(statusCodes[i]);
        }
    }
    return nullptr;
}