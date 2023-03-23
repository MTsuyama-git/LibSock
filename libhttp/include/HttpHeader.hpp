#pragma once
#include <map>
#include <string>
#include <iostream>
#include <json/json.hpp>
#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#include <unistd.h>
#endif
typedef enum
{
    UNKNOWN,
    GET,
    POST,
    PATCH,
    PUT,
    DELETE,
    UPGRADE,
} request_type;

class HttpHeader
{
private:
public:
    const request_type __type;
    std::string Path;
    std::map<std::string, std::string> Params;
    std::string Connection;
    std::string Host;
    std::string Upgrade;
    std::string SecWebSocketKey;
private:
static request_type judge_request_type(const char* read_buffer, const ssize_t& length);
public:    
    HttpHeader(const char *read_buffer, const ssize_t& length);
    request_type type(void) const;
    friend nlohmann::json& operator<<(nlohmann::json&, const HttpHeader&);
    friend std::ostream& operator<<(std::ostream&, const HttpHeader&);

};

// Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7
// Accept-Encoding: gzip, deflate, br
// Accept-Language: ja,en-US;q=0.9,en;q=0.8,zh-CN;q=0.7,zh;q=0.6,th;q=0.5
// Connection: keep-alive
// Host: localhost:3001
// Referer: http://localhost:3001/chat.html
// sec-ch-ua: "Google Chrome";v="111", "Not(A:Brand";v="8", "Chromium";v="111"
// sec-ch-ua-mobile: ?0
// sec-ch-ua-platform: "macOS"
// Sec-Fetch-Dest: document
// Sec-Fetch-Mode: navigate
// Sec-Fetch-Site: same-origin
// Upgrade-Insecure-Requests: 1
// User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/111.0.0.0 Safari/537.36