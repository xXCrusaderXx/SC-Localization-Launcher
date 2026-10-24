#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "backend/utils/ZipWriter.hpp"

// Reads the archives ZipWriter writes: stored entries only, every entry checked against its CRC-32.
// Anything else (compressed entries, broken or foreign archives) is rejected as a whole.
namespace ZipReader
{

struct File
{
    std::string name;
    std::string data;
};

namespace detail
{

inline uint16_t get16 (const std::string& data, size_t pos)
{
    return static_cast<uint16_t>(static_cast<unsigned char>(data[pos]) | static_cast<unsigned char>(data[pos + 1]) << 8);
}

inline uint32_t get32 (const std::string& data, size_t pos) { return get16(data, pos) | static_cast<uint32_t>(get16(data, pos + 2)) << 16; }

}  // namespace detail

inline std::optional<std::vector<File>> read (const std::filesystem::path& path)
{
    using detail::get16;
    using detail::get32;
    std::ifstream in(path, std::ios::binary);
    if(!in.is_open()) return std::nullopt;
    std::string data((std::istreambuf_iterator<char>(in)), {});
    // End of central directory, no archive comment
    if(data.size() < 22 || get32(data, data.size() - 22) != 0x06054b50) return std::nullopt;
    size_t end = data.size() - 22;
    uint16_t count = get16(data, end + 10);
    size_t pos = get32(data, end + 16);

    std::vector<File> files;
    for(uint16_t i = 0; i < count; ++i)
    {
        if(pos + 46 > end || get32(data, pos) != 0x02014b50) return std::nullopt;
        uint16_t method = get16(data, pos + 10);
        uint32_t crc = get32(data, pos + 16);
        uint32_t size = get32(data, pos + 20);
        uint32_t stored = get32(data, pos + 24);
        uint16_t nameLength = get16(data, pos + 28);
        size_t skip = 46 + nameLength + get16(data, pos + 30) + get16(data, pos + 32);
        uint32_t offset = get32(data, pos + 42);
        if(method != 0 || size != stored || pos + skip > end) return std::nullopt;
        std::string name = data.substr(pos + 46, nameLength);
        pos += skip;

        if(static_cast<size_t>(offset) + 30 > data.size() || get32(data, offset) != 0x04034b50) return std::nullopt;
        size_t start = static_cast<size_t>(offset) + 30 + get16(data, offset + 26) + get16(data, offset + 28);
        if(start + size > data.size()) return std::nullopt;
        std::string content = data.substr(start, size);
        if(ZipWriter::crc32(content) != crc) return std::nullopt;
        files.push_back({std::move(name), std::move(content)});
    }
    return files;
}

}  // namespace ZipReader
