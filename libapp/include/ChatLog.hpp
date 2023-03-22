#pragma once
#include <cstdint>
#include <string>
#include <json/json.hpp>

class ChatLog
{
private:
    uint64_t __time;
    std::string __message;
    std::string __user_id_str;

public:
    uint64_t time(void) const;
    std::string message(void) const;
    std::string user_id_str(void) const;
    nlohmann::json serialize(void) const;
    ChatLog();
    ChatLog(uint64_t time, std::string message, std::string user_id_str);
};