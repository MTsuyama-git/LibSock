#pragma once
#include <string>
#include <cstdint>

#define __UUID_BODY_LENGTH 16

class UUID
{
private:
    uint8_t __body[__UUID_BODY_LENGTH];
    static const char __c_table[__UUID_BODY_LENGTH];
public:
private:
    static std::string encode(const uint8_t&);
    static uint8_t decode(const uint8_t &);
public:
    UUID();
    UUID &operator=(const UUID &) &;
    operator std::string &() const;
};
