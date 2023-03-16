#include <TcpServer.hpp>
#include <BindConfig.hpp>
#include <StrUtils.hpp>
#include <unistd.h>   // close()
#include <sys/stat.h> // struct stat
#include <libgen.h>
#include <stdexcept>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <map>

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
    if (!assets_dir.empty() && !exists(assets_dir))
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
    std::vector<std::string> requestHeaders = StrUtils::split(read_buffer, "\n");
    std::vector<std::string> requestLine = StrUtils::split(requestHeaders.at(0), " ");
#ifdef __DEBUG
    std::vector<std::string> requestParams;
    std::copy_if(requestHeaders.begin() + 1, requestHeaders.end(), std::back_inserter(requestParams), [](const std::string &item)
                 { return StrUtils::trim(item).length() >= 1; });
    for (std::string r_param : requestParams)
    {
        std::vector<std::string> p = StrUtils::split(r_param, ":");

        std::cout << "\"" << p.at(0) << "\" \"" << StrUtils::trim(p.at(1)) << "\"" << std::endl;
    }
#endif
    if (requestLine.at(0) == "GET")
    {
        rtype = request_type::GET;
    }
    else if (requestLine.at(0) == "POST")
    {
        rtype = request_type::POST;
    }
    else
    {
        rtype = request_type::UNKNOWN;
    }

    std::vector<std::string> requestContent = StrUtils::split(requestLine.at(1), "?");
    std::string request_path = StrUtils::ltrim(requestContent.at(0), "/");
    if(request_path.length() == 0) {
        request_path = "index.html";
    }
    std::cout << "/" << request_path << std::endl;
    std::map<std::string, std::string> params;
    /* std::vector<std::string> params = (requestContent.size() > 1) ? StrUtils::split(requestContent.at(1), "&") : std::vector<std::string>(); */
    // parse params
    if (requestContent.size() > 1)
    {
        for (std::string param : StrUtils::split(requestContent.at(1), "&"))
        {
            try
            {
                std::vector p = StrUtils::split(param, "=");
                params[p[0]] = p[1];
            }
            catch (std::exception e)
            {
                std::cout << "[ERROR]: " << e.what() << std::endl;
            }
        }
        for (auto param : params)
        {
            std::cout << param.first << " => " << param.second << std::endl;
        }
    }

    std::string content_type = "text/html";
    resp_status_code_t *respStatusCode = (rtype == request_type::UNKNOWN) ? getResponseStatusCode(405) : getResponseStatusCode(200);
    if (rtype == request_type::GET)
    {
        std::cout << "GET" << std::endl;
        path file_path = assets_dir / request_path;
        std::cout << "searching: " << assets_dir << " " << assets_dir / request_path << std::endl;
        if (exists(file_path) && is_regular_file(file_path))
        {
            if (file_path.extension() == ".jpg" || file_path.extension() == ".jpeg" || file_path.extension() == ".JPG")
            {
                content_type = "image/jpg";
            }
            else if (file_path.extension() == ".html")
            {
                content_type = "text/html";
            }
        }
        else
        {
            content_type = "text/html";
            respStatusCode = getResponseStatusCode(404);
        }

        // header
        snprintf(resp_buffer, RESP_BUFFER_LEN, RESP, respStatusCode->number, respStatusCode->message);
        write(cliSock, resp_buffer, strlen(resp_buffer));
        snprintf(resp_buffer, RESP_BUFFER_LEN, CONTENT_TYPE, content_type.c_str());
        write(cliSock, resp_buffer, strlen(resp_buffer));
        write(cliSock, "\r\n\r\n", 4);
        // body
        if (respStatusCode->number == 404)
        {
            snprintf(resp_buffer, RESP_BUFFER_LEN, "<html><head></head><body><h2>404 Not Found</h2></body></html>");
            write(cliSock, resp_buffer, strlen(resp_buffer));
        }
        else
        {
            std::fstream ifs(file_path, std::ios::in | std::ios::binary);
            do
            {
                ifs.read(resp_buffer, READ_BUFFER_LEN);
                write(cliSock, resp_buffer, ifs.gcount());
            }while (ifs.gcount() > 0);

            ifs.close();
        }

        write(cliSock, "\r\n\r\n", 4);
        close(cliSock);
    }
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