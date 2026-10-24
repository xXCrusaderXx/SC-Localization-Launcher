#pragma once

#include <array>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Writes a ZIP archive of text files without compression ("stored"), enough for a problem report that every
// unzip tool and file manager opens. Names are UTF-8, sizes below 4 GB.
class ZipWriter
{
    struct Entry
    {
        std::string name;
        uint32_t crc;
        uint32_t size;
        uint32_t offset;
    };

    std::ofstream out;
    std::vector<Entry> entries;
    uint16_t dosTime = 0;
    uint16_t dosDate = 0;

    void put16 (uint16_t value)
    {
        out.put(static_cast<char>(value & 0xff));
        out.put(static_cast<char>(value >> 8));
    }

    void put32 (uint32_t value)
    {
        put16(static_cast<uint16_t>(value & 0xffff));
        put16(static_cast<uint16_t>(value >> 16));
    }

   public:
    static uint32_t crc32 (const std::string& data)
    {
        static const auto table = []
        {
            std::array<uint32_t, 256> t{};
            for(uint32_t i = 0; i < 256; ++i)
            {
                uint32_t c = i;
                for(int k = 0; k < 8; ++k) c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
                t[i] = c;
            }
            return t;
        }();
        uint32_t crc = 0xffffffffu;
        for(unsigned char byte : data) crc = table[(crc ^ byte) & 0xff] ^ (crc >> 8);
        return crc ^ 0xffffffffu;
    }

    explicit ZipWriter (const std::filesystem::path& path)
        : out(path, std::ios::binary | std::ios::trunc)
    {
        std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_r(&now, &local);
        dosTime = static_cast<uint16_t>(local.tm_hour << 11 | local.tm_min << 5 | local.tm_sec / 2);
        dosDate = static_cast<uint16_t>((local.tm_year - 80) << 9 | (local.tm_mon + 1) << 5 | local.tm_mday);
    }

    bool isOpen () const { return out.is_open(); }

    void add (const std::string& name, const std::string& data)
    {
        Entry entry{name, crc32(data), static_cast<uint32_t>(data.size()), static_cast<uint32_t>(out.tellp())};
        put32(0x04034b50);
        put16(20);
        put16(0x0800);  // names are UTF-8
        put16(0);       // stored
        put16(dosTime);
        put16(dosDate);
        put32(entry.crc);
        put32(entry.size);
        put32(entry.size);
        put16(static_cast<uint16_t>(name.size()));
        put16(0);
        out.write(name.data(), static_cast<std::streamsize>(name.size()));
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        entries.push_back(entry);
    }

    // Writes the central directory; false if anything could not be written
    bool finish ()
    {
        auto start = static_cast<uint32_t>(out.tellp());
        for(const auto& entry : entries)
        {
            put32(0x02014b50);
            put16(0x0314);  // made by Unix, ZIP 2.0
            put16(20);
            put16(0x0800);
            put16(0);
            put16(dosTime);
            put16(dosDate);
            put32(entry.crc);
            put32(entry.size);
            put32(entry.size);
            put16(static_cast<uint16_t>(entry.name.size()));
            put16(0);
            put16(0);
            put16(0);
            put16(0);
            put32(0100644u << 16);  // regular file, rw-r--r--
            put32(entry.offset);
            out.write(entry.name.data(), static_cast<std::streamsize>(entry.name.size()));
        }
        auto size = static_cast<uint32_t>(out.tellp()) - start;
        put32(0x06054b50);
        put16(0);
        put16(0);
        put16(static_cast<uint16_t>(entries.size()));
        put16(static_cast<uint16_t>(entries.size()));
        put32(size);
        put32(start);
        put16(0);
        out.close();
        return !out.fail();
    }
};
