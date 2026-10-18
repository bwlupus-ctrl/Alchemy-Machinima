#include "linden_common.h"
#include "alvcammediafilepolicy.h"
#include <openssl/sha.h>
namespace ALMediaFile
{
std::string cacheKey(const std::string& url)
{
    const auto key = url.substr(0, url.find('#'));
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(key.data()), key.size(), hash);
    std::string out;
    const char* hexDigits = "0123456789abcdef";
    for (unsigned char c : hash) { out += hexDigits[c >> 4]; out += hexDigits[c & 15]; }
    return out;
}
}
