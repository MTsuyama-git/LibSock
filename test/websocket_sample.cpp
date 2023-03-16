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
#include <cmath>
#include <map>

extern "C"
{
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
}

#define SERVER_SIGNATURE "tiny-server 1.0.0"
#define RESP "HTTP/1.1 %d %s\r\n"
#define CONTENT_TYPE "Content-Type: %s; charset=UTF-8\r\nConnection: keep-alive\nServer: " SERVER_SIGNATURE "\n"

#define READ_BUFFER_LEN 2048
#define RESP_BUFFER_LEN 8192

#define PORT_NUMBER 8080

using namespace std::filesystem;

void servWS(void);
void servHTML(int servSock);
void printBuffer(void *buffer, size_t length);

char read_buffer[READ_BUFFER_LEN];
char resp_buffer[RESP_BUFFER_LEN];
unsigned char websocket_sha1_buf[512];
unsigned char websocket_b64_buf[512];

std::vector<int> wsClients;
const path assets_dir = "./statics";

typedef enum
{
    UNKNOWN,
    GET,
    POST,
    PATCH,
    DELETE,
    UPGRADE,
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
int Base64Encode(char *dest, const char *message);

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
    static struct timeval timeout;
    static int rv;
    static fd_set set;
    static char read_buffer[READ_BUFFER_LEN];                                       // read buffer
    static const unsigned char text[] = {0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f}; // hello
    static int acceptLen;
    int i;
    timeout.tv_sec = 0;
    timeout.tv_usec = 900;
    for (int wsClient : wsClients)
    {
        // readのtimeout
        FD_ZERO(&set);          /* clear the set */
        FD_SET(wsClient, &set); /* add our file descriptor to the set */
        rv = select(wsClient + 1, &set, NULL, NULL, &timeout);
        if (rv <= 0)
            continue;
        acceptLen = read(wsClient, read_buffer, READ_BUFFER_LEN);
        if (acceptLen == 0)
            continue;
        read_buffer[acceptLen] = 0;
        std::cout << "accept: " << std::flush;
        printBuffer(read_buffer, acceptLen);
        size_t start = 0;
        while (start < acceptLen)
        {
            std::cout << start << "/" << acceptLen << std::endl;
            std::cout << (((read_buffer[start] >> 7) & 1) ? "is fin" : "cont") << std::endl;
            uint8_t opcode = (read_buffer[start] & 0xF);
            uint8_t mask = (read_buffer[start + 1] >> 7) & 0x1;
            unsigned long payloadlen = (read_buffer[start + 1] & 0x7F);
            uint8_t payloadstart = start + 2;
            if (payloadlen == 126)
            {
                payloadlen = (read_buffer[start + 3] << 8) | read_buffer[start + 2];
                payloadstart += 2;
            }
            if (payloadlen == 127)
            {
                payloadlen = (read_buffer[start + 5] << 24) | read_buffer[start + 4] << 16 | (read_buffer[start + 3] << 8) | read_buffer[start + 2];
                payloadstart += 4;
            }
            uint32_t maskkey = 0x00;
            if (mask)
            {
                maskkey = ((read_buffer[payloadstart + 3] & 0xFF) << 24) | (read_buffer[payloadstart + 2] & 0xFF) << 16 | ((read_buffer[payloadstart + 1] & 0xFF) << 8) | (read_buffer[payloadstart] & 0xFF);
                printf("maskkey: 0x%02X 0x%02X 0x%02X 0x%02X\n", read_buffer[payloadstart] & 0xFF, read_buffer[payloadstart + 1] & 0xFF, read_buffer[payloadstart + 2] & 0xFF, read_buffer[payloadstart + 3] & 0xFF);
                std::cout << "maskkey:" << maskkey << std::endl;
                payloadstart += 4;
            }
            std::cout << "isMask: " << ((mask)? "true" : "false") << std::endl;
            std::cout << "payloadlen:" << payloadlen << std::endl;
            if (opcode == 0x0)
            {
                std::cout << "Continuous frame" << std::endl;
            }
            else if (opcode == 0x1)
            {
                std::cout << "Text frame" << std::endl;
                char *ptr = (char *)(&maskkey);
                /* char *ptr = (char *)(&maskkey);
                if (mask)
                {
                    for (int i = 0; i < 4; ++i)
                    {
                        printf("0x%02X ", ptr[i] & 0xFF);
                    }
                    printf("\n");
                } */
                for (unsigned long i = payloadstart; i < payloadstart + payloadlen; ++i)
                {
                    if (mask)
                    {
                        printf("%c", (read_buffer[i] ^ ((ptr[(i - payloadstart) % 4] & 0xFF)) & 0xFF));
                    }
                    else
                    {
                        printf("%c", read_buffer[i]);
                    }
                }
                write(wsClient, text, sizeof(text));
            }
            else if (opcode == 0x2)
            {
                std::cout << "Binary frame" << std::endl;
            }
            else if (opcode == 0x8)
            {
                std::cout << "Open frame" << std::endl;
            }
            else if (opcode == 0x9)
            {
                std::cout << "ping" << std::endl;
            }
            else if (opcode == 0xA)
            {
                std::cout << "pong" << std::endl;
            }
            start = payloadstart + payloadlen;
        }
        close(wsClient);
    }
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
    read_buffer[acceptLen] = 0;
    std::cout << "read_buffer: " << read_buffer << std::endl;
    std::vector<std::string> requestHeaders = StrUtils::split(read_buffer, "\n");
    std::vector<std::string> requestLine = StrUtils::split(requestHeaders.at(0), " ");
    std::map<std::string, std::string> requestHeader;
    std::vector<std::string> requestHeaderLines;
    std::copy_if(requestHeaders.begin() + 1, requestHeaders.end(), std::back_inserter(requestHeaderLines), [](const std::string &item)
                 { return StrUtils::trim(item).length() >= 1; });
    for (std::string r_param : requestHeaderLines)
    {
        std::vector<std::string> p = StrUtils::split(r_param, ":");
        if (p.size() < 2)
        {
            continue;
        }
#ifdef __DEBUG
        // std::cout << "\"" << p.at(0) << "\" \"" << StrUtils::trim(p.at(1)) << "\"" << std::endl;
#endif
        requestHeader[p.at(0)] = StrUtils::trim(p.at(1));
    }

    std::cout << "requestLine: " << requestHeaders.at(0) << std::endl;
    if (requestLine.at(0) == "GET")
    {
        if (requestHeader.count("Upgrade") != 0 && requestHeader["Upgrade"] == "websocket")
        {
            rtype = request_type::UPGRADE;
            std::cout << "Upgrade: " << requestHeader["Upgrade"] << std::endl;
            std::cout << "Sec-WebSocket-Key: " << requestHeader["Sec-WebSocket-Key"] << std::endl;
        }
        else
        {
            rtype = request_type::GET;
        }
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
    if (request_path.length() == 0)
    {
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
        // std::cout << "GET" << std::endl;
        path file_path = assets_dir / request_path;
        // std::cout << "searching: " << assets_dir << " " << assets_dir / request_path << std::endl;
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
            } while (ifs.gcount() > 0);

            ifs.close();
        }

        write(cliSock, "\r\n\r\n", 4);
        close(cliSock);
    }
    else if (rtype == request_type::UPGRADE)
    {
        // https://developer.mozilla.org/ja/docs/Web/API/WebSockets_API/Writing_WebSocket_servers
        respStatusCode = getResponseStatusCode(101);
        std::string websocket_key = requestHeader["Sec-WebSocket-Key"] + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

        SHA1((const unsigned char *)websocket_key.c_str(), websocket_key.length(), websocket_sha1_buf);
        Base64Encode((char *)websocket_b64_buf, (const char *)websocket_sha1_buf);
        snprintf(resp_buffer, RESP_BUFFER_LEN, RESP, respStatusCode->number, respStatusCode->message);
        write(cliSock, resp_buffer, strlen(resp_buffer));
        snprintf(resp_buffer, RESP_BUFFER_LEN, "Upgrade: websocket\r\nConnection: upgrade\r\nSec-WebSocket-Accept: %s\n\n", websocket_b64_buf);
        write(cliSock, resp_buffer, strlen(resp_buffer));
        wsClients.push_back(cliSock);
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

int Base64Encode(char *dest, const char *message)
{ // Encodes a string to base64
    BIO *bio, *b64;
    FILE *stream;
    int encodedSize = 4 * ceil((double)SHA_DIGEST_LENGTH / 3);

    stream = fmemopen(dest, encodedSize + 1, "w");
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_fp(stream, BIO_NOCLOSE);
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Ignore newlines - write everything in one line
    BIO_write(bio, message, SHA_DIGEST_LENGTH);
    BIO_flush(bio);
    BIO_free_all(bio);
    fclose(stream);
    return (0); // success
}

void printBuffer(void *buffer, size_t length)
{
    uint8_t *ubuf = (uint8_t *)buffer;
    for (size_t i = 0; i < length; ++i)
    {
        printf("0x%02X ", ubuf[i]);
    }
    printf("\n");
}