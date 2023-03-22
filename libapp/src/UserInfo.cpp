#include <UserInfo.hpp>
#include <cmath>
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

static std::string get_uuid_str(void);
static int Base64Encode(char *dest, const char *message);

std::string UserInfo::name(void) const
{
    return __name;
}

std::string UserInfo::user_id(void) const
{
    return __user_id;
}

bool UserInfo::is_match(std::string password) const
{
    unsigned char __buffer[512];
    char __buffer_b64[512];
    Base64Encode(__buffer_b64, (char *)SHA256((const unsigned char *)password.c_str(), password.length(), __buffer));
    return (strcmp(__password.c_str(), __buffer_b64) == 0);
}

UserInfo::UserInfo()
{
}

UserInfo::UserInfo(const std::string &name, const std::string &password) : __name(name)
{
    unsigned char __buffer[512];
    char __buffer_b64[512];
    Base64Encode(__buffer_b64, (char *)SHA256((const unsigned char *)password.c_str(), password.length(), __buffer));
    __password = std::string(__buffer_b64);
    __user_id = get_uuid_str();
}

UserInfo::UserInfo(const std::string &name, const std::string &password, const std::string &user_id) : __name(name)
{
    unsigned char __buffer[512];
    char __buffer_b64[512];
    Base64Encode(__buffer_b64, (char *)SHA256((const unsigned char *)password.c_str(), password.length(), __buffer));
    __password = std::string(__buffer_b64);
    __user_id = user_id;
}

std::map<std::string, std::string> UserInfo::serialize(void) const
{
    std::map<std::string, std::string> ret;
    ret["name"] = name();
    ret["id"] = user_id();
    return ret;
}


static std::string uuid_to_str(const uuid_t &uuid)
{
    uuid_string_t list_uuid_str;
    uuid_unparse_lower(uuid, list_uuid_str);
    // std::regex e("(\\w+)-(\\w+)-(\\w+)-(\\w+)-(\\w+)");
    return std::string(list_uuid_str);
    /* return std::regex_replace(std::string(list_uuid_str), e, "$1$2$3$4$5"); */
}

static std::string get_uuid_str(void)
{
    uuid_t list_uuid;
    uuid_generate(list_uuid);
    return uuid_to_str(list_uuid);
}

int Base64Encode(char *dest, const char *message)
{ // Encodes a string to base64
    BIO *bio, *b64;
    FILE *stream;
    int encodedSize = 4 * ceil((double)SHA_DIGEST_LENGTH / 3);

    stream = fmemopen(dest, encodedSize + 1, "w");
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_fp(stream, BIO_NOCLOSE);
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Ignore newlines - write everything in one line
    BIO_write(bio, message, SHA_DIGEST_LENGTH);
    BIO_flush(bio);
    BIO_free_all(bio);
    fclose(stream);
    return (0); // success
}
