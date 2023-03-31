#pragma once
#include <string>
#include <map>
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

#ifndef _UUID_STRING_T
#define _UUID_STRING_T
typedef char _uuid_string_t[UUID_STR_LEN];
typedef _uuid_string_t uuid_string_t;
#endif

class UserInfo
{
private:
    std::string __name;
    std::string __user_id;
    std::string __password;
public:
private:
public:
UserInfo();
UserInfo(const std::string& name, const std::string& password, const std::string& user_id);
UserInfo(const std::string& name, const std::string& password);
std::string name(void) const;
std::string user_id(void) const;
std::map<std::string, std::string> serialize(void) const;
bool is_match(std::string password) const;
};