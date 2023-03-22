#include <ChatLog.hpp>

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