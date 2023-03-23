#include <Base64.hpp>
#include <string>
#include <iostream>
#include <limits>
#include <cassert>

const char Base64::b64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const char Base64::reverse_table[128] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
    64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64};

std::string Base64::encode(const std::string &encoded)
{
    if (encoded.size() > (std::numeric_limits<std::string::size_type>::max() / 4u) * 3u)
    {
        throw std::length_error("Converting too large a string to base64.");
    }

    const std::size_t encodedLen = encoded.size();
    // padding
    std::string ret((((encodedLen + 2) / 3) * 4), '=');
    std::size_t outpos = 0;
    int bits_collected = 0;
    unsigned int accumulator = 0;
    const std::string::const_iterator end = encoded.end();
    for (std::string::const_iterator i = encoded.begin(); i != end; ++i)
    {
        accumulator = (accumulator << 8) | (*i & 0xffu);
        bits_collected += 8;
        while (bits_collected >= 6)
        {
            bits_collected -= 6;
            ret[outpos++] = b64_table[(accumulator >> bits_collected) & 0x3fu];
        }
    }
    if (bits_collected > 0)
    {
        assert(bits_collected < 6);
        accumulator <<= 6 - bits_collected;
        ret[outpos++] = b64_table[accumulator & 0x3fu];
    }
    assert(outpos >= ret.size() - 2);
    assert(outpos <= ret.size());

    return ret;
}

size_t Base64::decode(uint8_t *dest, const std::string &encoded)
{
    uint8_t *d = dest;
    uint32_t accumulator = 0;
    int bits_collected;

    for (auto i = encoded.begin(); i != encoded.end(); ++i)
    {
        const int c = *i;
        if (std::isspace(c) || c == '=')
        {
            continue;
        }
        if ((c > 127) || (c < 0) || reverse_table[c] > 63)
        {
            throw std::invalid_argument("This contains characters not legal in a base64 encoded string.");
        }
        accumulator = (accumulator << 6) | reverse_table[c];
        bits_collected += 6;
        if (bits_collected >= 8)
        {
            bits_collected -= 8;
            *(d++) += static_cast<uint8_t>((accumulator >> bits_collected & 0xffu));
        }
    }
    return d - dest;
}

std::string Base64::decode(const std::string &encoded)
{
    std::string ret;
    const std::string::const_iterator last = encoded.end();
    int bits_collected = 0;
    unsigned int accumulator = 0;

    for (std::string::const_iterator i = encoded.begin(); i != last; ++i)
    {
        const int c = *i;
        if (std::isspace(c) || c == '=')
        {
            // skip white space and padding
            continue;
        }
        if (((c > 127) || (c < 0) || reverse_table[c] > 63))
        {
            throw std::invalid_argument("This contains characters not legal in a base64 encoded string.");
        }
        accumulator = (accumulator << 6) | reverse_table[c];
        bits_collected += 6;
        if (bits_collected >= 8)
        {
            bits_collected -= 8;
            ret += static_cast<char>((accumulator >> bits_collected & 0xffu));
        }
    }
    return ret;
}