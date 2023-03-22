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
#include <sstream>
#include <cmath>
#include <map>
#include <regex>
#include <UserInfo.hpp>
#include <ChatLog.hpp>
#include <json/json.hpp>

extern "C"
{
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <uuid/uuid.h>
}

#define SERVER_SIGNATURE "tiny-server 1.0.0"
#define RESP "HTTP/1.1 %d %s\r\n"

#define READ_BUFFER_LEN 2048
#define SEND_BUFFER_LEN 2048
#define RESP_BUFFER_LEN 8192

#define PORT_NUMBER 8080

using namespace std::filesystem;

typedef enum
{
    UNKNOWN,
    GET,
    POST,
    PATCH,
    DELETE,
    UPGRADE,
} request_type;

typedef enum
{
    Continuous = 0x00,
    Text = 0x01,
    Binary = 0x02,
    Close = 0x08,
    Ping = 0x09,
    Pong = 0x0A,
} ws_frame_type;

typedef struct
{
    int number;
    const char *message;
} resp_status_code_t;

//
typedef struct
{
    uint64_t time;
    uuid_t id;
    std::string message;
    uuid_t author_id;
} chat_info;

typedef struct
{
    uuid_t id;
    std::string name;
} author_info;

typedef struct
{
    int sock;
    uuid_t id;
    uuid_t author_id;
} sock_info;
//

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

char read_buffer[READ_BUFFER_LEN];
char send_buffer[SEND_BUFFER_LEN];
char resp_buffer[RESP_BUFFER_LEN];
unsigned char websocket_sha1_buf[512];
unsigned char websocket_b64_buf[512];

std::vector<chat_info> chat_history;
std::vector<author_info> author_infos;
std::map<int, std::string> sock_map;
std::map<std::string, UserInfo> user_info_map;
std::vector<ChatLog> chat_log_list;

std::vector<int> wsClients;
const path assets_dir = "./statics/chat_app_sample";

resp_status_code_t *getResponseStatusCode(int);
int Base64Encode(char *dest, const char *message);
void servWS(void);
void servHTML(int servSock);
void printBuffer(void *buffer, size_t length);
std::string get_uuid_str(void);
void wsSendMsg(const int &sock, const ws_frame_type &type, const void *data, const size_t &length);
std::string uuid_to_str(const uuid_t &);
void app(int cliSock, const nlohmann::json &);

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
    std::cout << "END" << std::endl;

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
        if (acceptLen <= 0)
            continue;
        read_buffer[acceptLen] = 0;
#ifdef __DEBUG
        std::cout << "accept: " << std::flush;
#endif
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
            std::cout << "isMask: " << ((mask) ? "true" : "false") << std::endl;
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
                if (mask)
                {
                    for (unsigned long i = payloadstart; i < payloadstart + payloadlen; ++i)
                    {
                        read_buffer[i] ^= ((ptr[(i - payloadstart) % 4] & 0xFF)) & 0xFF;
                    }
                }
                read_buffer[payloadstart + payloadlen] = 0;
                app(wsClient, nlohmann::json::parse(&read_buffer[payloadstart]));
            }
            else if (opcode == 0x2)
            {
                std::cout << "Binary frame" << std::endl;
            }
            else if (opcode == ws_frame_type::Close)
            {
                std::cout << "Close frame" << std::endl;
            }
            else if (opcode == 0x9)
            {
                std::cout << "ping" << std::endl;
                wsSendMsg(
                    wsClient,
                    ws_frame_type::Pong,
                    "",
                    0);
            }
            else if (opcode == 0xA)
            {
                std::cout << "pong" << std::endl;
            }
            start = payloadstart + payloadlen;
        }
        // close(wsClient);
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
    std::map<std::string, std::string> responseHeader;
    if (rtype == request_type::GET)
    {
        // std::cout << "GET" << std::endl;
        path file_path = assets_dir / request_path;
        // std::cout << "searching: " << assets_dir << " " << assets_dir / request_path << std::endl;
        if (exists(file_path) && is_regular_file(file_path))
        {
            if (file_path.extension() == ".jpg" || file_path.extension() == ".jpeg" || file_path.extension() == ".JPG")
            {
                responseHeader["ContentType"] = "image/jpg";
            }
            else if (file_path.extension() == ".ico")
            {
                responseHeader["ContentType"] = "image/x-icon";
            }
            else if (file_path.extension() == ".html")
            {
                responseHeader["ContentType"] = "text/html; charset=UTF-8";
            }
        }
        else
        {
            responseHeader["ContentType"] = "text/html; charset=UTF-8";
            respStatusCode = getResponseStatusCode(404);
        }

        // header
        snprintf(resp_buffer, RESP_BUFFER_LEN, RESP, respStatusCode->number, respStatusCode->message);
        write(cliSock, resp_buffer, strlen(resp_buffer));
        // body
        if (respStatusCode->number == 404)
        {
            for (auto header_item : responseHeader)
            {
                std::ostringstream oss;
                oss << header_item.first << ": " << header_item.second << "\r\n";
                std::string header_item_line = oss.str();
                write(cliSock, header_item_line.c_str(), header_item_line.length());
            }
            write(cliSock, "\r\n", 2);
            snprintf(resp_buffer, RESP_BUFFER_LEN, "<html><head></head><body><h2>404 Not Found</h2></body></html>");
            write(cliSock, resp_buffer, strlen(resp_buffer));
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
                write(cliSock, header_item_line.c_str(), header_item_line.length());
            }
            write(cliSock, "\r\n", 2);

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

void wsSendMsg(const int &sock, const ws_frame_type &type, const void *data, const size_t &length)
{
    size_t offset = 2;
    // first byte
    uint8_t is_fin = 0x80;       //            : 0b10000000
    uint8_t RSV123 = 0x00;       // not in use : 0b01110000
    uint8_t opcode = type & 0xF; //      : 0b00001111
    // second byte
    uint8_t mask = 0x00;    //            : 0b10000000 the message shoud be masked
    uint8_t payloadLen;     //            : 0b01111111 125 <= or 126, 127
    uint16_t exPayloadLen2; //            : the length of the payload that is used when the payload Len is 126
    uint32_t exPayloadLen4; //            : the length of the payload that is used when the payload Len is 127

    uint32_t maskKey = 0xDEADBEAF;
    uint8_t *maskKeyArr = (uint8_t *)(&maskKey);

    if (length <= 125)
    {
        payloadLen = length;
    }
    else if (length <= 65535)
    {
        payloadLen = 126;
        exPayloadLen2 = length;
    }
    else
    {
        payloadLen = 127;
        exPayloadLen4 = length;
    }
    send_buffer[0] = is_fin | RSV123 | opcode;
    send_buffer[1] = mask | payloadLen;
    if (payloadLen == 126)
    {
        memcpy(&send_buffer[2], &exPayloadLen2, sizeof(uint16_t));
        offset += 2;
    }
    else if (payloadLen == 127)
    {
        memcpy(&send_buffer[2], &exPayloadLen4, sizeof(uint32_t));
        offset += 4;
    }
    // send_buffer[SEND_BUFFER_LEN]
    uint8_t *udata = (uint8_t *)data;
    if (mask)
    {
        for (size_t i = 0; i < length; ++i)
        {
            send_buffer[offset + i] = udata[i] ^ maskKeyArr[i % 4];
        }
    }
    else
    {
        for (size_t i = 0; i < length; ++i)
        {
            send_buffer[offset + i] = udata[i];
        }
    }

    size_t send_buffer_length = offset + length;
    write(sock, send_buffer, send_buffer_length);
}

std::string uuid_to_str(const uuid_t &uuid)
{
    uuid_string_t list_uuid_str;
    uuid_unparse_lower(uuid, list_uuid_str);
    // std::regex e("(\\w+)-(\\w+)-(\\w+)-(\\w+)-(\\w+)");
    return std::string(list_uuid_str);
    /* return std::regex_replace(std::string(list_uuid_str), e, "$1$2$3$4$5"); */
}

std::string get_uuid_str(void)
{
    uuid_t list_uuid;
    uuid_generate(list_uuid);
    return uuid_to_str(list_uuid);
}

void app(int sock, const nlohmann::json &request)
{
    std::string cmd = request["cmd"].get<std::string>();
    std::vector<std::string> args = request["args"].get<std::vector<std::string>>();
    std::cout << "cmd: " << cmd << std::endl;
    std::cout << "args:" << std::endl;
    for (auto arg : args)
    {
        std::cout << "\t" << arg << std::endl;
    }
    nlohmann::json response;
    if (cmd == "request")
    {
        uuid_t sock_uuid;
        if (args.size() >= 2 && args.at(0) == "set_userid")
        {
            if (user_info_map.count(args.at(1)) == 0)
            {
                response["subject"] = "Error";
                response["message"] = "Unknown author error";
            }
            else
            {
                sock_map[sock] = args.at(1);
                uuid_parse(args.at(1).c_str(), sock_uuid);
                response["subject"] = "set_userid";
                response["authorid"] = uuid_to_str(sock_uuid);
            }
        }
        else if (args.size() >= 2 && args.at(0) == "set_username")
        {
            UserInfo info(args.at(1), "P@assW0rd");
            user_info_map[info.user_id()] = info;
            response["subject"] = "set_username";
            response["authorid"] = info.user_id();
        }
        else if (args.size() >= 1 && args.at(0) == "userinfo")
        {
            response["subject"] = "userinfo";
            std::vector<std::map<std::string, std::string>> body;
            for (auto user_info : user_info_map)
            {
                body.push_back(user_info.second.serialize());
            }
            response["body"] = body;
            std::cout << "response ==>" << response.dump(4) << std::endl;
        }
        else if (args.size() >= 1 && args.at(0) == "history")
        {
            response["subject"] = "history";
            std::vector<nlohmann::json> body;
            for (auto chat_log : chat_log_list)
            {
                body.emplace_back(chat_log.serialize());
            }
            response["body"] = body;
        }
        std::string message_str = response.dump();

        wsSendMsg(
            sock,
            ws_frame_type::Text,
            message_str.c_str(),
            message_str.length());
    }
    else if (cmd == "msg" && args.size() >= 1)
    {
        if (sock_map.count(sock) > 0)
        {
            response["subject"] = "onMessage";
            response["authorid"] = sock_map[sock];
            response["message"] = args.at(0);
            response["time"] = request["time"];
            chat_log_list.emplace_back(request["time"], args.at(0), sock_map[sock]);
        }
        else
        {
            response["subject"] = "Error";
            response["message"] = "Unknown author error";
        }
        std::string message_str = response.dump();

        for (auto m : sock_map)
        {
            wsSendMsg(
                m.first,
                ws_frame_type::Text,
                message_str.c_str(),
                message_str.length());
        }
    }
}

std::string UserInfo::name(void) const
{
    return __name;
}

std::string UserInfo::user_id(void) const
{
    return __user_id;
}

bool UserInfo::is_match(std::string password) const
{
    unsigned char __buffer[512];
    char __buffer_b64[512];
    Base64Encode(__buffer_b64, (char *)SHA256((const unsigned char *)password.c_str(), password.length(), __buffer));
    return (strcmp(__password.c_str(), __buffer_b64) == 0);
}

UserInfo::UserInfo()
{
}

UserInfo::UserInfo(const std::string &name, const std::string &password) : __name(name)
{
    unsigned char __buffer[512];
    char __buffer_b64[512];
    Base64Encode(__buffer_b64, (char *)SHA256((const unsigned char *)password.c_str(), password.length(), __buffer));
    __password = std::string(__buffer_b64);
    __user_id = get_uuid_str();
}

UserInfo::UserInfo(const std::string &name, const std::string &password, const std::string &user_id) : __name(name)
{
    unsigned char __buffer[512];
    char __buffer_b64[512];
    Base64Encode(__buffer_b64, (char *)SHA256((const unsigned char *)password.c_str(), password.length(), __buffer));
    __password = std::string(__buffer_b64);
    __user_id = user_id;
}

std::map<std::string, std::string> UserInfo::serialize(void) const
{
    std::map<std::string, std::string> ret;
    ret["name"] = name();
    ret["id"] = user_id();
    return ret;
}

ChatLog::ChatLog(uint64_t time, std::string message, std::string user_id_str) : __time(time), __message(message), __user_id_str(user_id_str) {}

uint64_t ChatLog::time(void) const { return __time; }
std::string ChatLog::message(void) const { return __message; }
std::string ChatLog::user_id_str(void) const { return __user_id_str; }
nlohmann::json ChatLog::serialize(void) const
{
    nlohmann::json ret;
    ret["time"] = __time;
    ret["message"] = __message;
    ret["authorid"] = __user_id_str;

    return ret;
}

ChatLog::ChatLog(){};