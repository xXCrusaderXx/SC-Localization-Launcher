#pragma once

#include <sodium.h>
#include <array>
#include <cstring>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// Verifies minisign signatures (https://jedisct1.github.io/minisign/) as the release workflow makes them
// with `minisign -S`. Nothing of a launcher update is installed unless this succeeds.
namespace Minisign
{

struct PublicKey
{
    std::array<unsigned char, 8> keyId{};
    std::array<unsigned char, crypto_sign_PUBLICKEYBYTES> key{};
};

namespace detail
{

inline std::vector<std::string> lines (const std::string& text)
{
    std::vector<std::string> result;
    std::istringstream stream(text);
    std::string line;
    while(std::getline(stream, line))
    {
        if(!line.empty() && line.back() == '\r') line.pop_back();
        if(!line.empty()) result.push_back(line);
    }
    return result;
}

inline std::optional<std::vector<unsigned char>> base64Decode (const std::string& text)
{
    std::vector<unsigned char> bytes(text.size());
    size_t length = 0;
    if(sodium_base642bin(bytes.data(), bytes.size(), text.data(), text.size(), nullptr, &length, nullptr, sodium_base64_VARIANT_ORIGINAL) != 0)
        return std::nullopt;
    bytes.resize(length);
    return bytes;
}

}  // namespace detail

// The second line of a minisign .pub file: base64("Ed" + key ID + Ed25519 public key)
inline std::optional<PublicKey> parsePublicKey (const std::string& text)
{
    if(sodium_init() < 0) return std::nullopt;
    for(const auto& line : detail::lines(text))
    {
        if(line.starts_with("untrusted comment:")) continue;
        auto bytes = detail::base64Decode(line);
        if(!bytes || bytes->size() != 2 + 8 + crypto_sign_PUBLICKEYBYTES || (*bytes)[0] != 'E' || (*bytes)[1] != 'd') return std::nullopt;
        PublicKey key;
        std::memcpy(key.keyId.data(), bytes->data() + 2, 8);
        std::memcpy(key.key.data(), bytes->data() + 10, crypto_sign_PUBLICKEYBYTES);
        return key;
    }
    return std::nullopt;
}

struct Result
{
    bool valid = false;
    std::string trustedComment;
    std::string error;
};

// Checks that `data` was signed with `key` and that the trusted comment belongs to that signature.
// Accepts the prehashed "ED" format (minisign's default) and the legacy "Ed" format.
inline Result verify (const PublicKey& key, const std::string& signatureText, const std::string& data)
{
    Result result;
    if(sodium_init() < 0)
    {
        result.error = "libsodium could not be initialised";
        return result;
    }

    // untrusted comment, signature, trusted comment, global signature
    auto lines = detail::lines(signatureText);
    if(lines.size() < 4 || !lines[0].starts_with("untrusted comment:") || !lines[2].starts_with("trusted comment: "))
    {
        result.error = "malformed signature file";
        return result;
    }

    auto signature = detail::base64Decode(lines[1]);
    if(!signature || signature->size() != 2 + 8 + crypto_sign_BYTES)
    {
        result.error = "malformed signature";
        return result;
    }
    if(std::memcmp(signature->data() + 2, key.keyId.data(), 8) != 0)
    {
        result.error = "signed with another key";
        return result;
    }

    const unsigned char* signatureBytes = signature->data() + 10;
    bool prehashed = (*signature)[0] == 'E' && (*signature)[1] == 'D';
    bool legacy = (*signature)[0] == 'E' && (*signature)[1] == 'd';
    if(!prehashed && !legacy)
    {
        result.error = "unknown signature algorithm";
        return result;
    }

    int verified = -1;
    if(prehashed)
    {
        std::array<unsigned char, crypto_generichash_BYTES_MAX> hash{};
        crypto_generichash(hash.data(), hash.size(), reinterpret_cast<const unsigned char*>(data.data()), data.size(), nullptr, 0);
        verified = crypto_sign_verify_detached(signatureBytes, hash.data(), hash.size(), key.key.data());
    }
    else
    {
        verified = crypto_sign_verify_detached(signatureBytes, reinterpret_cast<const unsigned char*>(data.data()), data.size(), key.key.data());
    }
    if(verified != 0)
    {
        result.error = "signature does not match";
        return result;
    }

    // The global signature covers the signature and the trusted comment, so the comment cannot be swapped
    std::string trustedComment = lines[2].substr(std::strlen("trusted comment: "));
    auto globalSignature = detail::base64Decode(lines[3]);
    std::vector<unsigned char> signed_(signatureBytes, signatureBytes + crypto_sign_BYTES);
    signed_.insert(signed_.end(), trustedComment.begin(), trustedComment.end());
    if(!globalSignature || globalSignature->size() != crypto_sign_BYTES ||
       crypto_sign_verify_detached(globalSignature->data(), signed_.data(), signed_.size(), key.key.data()) != 0)
    {
        result.error = "trusted comment is not signed";
        return result;
    }

    result.valid = true;
    result.trustedComment = trustedComment;
    return result;
}

}  // namespace Minisign
