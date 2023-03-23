#pragma once
#include <string>

class Base64
{
private:
    static const char b64_table[];
    static const char reverse_table[];

public:
    static std::string encode(const std::string &decoded);
    static std::string decode(const std::string &encoded);
    static size_t decode(uint8_t *dest, const std::string &encoded);
};