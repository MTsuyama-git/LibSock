#include <BindConfig.hpp>
#include <ChatLog.hpp>
#include <HttpHeader.hpp>
#include <StrUtils.hpp>
#include <TcpServer.hpp>
#include <UserInfo.hpp>
#include <algorithm>
#include <arpa/inet.h>
#include <bits/types/struct_timeval.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <json/json.hpp>
#include <linux/limits.h>
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

#include "ThreadManager.h"

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
    ThreadState  state;
    sockaddr_in  clientInfo;
    int          descriptor;
    request_type requestType;
    fs::path     requestPath;
    std::string  secWebSocketKey;
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

struct HttpRequest
{
    request_type requestType;
    std::string  requestPath;
    std::string  secWebSocketKey;
};

constexpr char                  AssetsDir[]               = "./statics/chat_app_sample";
constexpr auto                  RequestBufferSize         = 2048;
constexpr auto                  ResponseBufferSize        = 8192;
constexpr auto                  WebSocketBase64BufferSize = 512;
constexpr int                   DefaultPortNumber         = 8080;
constexpr int                   ThreadCount               = 5;

volatile sig_atomic_t           g_Eflag                   = 0;
std::vector<int>                g_WsClients;
std::vector<int>                g_RemovedWsClients;
std::map<int, std::string>      g_SocketMap;
std::map<std::string, UserInfo> g_UserInfoMap;
std::vector<ChatLog>            g_ChatLogList;
std::mutex                      g_WsClientMutex;
std::mutex                      g_AddTaskMutex;
using ThreadManagerType = ThreadManager<ThreadCount, ResponseBufferSize, WebSocketBase64BufferSize>;

ThreadManagerType g_ThreadManager;

void              AbortHandler(int sig) noexcept;
void              HandleHttpGetRequest(
                 const std::span<char>& responseBuffer, int descriptor, const std::string& path) noexcept;
void HandleWebSocketUpgrade(
    const std::span<char>& responseBuffer,
    const std::span<char>& websocketBase64Buffer,
    int                    descriptor,
    const std::string&     secWebSocketKey) noexcept;

const StatusCode* getResponseStatusCode(int code) noexcept;

void              HandlePackets(
                 const ThreadManagerType::ThreadParam& param,
                 const std::span<char>&                responseBuffer,
                 const std::span<char>&                websocketBase64Buffer) noexcept;

void ProcessWsClients() noexcept;

void HandleWebSocket(
    int                    descriptor,
    const std::span<char>& requestBuffer,
    const std::span<char>& responseBuffer) noexcept;

int  Base64Encode(char* dest, uint8_t (&message)[SHA_DIGEST_LENGTH]) noexcept;

void SendWsMessage(
    int                             sock,
    WsFrameType                     type,
    const std::span<const uint8_t>& data,
    const std::span<char>&          responseBuffer) noexcept;

void App(
    int descriptor, const nlohmann::json& request, const std::span<char>& responseBuffer) noexcept;

HttpRequest           ParseHttpRequest(const std::span<const char>& requestData) noexcept;

void                  Return404(const std::span<char>& responseBuffer, int descriptor) noexcept;

void                  AddWebSocketClient(int descriptor) noexcept;

bool                  ExistsAsWebSocketClient(int descriptor) noexcept;

constexpr const char* ToString(request_type type) noexcept
{
    switch (type)
    {
        case request_type::GET:
            return "Get";
        case request_type::POST:
            return "Post";
        case request_type::PATCH:
            return "Patch";
        case request_type::PUT:
            return "Put";
        case request_type::DELETE:
            return "Delete";
        case request_type::UPGRADE:
            return "Upgrade";
        case request_type::UNKNOWN:
            return "Unknown";
    }

    return "Unknown";
}

} // namespace

int main(int argc, char** argv) noexcept
{
    auto*              requestBuffer = new char[RequestBufferSize]();
    socklen_t          clientLen     = sizeof(sockaddr_in);
    struct sockaddr_in cliSockAddr; // client internet socket address
    int                portNumber  = DefaultPortNumber;
    fs::path           assetsDir   = AssetsDir;
    constexpr auto     ThreadCount = 4;
    struct timeval     timeout;
    int                readValue;
    fd_set             set;

    timeout.tv_sec  = 0;
    timeout.tv_usec = 900;

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

    std::cout << "Listening on Port: " << portNumber << "\n";

    g_ThreadManager.StartThread(HandlePackets);

    auto wsThread = std::thread(ProcessWsClients);

    while (g_Eflag == 0)
    {
        FD_ZERO(&set);          /* clear the set */
        FD_SET(servSock, &set); /* add our file descriptor to the set */
        readValue = select(servSock + 1, &set, NULL, NULL, &timeout);
        if (readValue <= 0)
        {
            continue;
        }

        int cliSock = -1;

        if ((cliSock = accept(servSock, (struct sockaddr*)&cliSockAddr, &clientLen)) < 0)
        {
            std::cerr << "accept() failed." << std::endl;
            continue;
        }

        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        FD_ZERO(&set);         /* clear the set */
        FD_SET(cliSock, &set); /* add our file descriptor to the set */
        readValue = select(cliSock + 1, &set, NULL, NULL, &timeout);
        if (readValue <= 0)
        {
            close(cliSock);
            continue;
        }
        auto acceptLen = read(cliSock, requestBuffer, RequestBufferSize);

        if (acceptLen <= 0)
        {
            close(cliSock);
            continue;
        }

        auto requestHeader = ParseHttpRequest({requestBuffer, RequestBufferSize});

        g_ThreadManager.SetParam({
            cliSock,
            requestHeader.requestType,
            requestHeader.requestPath,
            requestHeader.secWebSocketKey,
        });

        std::cout << ToString(requestHeader.requestType) << " /" << requestHeader.requestPath << " "
                  << inet_ntoa(cliSockAddr.sin_addr);
    }

    g_ThreadManager.Join();

    if (wsThread.joinable())
    {
        wsThread.join();
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

void HandlePackets(
    const ThreadManagerType::ThreadParam& param,
    const std::span<char>&                responseBuffer,
    const std::span<char>&                websocketBase64Buffer) noexcept
{
    switch (param.requestType)
    {
        case request_type::GET:
        {
            HandleHttpGetRequest(responseBuffer, param.descriptor, param.requestPath);
            break;
        }
        case request_type::UPGRADE:
        {
            HandleWebSocketUpgrade(
                responseBuffer, websocketBase64Buffer, param.descriptor, param.secWebSocketKey);

            AddWebSocketClient(param.descriptor);
            break;
        }
        case request_type::UNKNOWN:
        case request_type::POST:
        case request_type::PATCH:
        case request_type::PUT:
        case request_type::DELETE:
        {
            Return404(responseBuffer, param.descriptor);
            break;
        }
    }

    // wsClients にいなければ close
    if (!ExistsAsWebSocketClient(param.descriptor))
    {
        close(param.descriptor);
    }
}

void ProcessWsClients() noexcept
{
    auto*          requestBuffer  = new char[RequestBufferSize]();
    auto*          responseBuffer = new char[ResponseBufferSize]();

    struct timeval timeout;
    int            readValue;
    fd_set         set;

    while (g_Eflag == 0)
    {
        g_WsClientMutex.lock();
        auto tempClients = g_WsClients;
        g_WsClientMutex.unlock();
        g_RemovedWsClients.clear();
        for (auto& wsClient : tempClients)
        {
            FD_ZERO(&set);          /* clear the set */
            FD_SET(wsClient, &set); /* add our file descriptor to the set */
            timeout.tv_sec  = 0;
            timeout.tv_usec = 900;
            readValue       = select(wsClient + 1, &set, NULL, NULL, &timeout);
            if (readValue <= 0)
            {
                continue;
            }

            HandleWebSocket(
                wsClient, {requestBuffer, RequestBufferSize}, {responseBuffer, ResponseBufferSize});
        }
        g_WsClientMutex.lock();
        for (auto& removedClient : g_RemovedWsClients)
        {
            auto itr =
                find_if(g_WsClients.begin(), g_WsClients.end(), [removedClient](int descriptor) {
                    return removedClient == descriptor;
                });
            if (g_WsClients.end() <= itr)
            {
                continue;
            }

            g_WsClients.erase(itr);
        }
        g_WsClientMutex.unlock();
    }

    delete[] (requestBuffer);
    delete[] (responseBuffer);
}

void HandleWebSocket(
    int                    descriptor,
    const std::span<char>& requestBuffer,
    const std::span<char>& responseBuffer) noexcept
{
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
            try
            {
                App(descriptor,
                    nlohmann::json::parse(&requestBuffer[payloadstart]),
                    responseBuffer);
            }
            catch (nlohmann::json_abi_v3_11_2::detail::parse_error e)
            {
                auto requestHeader = ParseHttpRequest(requestBuffer);

                std::cout << ToString(requestHeader.requestType) << " /"
                          << requestHeader.requestPath << " "
                          << "unknown"
                          << "FD " << descriptor << "\n";

                g_ThreadManager.SetParam({
                    descriptor,
                    requestHeader.requestType,
                    requestHeader.requestPath,
                    requestHeader.secWebSocketKey,
                });

                return;
            }
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
            g_RemovedWsClients.push_back(descriptor);
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

int Base64Encode(char* dest, uint8_t (&message)[SHA_DIGEST_LENGTH]) noexcept
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
    const std::span<char>&          responseBuffer) noexcept
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
    int descriptor, const nlohmann::json& request, const std::span<char>& responseBuffer) noexcept
{
    std::string              cmd  = request["cmd"].get<std::string>();
    std::vector<std::string> args = request["args"].get<std::vector<std::string>>();

    nlohmann::json           response;
    if (cmd == "request")
    {
        uuid_t sock_uuid;
        if (args.size() >= 2 && args.at(0) == "set_userid")
        {
            if (!g_UserInfoMap.contains(args.at(1)))
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
                for (auto map : g_SocketMap)
                {
                    SendWsMessage(
                        map.first,
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

HttpRequest ParseHttpRequest(const std::span<const char>& requestData) noexcept
{
    HttpRequest outResult{};

    HttpHeader  requestHeader(requestData.data(), requestData.size_bytes());
    outResult.requestType = requestHeader.type();
    outResult.requestPath = StrUtils::ltrim(requestHeader.Path, "/");
    if (outResult.requestPath.length() == 0)
    {
        outResult.requestPath = "index.html";
    }

    if (outResult.requestType == request_type::UPGRADE)
    {
        outResult.secWebSocketKey = requestHeader.SecWebSocketKey;
    }

    return outResult;
}

void Return404(const std::span<char>& responseBuffer, int descriptor) noexcept
{
    const auto* respStatusCode = getResponseStatusCode(404);

    auto        responseSize   = snprintf(
        reinterpret_cast<char*>(responseBuffer.data()),
        responseBuffer.size_bytes(),
        RespFormat,
        respStatusCode->number,
        respStatusCode->message);

    write(descriptor, responseBuffer.data(), responseSize);
    write(descriptor, "Connection: Close\r\n\r\n", 2);
    responseSize = snprintf(
        reinterpret_cast<char*>(responseBuffer.data()),
        responseBuffer.size(),
        "<html><head></head><body><h2>404 Not Found</h2></body></html>");
    write(descriptor, responseBuffer.data(), responseSize);
}

bool ExistsAsWebSocketClientImpl(int descriptor) noexcept
{
    return std::any_of(g_WsClients.begin(), g_WsClients.end(), [descriptor](int client) -> bool {
        return descriptor == client;
    });
}

void AddWebSocketClient(int descriptor) noexcept
{
    g_WsClientMutex.lock();
    if (ExistsAsWebSocketClientImpl(descriptor))
    {
        g_WsClientMutex.unlock();
        return;
    }

    g_WsClients.push_back(descriptor);

    g_WsClientMutex.unlock();
}

bool ExistsAsWebSocketClient(int descriptor) noexcept
{
    g_WsClientMutex.lock();

    auto outResult = ExistsAsWebSocketClientImpl(descriptor);

    g_WsClientMutex.unlock();

    return outResult;
}

void HandleHttpGetRequest(
    const std::span<char>& responseBuffer, int descriptor, const std::string& path) noexcept
{
    static const auto*                 responseStatusCode = getResponseStatusCode(200);

    std::map<std::string, std::string> responseHeader;

    fs::path                           file_path = fs::path(AssetsDir) / path;
    if (!(exists(file_path) && is_regular_file(file_path)))
    {
        return Return404(responseBuffer, descriptor);
    }

    auto responesSize = snprintf(
        reinterpret_cast<char*>(responseBuffer.data()),
        responseBuffer.size_bytes(),
        RespFormat,
        responseStatusCode->number,
        responseStatusCode->message);

    write(descriptor, responseBuffer.data(), responesSize);

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
    responseHeader["Connection"] = "close";

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
        ifs.read(reinterpret_cast<char*>(responseBuffer.data()), responseBuffer.size_bytes());
        write(descriptor, responseBuffer.data(), ifs.gcount());
    } while (ifs.gcount() > 0);

    ifs.close();
}

void HandleWebSocketUpgrade(
    const std::span<char>& responseBuffer,
    const std::span<char>& websocketBase64Buffer,
    int                    descriptor,
    const std::string&     secWebSocketKey) noexcept
{
    static uint8_t     sha1Buffer[SHA_DIGEST_LENGTH];
    static const auto* respStatusCode = getResponseStatusCode(101);
    // https://developer.mozilla.org/ja/docs/Web/API/WebSockets_API/Writing_WebSocket_servers
    std::string        websocket_key  = secWebSocketKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    SHA1((const unsigned char*)websocket_key.c_str(), websocket_key.length(), sha1Buffer);
    Base64Encode(websocketBase64Buffer.data(), sha1Buffer);

    auto responseSize = snprintf(
        reinterpret_cast<char*>(responseBuffer.data()),
        responseBuffer.size_bytes(),
        RespFormat,
        respStatusCode->number,
        respStatusCode->message);
    write(descriptor, responseBuffer.data(), responseSize);

    responseSize = snprintf(
        reinterpret_cast<char*>(responseBuffer.data()),
        responseBuffer.size_bytes(),
        "Upgrade: websocket\r\nConnection: upgrade\r\nSec-WebSocket-Accept: %s\n\n",
        websocketBase64Buffer.data());

    write(descriptor, responseBuffer.data(), responseSize);

    AddWebSocketClient(descriptor);
}

} // namespace
