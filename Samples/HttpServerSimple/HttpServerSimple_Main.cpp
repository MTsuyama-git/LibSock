#include "BindConfig.hpp"
#include <ChatLog.hpp>
#include <HttpHeader.hpp>
#include <StrUtils.hpp>
#include <TcpServer.hpp>
#include <UserInfo.hpp>
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
#include <json/json.hpp>
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
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <signal.h>
#include <uuid/uuid.h>
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

enum class WsFrameType : uint8_t
{
    Continuous = 0x00,
    Text       = 0x01,
    Binary     = 0x02,
    Close      = 0x08,
    Ping       = 0x09,
    Pong       = 0x0A,
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

constexpr char                  AssetsDir[]          = "./statics/chat_app_sample";
constexpr auto                  RequestBufferLength  = 2048;
constexpr auto                  ResponseBufferLength = 8192;
constexpr int                   DefaultPortNumber    = 8080;

volatile sig_atomic_t           g_Eflag              = 0;
std::vector<std::thread>        g_Threads;
std::vector<int>                g_WsClients;
std::map<int, std::string>      g_SocketMap;
std::map<std::string, UserInfo> g_UserInfoMap;
std::vector<ChatLog>            g_ChatLogList;

void                            AbortHandler(int sig) noexcept;
void                            HandleHttpRequest(int cliSock) noexcept;

const StatusCode*               getResponseStatusCode(int code) noexcept;

void                            HandlePackets(ThreadParam* pParam) noexcept;

void                            HandleHttp(
                               int                       descriptor,
                               const std::span<uint8_t>& requestBuffer,
                               const std::span<uint8_t>& responseBuffer,
                               const std::span<uint8_t>& websocketSha1Buffer,
                               const std::span<uint8_t>& websocketBase64Buffer) noexcept;
void HandleWebSocket(
    int                       descriptor,
    const std::span<uint8_t>& requestBuffer,
    const std::span<uint8_t>& responseBuffer) noexcept;

int  Base64Encode(char* dest, const char* message) noexcept;

void SendWsMessage(
    int                             sock,
    WsFrameType                     type,
    const std::span<const uint8_t>& data,
    const std::span<uint8_t>&       responseBuffer) noexcept;

void App(
    int                       descriptor,
    const nlohmann::json&     request,
    const std::span<uint8_t>& responseBuffer) noexcept;

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
        for (auto& wsClient : g_WsClients)
        {
            // readのtimeout
            FD_ZERO(&set);          /* clear the set */
            FD_SET(wsClient, &set); /* add our file descriptor to the set */
            timeout.tv_sec  = 0;
            timeout.tv_usec = 900;
            readValue       = select(wsClient + 1, &set, NULL, NULL, &timeout);
            if (readValue <= 0)
            {
                continue;
            }

            auto* pParam = params.end();
            do
            {
                pParam = std::find_if(
                    params.begin(), params.end(), [](const ThreadParam& param) -> bool {
                        return param.state == ThreadState::Idle;
                    });
            } while (params.end() <= pParam);

            pParam->descriptor = wsClient;
            pParam->task       = TaskType::WebSocket;
            pParam->state      = ThreadState::Triggered;
        }
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
    auto* requestBuffer      = new uint8_t[RequestBufferLength]();
    auto* responseBuffer     = new uint8_t[ResponseBufferLength]();
    auto* websocketSha1Buf   = new uint8_t[SHA_DIGEST_LENGTH];
    auto* websocketBase64Buf = new uint8_t[512];

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
                    {responseBuffer, ResponseBufferLength},
                    {websocketSha1Buf, SHA_DIGEST_LENGTH},
                    {websocketBase64Buf, 512});
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
    int                       descriptor,
    const std::span<uint8_t>& requestBuffer,
    const std::span<uint8_t>& responseBuffer,
    const std::span<uint8_t>& websocketSha1Buffer,
    const std::span<uint8_t>& websocketBase64Buffer) noexcept
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
        // https://developer.mozilla.org/ja/docs/Web/API/WebSockets_API/Writing_WebSocket_servers
        respStatusCode = getResponseStatusCode(101);
        std::string websocket_key =
            requestHeader.SecWebSocketKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

#if defined(__DEBUG) && defined(__VERBOSE)
        std::cout << "websocket_key: \"" << websocket_key << "\"" << std::endl;
#endif
        SHA1(
            (const unsigned char*)websocket_key.c_str(),
            websocket_key.length(),
            websocketSha1Buffer.data());
        Base64Encode((char*)websocketBase64Buffer.data(), (const char*)websocketSha1Buffer.data());
#if defined(__DEBUG) && defined(__VERBOSE)
        std::cout << "websocket_key(B64): \"" << websocketBase64Buffer.data() << "\"" << std::endl;
#endif
        auto responseSize = snprintf(
            reinterpret_cast<char*>(responseBuffer.data()),
            responseBuffer.size_bytes(),
            RespFormat,
            respStatusCode->number,
            respStatusCode->message);
        write(descriptor, responseBuffer.data(), responseSize);
#if defined(__DEBUG) && defined(__VERBOSE) && defined(__UPGRADE)
        write(1, resp_buffer, strlen(resp_buffer));
#endif
        responseSize = snprintf(
            reinterpret_cast<char*>(responseBuffer.data()),
            responseBuffer.size_bytes(),
            "Upgrade: websocket\r\nConnection: upgrade\r\nSec-WebSocket-Accept: %s\n\n",
            websocketBase64Buffer.data());
#if defined(__DEBUG) && defined(__VERBOSE) && defined(__UPGRADE)
        write(1, resp_buffer, strlen(resp_buffer));
#endif
        write(descriptor, responseBuffer.data(), responseSize);
        g_WsClients.push_back(descriptor);
    }
#if defined(__DEBUG) && defined(__VERBOSE)
    std::cout << "END========> servHTML" << std::endl;
#endif
}

void HandleWebSocket(
    int                       descriptor,
    const std::span<uint8_t>& requestBuffer,
    const std::span<uint8_t>& responseBuffer) noexcept
{
    struct timeval timeout;
    static fd_set  set;
    int            readValue;

    timeout.tv_sec  = 0;
    timeout.tv_usec = 900;

    FD_ZERO(&set);            /* clear the set */
    FD_SET(descriptor, &set); /* add our file descriptor to the set */

    auto acceptLen = read(descriptor, requestBuffer.data(), requestBuffer.size_bytes());
    if (acceptLen <= 0)
    {
        return;
    }
    requestBuffer[acceptLen] = '\0';
#if defined(__DEBUG) && defined(__VERBOSE)
    std::cout << "accept: " << std::flush;
    printBuffer(requestBuffer.data(), acceptLen);
#endif
    size_t start = 0;
    while (start < acceptLen)
    {
#if defined(__DEBUG) && defined(__VERBOSE)
        std::cout << start << "/" << acceptLen << std::endl;
        std::cout << (((read_buffer[start] >> 7) & 1) ? "is fin" : "cont") << std::endl;
#endif
        auto          opcode       = static_cast<WsFrameType>(requestBuffer[start] & 0xF);
        auto          mask         = ((requestBuffer[start + 1] >> 7) & 0x1) != 0;
        unsigned long payloadlen   = (requestBuffer[start + 1] & 0x7F);
        uint8_t       payloadstart = start + 2;
        if (payloadlen == 126)
        {
            payloadlen = (requestBuffer[start + 2] << 8) | requestBuffer[start + 3];
            payloadstart += 2;
        }
        if (payloadlen == 127)
        {
            payloadlen = (requestBuffer[start + 2] << 24) | requestBuffer[start + 3] << 16
                         | (requestBuffer[start + 4] << 8) | requestBuffer[start + 5];
            payloadstart += 4;
        }
        uint32_t maskkey = 0x00;
        if (mask)
        {
            maskkey = ((requestBuffer[payloadstart + 3] & 0xFF) << 24)
                      | (requestBuffer[payloadstart + 2] & 0xFF) << 16
                      | ((requestBuffer[payloadstart + 1] & 0xFF) << 8)
                      | (requestBuffer[payloadstart] & 0xFF);

#if defined(__DEBUG) && defined(__VERBOSE)
            printf(
                "maskkey: 0x%02X 0x%02X 0x%02X 0x%02X\n",
                requestBuffer[payloadstart] & 0xFF,
                requestBuffer[payloadstart + 1] & 0xFF,
                requestBuffer[payloadstart + 2] & 0xFF,
                requestBuffer[payloadstart + 3] & 0xFF);
            std::cout << "maskkey:" << maskkey << std::endl;
#endif
            payloadstart += 4;
        }
#if defined(__DEBUG) && defined(__VERBOSE)
        std::cout << "isMask: " << ((mask) ? "true" : "false") << std::endl;
        std::cout << "payloadlen:" << payloadlen << std::endl;
#endif
        if (opcode == WsFrameType::Continuous)
        {
#if defined(__DEBUG) && defined(__VERBOSE)
            std::cout << "Continuous frame" << std::endl;
#endif
        }
        else if (opcode == WsFrameType::Text)
        {
#if defined(__DEBUG) && defined(__VERBOSE)
            std::cout << "Text frame" << std::endl;
#endif
            char* ptr = (char*)(&maskkey);
            if (mask)
            {
                for (unsigned long i = payloadstart; i < payloadstart + payloadlen; ++i)
                {
                    requestBuffer[i] ^= ((ptr[(i - payloadstart) % 4] & 0xFF)) & 0xFF;
                }
            }
            requestBuffer[payloadstart + payloadlen] = '\0';
            App(descriptor, nlohmann::json::parse(&requestBuffer[payloadstart]), responseBuffer);
        }
        else if (opcode == WsFrameType::Binary)
        {
#if defined(__DEBUG) && defined(__VERBOSE)
            std::cout << "Binary frame" << std::endl;
#endif
        }
        else if (opcode == WsFrameType::Close)
        {
#if defined(__DEBUG) && defined(__VERBOSE)
            std::cout << "Close frame" << std::endl;
#endif
            close(descriptor);
            g_SocketMap.erase(descriptor);
        }
        else if (opcode == WsFrameType::Ping)
        {
#if defined(__DEBUG) && defined(__VERBOSE)
            std::cout << "ping" << std::endl;
#endif
            SendWsMessage(
                descriptor,
                WsFrameType::Pong,
                {reinterpret_cast<const uint8_t*>(""), 0},
                responseBuffer);
        }
        else if (opcode == WsFrameType::Pong)
        {
#if defined(__DEBUG) && defined(__VERBOSE)
            std::cout << "pong" << std::endl;
#endif
        }
        start = payloadstart + payloadlen;
    }
    // close(wsClient);
}

int Base64Encode(char* dest, const char* message) noexcept
{ // Encodes a string to base64
    BIO*  bio;
    BIO*  b64;
    FILE* stream;
    int   encodedSize = 4 * ceil((double)SHA_DIGEST_LENGTH / 3);

    stream            = fmemopen(dest, encodedSize + 1, "w");
    b64               = BIO_new(BIO_f_base64());
    bio               = BIO_new_fp(stream, BIO_NOCLOSE);
    bio               = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Ignore newlines - write everything in one line
    BIO_write(bio, message, SHA_DIGEST_LENGTH);
    BIO_flush(bio);
    BIO_free_all(bio);
    fclose(stream);
    return (0); // success
}

void SendWsMessage(
    int                             sock,
    WsFrameType                     type,
    const std::span<const uint8_t>& data,
    const std::span<uint8_t>&       responseBuffer) noexcept
{
    size_t  offset = 2;
    // first byte
    uint8_t is_fin = 0x80; //            : 0b10000000
    uint8_t RSV123 = 0x00; // not in use : 0b01110000
    //uint8_t opcode = type & 0xF; //      : 0b00001111
    // second byte
    uint8_t mask   = 0x00; //            : 0b10000000 the message shoud be masked
    uint8_t payloadLen;    //            : 0b01111111 125 <= or 126, 127
    uint16_t
        exPayloadLen2; //            : the length of the payload that is used when the payload Len is 126
    uint32_t
        exPayloadLen4; //            : the length of the payload that is used when the payload Len is 127

    uint32_t maskKey    = 0xDEADBEAF;
    uint8_t* maskKeyArr = (uint8_t*)(&maskKey);

    if (data.size_bytes() <= 125)
    {
        payloadLen = data.size_bytes();
    }
    else if (data.size_bytes() <= 65535)
    {
        payloadLen    = 126;
        exPayloadLen2 = data.size_bytes();
    }
    else
    {
        payloadLen    = 127;
        exPayloadLen4 = data.size_bytes();
    }
    responseBuffer[0] = is_fin | RSV123 | (static_cast<uint8_t>(type) & 0xF);
    responseBuffer[1] = mask | payloadLen;
    if (payloadLen == 126)
    {
        responseBuffer[2] = ((exPayloadLen2 >> 8) & 0xFF);
        responseBuffer[3] = ((exPayloadLen2) & 0xFF);

        /* this code not works on little endian  */
        /* memcpy(&responseBuffer [2], &exPayloadLen2, sizeof(uint16_t)); */

        offset += 2;
    }
    else if (payloadLen == 127)
    {
        responseBuffer[2] = ((exPayloadLen4 >> 24) & 0xFF);
        responseBuffer[3] = ((exPayloadLen4 >> 16) & 0xFF);
        responseBuffer[4] = ((exPayloadLen4 >> 8) & 0xFF);
        responseBuffer[5] = ((exPayloadLen4) & 0xFF);

        /* this code not works on little endian  */
        /* memcpy(&responseBuffer [2], &exPayloadLen4, sizeof(uint32_t)); */

        offset += 4;
    }

    if (mask != 0)
    {
        auto count = 0UL;
        for (const auto& value : data)
        {
            responseBuffer[offset + count] = value ^ maskKeyArr[count % 4];
            ++count;
        }
    }
    else
    {
        auto count = 0UL;
        for (const auto& value : data)
        {
            responseBuffer[offset + count] = value;
            ++count;
        }
    }

    write(sock, responseBuffer.data(), offset + data.size_bytes());
}

void App(
    int                       descriptor,
    const nlohmann::json&     request,
    const std::span<uint8_t>& responseBuffer) noexcept
{
    std::string              cmd  = request["cmd"].get<std::string>();
    std::vector<std::string> args = request["args"].get<std::vector<std::string>>();

    nlohmann::json           response;
    if (cmd == "request")
    {
        uuid_t sock_uuid;
        if (args.size() >= 2 && args.at(0) == "set_userid")
        {
            if (g_UserInfoMap.count(args.at(1)) == 0)
            {
                response["subject"] = "Error";
                response["message"] = "Unknown author error";
            }
            else
            {

                uuid_string_t list_uuid_str;
                g_SocketMap[descriptor] = args.at(1);
                uuid_parse(args.at(1).c_str(), sock_uuid);
                response["subject"] = "set_userid";
                uuid_unparse_lower(sock_uuid, list_uuid_str);
                response["authorid"] = std::string(list_uuid_str);
                std::string message  = response.dump();
                for (auto m : g_SocketMap)
                {
                    SendWsMessage(
                        m.first,
                        WsFrameType::Text,
                        {reinterpret_cast<const uint8_t*>(message.c_str()), message.length()},
                        responseBuffer);
                }
                return;
            }
        }
        else if (args.size() >= 2 && args.at(0) == "set_username")
        {
            UserInfo info(args.at(1), "P@assW0rd");
            g_UserInfoMap[info.user_id()] = info;
            response["subject"]           = "set_username";
            response["authorid"]          = info.user_id();
        }
        else if (args.size() >= 1 && args.at(0) == "userinfo")
        {
            response["subject"] = "userinfo";
            std::vector<std::map<std::string, std::string>> body;
            for (auto user_info : g_UserInfoMap)
            {
                body.push_back(user_info.second.serialize());
            }
            response["body"] = body;
        }
        else if (args.size() >= 1 && args.at(0) == "history")
        {
            response["subject"] = "history";
            std::vector<nlohmann::json> body;
            for (auto chat_log : g_ChatLogList)
            {
                body.emplace_back(chat_log.serialize());
            }
            response["body"] = body;
        }
        std::string message = response.dump();
        SendWsMessage(
            descriptor,
            WsFrameType::Text,
            {reinterpret_cast<const uint8_t*>(message.c_str()), message.length()},
            responseBuffer);
    }
    else if (cmd == "msg" && args.size() >= 1)
    {
        if (g_SocketMap.contains(descriptor))
        {
            response["subject"]  = "onMessage";
            response["authorid"] = g_SocketMap[descriptor];
            response["message"]  = args.at(0);
            response["time"]     = request["time"];
            g_ChatLogList.emplace_back(request["time"], args.at(0), g_SocketMap[descriptor]);
        }
        else
        {
            response["subject"] = "Error";
            response["message"] = "Unknown author error";
        }
        std::string message_str = response.dump();
        for (auto map : g_SocketMap)
        {
            SendWsMessage(
                map.first,
                WsFrameType::Text,
                {reinterpret_cast<const uint8_t*>(message_str.c_str()), message_str.length()},
                responseBuffer);
        }
    }
}

} // namespace
