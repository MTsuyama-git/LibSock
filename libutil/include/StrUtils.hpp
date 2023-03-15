#pragma once
#include <string>
#include <vector>

class StrUtils
{
public:
    static std::vector<std::string> split(const std::string &str, const std::string &del);
};