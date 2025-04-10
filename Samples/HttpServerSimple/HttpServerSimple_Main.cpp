#include "BindConfig.hpp"
#include <HttpHeader.hpp>
#include <StrUtils.hpp>
#include <TcpServer.hpp>
#include <algorithm>
#include <array>
#include <bits/types/struct_timeval.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <netinet/in.h>
#include <span>
#include <sstream>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

extern "C"
{
#include <signal.h>
}

namespace fs = std::filesystem;

struct StatusCode
{
    int         number;
    const char* message;
};

enum class ThreadState : uint8_t
{
    Idle,
    Triggered,
    Processing,
};

enum class TaskType : uint8_t
{
    Invalid,
    Http,
    WebSocket,
};

struct ThreadParam
{
    ThreadState state;
    TaskType    task;
    int         descriptor;
    sockaddr_in cliSockAddr;
};

namespace
{

constexpr const char ServerSignature[] = "tiny-server 2.0.0";
constexpr const char RespFormat[]      = "HTTP/1.1 %d %s\r\n";

constexpr StatusCode StatusCodes[]     = {
    {100,              "Continue"},
    {101,   "Switching Protocols"},
    {102,            "Processing"},
    {200,                    "OK"},
    {201,               "Created"},
    {204,            "No Content"},
    {301,     "Moved Permanently"},
    {302,                 "Found"},
    {400,           "Bad Request"},
    {401,          "Unauthorized"},
    {403,             "Forbidden"},
    {404,             "Not Found"},
    {405,    "Method Not Allowed"},
    {500, "Internal Server Error"},
    {503,   "Service Unavailable"},
    {999, "xxxxxxxxxxxxxxxxxxxxx"},
};

constexpr char           AssetsDir[]          = "./statics/chat_app_sample";
constexpr auto           RequestBufferLength  = 2048;
constexpr auto           ResponseBufferLength = 8192;
constexpr int            DefaultPortNumber    = 8080;

volatile sig_atomic_t    g_Eflag              = 0;
std::vector<std::thread> g_Threads;

void                     AbortHandler(int sig) noexcept;
void                     HandleHttpRequest(int cliSock) noexcept;

const StatusCode*        getResponseStatusCode(int code) noexcept;

void                     HandlePackets(ThreadParam* pParam) noexcept;

void                     HandleHttp(
                        int descriptor, std::span<uint8_t> requestBuffer, std::span<uint8_t> responseBuffer) noexcept;
void HandleWebSocket(
    int descriptor, std::span<uint8_t> requestBuffer, std::span<uint8_t> responseBuffer) noexcept;

} // namespace

int main(int argc, char** argv) noexcept
{
    socklen_t                            clientLen = sizeof(sockaddr_in);
    struct sockaddr_in                   cliSockAddr; // client internet socket address
    int                                  portNumber  = DefaultPortNumber;
    fs::path                             assetsDir   = AssetsDir;
    constexpr auto                       ThreadCount = 4;
    std::array<std::thread, ThreadCount> threads;
    std::array<ThreadParam, ThreadCount> params;
    struct timeval                       timeout;
    int                                  readValue;
    fd_set                               set;

    timeout.tv_sec  = 0;
    timeout.tv_usec = 900;

    for (auto& param : params)
    {
        param = {ThreadState::Idle, TaskType::Invalid, -1};
    }

    if (argc >= 2)
    {
        portNumber = atoi(argv[1]);
    }

    if (signal(SIGINT, AbortHandler) == SIG_ERR)
    {
        exit(1);
    }

    // prepare assets directory
    if (!assetsDir.empty() && !exists(assetsDir))
    {
        create_directories(assetsDir);
    }

    TcpServer server(BindConfig::BindConfig4Any(portNumber), SOCK_STREAM);
    int       servSock = server.serv_sock();

    std::cout << "Listening on Port" << portNumber << "\n";

    for (auto& thread : threads)
    {
        auto index = std::distance(threads.begin(), &thread);
        thread     = std::thread(HandlePackets, &params[index]);
    }

    while (g_Eflag == 0)
    {
        FD_ZERO(&set);          /* clear the set */
        FD_SET(servSock, &set); /* add our file descriptor to the set */
        readValue = select(servSock + 1, &set, NULL, NULL, &timeout);
        if (readValue <= 0)
        {
            continue;
        }

        auto* pParam = params.end();
        do
        {
            pParam =
                std::find_if(params.begin(), params.end(), [](const ThreadParam& param) -> bool {
                    return param.state == ThreadState::Idle;
                });
        } while (params.end() <= pParam);

        if ((pParam->descriptor =
                 accept(servSock, (struct sockaddr*)&pParam->cliSockAddr, &clientLen))
            < 0)
        {
            std::cerr << "accept() failed." << std::endl;
            continue;
        }
        pParam->task  = TaskType::Http;
        pParam->state = ThreadState::Triggered;

        // TODO: websocket の処理
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    std::cout << "End\n";

    return 0;
}

namespace
{

void AbortHandler(int sig) noexcept
{
    g_Eflag = 1;
}

void HandleHttpRequest(int cliSock) noexcept
{
    struct timeval timeout;
    int            readValue;
    fd_set         set;

    timeout.tv_sec  = 0;
    timeout.tv_usec = 900;

    FD_ZERO(&set);         /* clear the set */
    FD_SET(cliSock, &set); /* add our file descriptor to the set */
    readValue = select(cliSock + 1, &set, NULL, NULL, &timeout);

    if (readValue <= 0)
    {
        close(cliSock);
        return;
    }

    auto* read_buffer = new char[RequestBufferLength]();
    auto  acceptLen   = read(cliSock, read_buffer, RequestBufferLength);
    if (acceptLen <= 0)
    {
        close(cliSock);
        delete[] (read_buffer);
        return;
    }
    read_buffer[acceptLen] = 0;
    HttpHeader  requestHeader(read_buffer, acceptLen);
    std::string request_path = StrUtils::ltrim(requestHeader.Path, "/");
    if (request_path.length() == 0)
    {
        request_path = "index.html";
    }

    std::string content_type   = "text/html";
    const auto* respStatusCode = (requestHeader.type() == request_type::UNKNOWN) ?
                                     getResponseStatusCode(405) :
                                     getResponseStatusCode(200);
    std::map<std::string, std::string> responseHeader;
    if (requestHeader.type() == request_type::GET)
    {
        // std::cout << "GET" << std::endl;
        fs::path file_path = fs::path(AssetsDir) / request_path;
        // std::cout << "searching: " << assets_dir << " " << assets_dir /
        // request_path << std::endl;
        if (exists(file_path) && is_regular_file(file_path))
        {
            if (file_path.extension() == ".jpg" || file_path.extension() == ".jpeg"
                || file_path.extension() == ".JPG")
            {
                responseHeader["Content-Type"] = "image/jpg";
            }
            else if (file_path.extension() == ".ico")
            {
                responseHeader["Content-Type"] = "image/x-icon";
            }
            else if (file_path.extension() == ".html")
            {
                responseHeader["Content-Type"] = "text/html; charset=UTF-8";
            }
            else if (file_path.extension() == ".js")
            {
                responseHeader["Content-Type"]           = "text/javascript; charset=UTF-8";
                responseHeader["X-Content-Type-Options"] = "nosniff";
            }
            else if (file_path.extension() == ".css")
            {
                responseHeader["Content-Type"]           = "text/css; charset=UTF-8";
                responseHeader["X-Content-Type-Options"] = "nosniff";
            }
        }
    }
}

const StatusCode* getResponseStatusCode(int code) noexcept
{
    for (int i = 0; StatusCodes[i].number != 999; ++i)
    {
        if (code == StatusCodes[i].number)
        {
            return &(StatusCodes[i]);
        }
    }
    return nullptr;
}

void HandlePackets(ThreadParam* pParam) noexcept
{
    auto* requestBuffer  = new uint8_t[RequestBufferLength]();
    auto* responseBuffer = new uint8_t[ResponseBufferLength]();

    while (g_Eflag == 0)
    {
        if (pParam->state == ThreadState::Idle)
        {
            continue;
        }

        pParam->state = ThreadState::Processing;
        switch (pParam->task)
        {
            case TaskType::Http:
            {
                HandleHttp(
                    pParam->descriptor,
                    {requestBuffer, RequestBufferLength},
                    {responseBuffer, ResponseBufferLength});
                break;
            }
            case TaskType::WebSocket:
            {
                HandleWebSocket(
                    pParam->descriptor,
                    {requestBuffer, RequestBufferLength},
                    {responseBuffer, ResponseBufferLength});
                break;
            }
            case TaskType::Invalid:
                break;
        }

        pParam->task  = TaskType::Invalid;
        pParam->state = ThreadState::Idle;
    }

    delete[] (requestBuffer);
}

void HandleHttp(
    int descriptor, std::span<uint8_t> requestBuffer, std::span<uint8_t> responseBuffer) noexcept
{
    struct timeval timeout;
    static fd_set  set;
    int            readValue;

    timeout.tv_sec  = 0;
    timeout.tv_usec = 900;

    FD_ZERO(&set);            /* clear the set */
    FD_SET(descriptor, &set); /* add our file descriptor to the set */

    readValue = select(descriptor + 1, &set, NULL, NULL, &timeout);

    if (readValue <= 0)
    {
        close(descriptor);
        return;
    }
    // TODO
    // #if defined(__DEBUG) && defined(__VERBOSE)
    //     std::cout << "connected from " << inet_ntoa(cliSockAddr.sin_addr) << "." << std::endl;
    // #endif
    auto acceptLen = read(descriptor, requestBuffer.data(), requestBuffer.size_bytes());

    if (acceptLen <= 0)
    {
        close(descriptor);
        return;
    }
    requestBuffer[acceptLen] = '\0';
    HttpHeader  requestHeader(reinterpret_cast<char*>(requestBuffer.data()), acceptLen);
    std::string requestPath = StrUtils::ltrim(requestHeader.Path, "/");
    if (requestPath.length() == 0)
    {
        requestPath = "index.html";
    }
    std::string contentType    = "text/html";
    const auto* respStatusCode = (requestHeader.type() == request_type::UNKNOWN) ?
                                     getResponseStatusCode(405) :
                                     getResponseStatusCode(200);
    std::map<std::string, std::string> responseHeader;
    if (requestHeader.type() == request_type::GET)
    {
        //std::cout << "GET" << std::endl;
        auto file_path = fs::path(AssetsDir) / requestPath;
        // std::cout << "searching: " << assets_dir << " " << assets_dir / request_path << std::endl;
        if (exists(file_path) && is_regular_file(file_path))
        {
            if (file_path.extension() == ".jpg" || file_path.extension() == ".jpeg"
                || file_path.extension() == ".JPG")
            {
                responseHeader["Content-Type"] = "image/jpg";
            }
            else if (file_path.extension() == ".ico")
            {
                responseHeader["Content-Type"] = "image/x-icon";
            }
            else if (file_path.extension() == ".html")
            {
                responseHeader["Content-Type"] = "text/html; charset=UTF-8";
            }
            else if (file_path.extension() == ".js")
            {
                responseHeader["Content-Type"]           = "text/javascript; charset=UTF-8";
                responseHeader["X-Content-Type-Options"] = "nosniff";
            }
            else if (file_path.extension() == ".css")
            {
                responseHeader["Content-Type"]           = "text/css; charset=UTF-8";
                responseHeader["X-Content-Type-Options"] = "nosniff";
            }
        }
        else
        {
            responseHeader["Content-Type"] = "text/html; charset=UTF-8";
            respStatusCode                 = getResponseStatusCode(404);
        }

        auto responesSize = snprintf(
            reinterpret_cast<char*>(responseBuffer.data()),
            responseBuffer.size_bytes(),
            RespFormat,
            respStatusCode->number,
            respStatusCode->message);

        write(descriptor, responseBuffer.data(), responesSize);

        if (respStatusCode->number == 404)
        {
            for (auto header_item : responseHeader)
            {
                std::ostringstream oss;
                oss << header_item.first << ": " << header_item.second << "\r\n";
                std::string header_item_line = oss.str();
                write(descriptor, header_item_line.c_str(), header_item_line.length());
            }
            write(descriptor, "\r\n", 2);
            auto responseSize = snprintf(
                reinterpret_cast<char*>(responseBuffer.data()),
                responseBuffer.size(),
                "<html><head></head><body><h2>404 Not Found</h2></body></html>");
            write(descriptor, responseBuffer.data(), responseSize);
        }
        else
        {
            std::fstream ifs(file_path, std::ios::in | std::ios::binary);
            ifs.seekg(0, std::ios_base::end);
            responseHeader["Content-Length"] = std::to_string(ifs.tellg());
            ifs.seekg(0, std::ios_base::beg);
            for (auto header_item : responseHeader)
            {
                std::ostringstream oss;
                oss << header_item.first << ": " << header_item.second << "\r\n";
                std::string header_item_line = oss.str();
                write(descriptor, header_item_line.c_str(), header_item_line.length());
            }
            write(descriptor, "\r\n", 2);

            do
            {
                ifs.read(
                    reinterpret_cast<char*>(responseBuffer.data()), responseBuffer.size_bytes());
                write(descriptor, responseBuffer.data(), ifs.gcount());
            } while (ifs.gcount() > 0);

            ifs.close();
        }

        write(descriptor, "\r\n\r\n", 4);
        close(descriptor);
    }
    else if (requestHeader.type() == request_type::UPGRADE)
    {
//         // https://developer.mozilla.org/ja/docs/Web/API/WebSockets_API/Writing_WebSocket_servers
//         respStatusCode = getResponseStatusCode(101);
//         std::string websocket_key = requestHeader.SecWebSocketKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// #if defined(__DEBUG)
//         std::cout << "websocket_key: \"" << websocket_key <<  "\"" << std::endl;
// #endif
//         SHA1((const unsigned char *)websocket_key.c_str(), websocket_key.length(), websocket_sha1_buf);
//         Base64Encode((char *)websocket_b64_buf, (const char *)websocket_sha1_buf);
// #if defined(__DEBUG)
//         std::cout << "websocket_key(B64): \"" << websocket_b64_buf <<  "\"" << std::endl;
// #endif
//         snprintf(resp_buffer, RESP_BUFFER_LEN, RESP, respStatusCode->number, respStatusCode->message);
//         write(cliSock, resp_buffer, strlen(resp_buffer));
// #if defined(__DEBUG) && defined(__UPGRADE)
//         write(1, resp_buffer, strlen(resp_buffer));
// #endif
//         snprintf(resp_buffer, RESP_BUFFER_LEN, "Upgrade: websocket\r\nConnection: upgrade\r\nSec-WebSocket-Accept: %s\n\n", websocket_b64_buf);
// #if defined(__DEBUG) && defined(__UPGRADE)
//         write(1, resp_buffer, strlen(resp_buffer));
// #endif
//         write(cliSock, resp_buffer, strlen(resp_buffer));
//         wsClients.push_back(cliSock);
//     }
// #if defined(__DEBUG) && defined(__VERBOSE)
//     std::cout << "END========> servHTML" << std::endl;
// #endif
    }
}

void HandleWebSocket(
    int descriptor, std::span<uint8_t> requestBuffer, std::span<uint8_t> responseBuffer) noexcept
{
}

} // namespace
