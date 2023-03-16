#pragma once
#include <string>
#include <vector>

class StrUtils
{
public:
    static std::vector<std::string> split(const std::string &str, const std::string &del);
    static std::string trim(std::string &string, const char *trimCharacterList = " \t\v\r\n");
    static std::string trim(const std::string &string, const char *trimCharacterList = " \t\v\r\n");
    static std::string ltrim(std::string &string, const char *trimCharacterList = " \t\v\r\n");
    static std::string ltrim(const std::string &string, const char *trimCharacterList = " \t\v\r\n");
};