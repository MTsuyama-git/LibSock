#include <UUID.hpp>

const char UUID::__c_table[__UUID_BODY_LENGTH] = {
    '0',
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    'a',
    'b',
    'c',
    'd',
    'e',
    'f'};

UUID::UUID()
{
    memset(__body, 0, sizeof(uint8_t) * __UUID_BODY_LENGTH);
}

std::string UUID::encode(const uint8_t &ch)
{
    return std::string{__c_table[(ch >> 4) & 0xF], __c_table[(ch)&0xF]};
}

uint8_t UUID::decode(const uint8_t &v)
{
    if ('0' <= v && v <= '9')
    {
        return v - '0';
    }
    else if('A' <= v && v <= 'F')
    {
        return 10 + v - 'A';
    }
    else if('a' <= v && v <= 'f')
    {
        return 10 + v - 'a';
    }
    return 0;
}

UUID &UUID::operator=(const UUID &in) &
{
    memcpy(this->__body, in.__body, sizeof(uint8_t) * __UUID_BODY_LENGTH);
}

UUID::operator std::string &() const
{
    std::string ret = "";
    for (int i = 0; i < __UUID_BODY_LENGTH; ++i)
    {
        ret += UUID::encode(__body[i]);
    }
    return ret;
}