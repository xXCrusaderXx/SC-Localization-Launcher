#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// Client side of zsync (http://zsync.moria.org.uk/) for AppImage updates: finds the blocks of a release that the
// installed AppImage already contains, so only the changed blocks are downloaded. Reads the .zsync files that
// appimagetool writes (format 0.6.2, uncompressed targets only). The result is not trusted by itself; the caller
// checks the assembled file against the signed checksums.
namespace Zsync
{

struct ControlFile
{
    size_t blockSize = 0;
    uint64_t length = 0;
    int seqMatches = 1;
    int rsumBytes = 4;
    int checksumBytes = 16;
    // Per block: the stored part of the rolling checksum and the first checksumBytes of its MD4
    std::vector<uint32_t> rsums;
    std::vector<std::array<unsigned char, 16>> checksums;
};

// Half-open byte range [begin, end) of the target file
struct Range
{
    uint64_t begin = 0;
    uint64_t end = 0;
};

namespace detail
{

// MD4 (RFC 1320), which zsync uses for its strong block checksums
class Md4
{
    std::array<uint32_t, 4> state{0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};

    static uint32_t rotl (uint32_t x, int s) { return (x << s) | (x >> (32 - s)); }

    void block (const unsigned char* data)
    {
        std::array<uint32_t, 16> x;
        for(int i = 0; i < 16; ++i)
            x[i] = uint32_t(data[i * 4]) | uint32_t(data[i * 4 + 1]) << 8 | uint32_t(data[i * 4 + 2]) << 16 | uint32_t(data[i * 4 + 3]) << 24;
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        auto f = [] (uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); };
        auto g = [] (uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (x & z) | (y & z); };
        auto h = [] (uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; };
        for(int i : {0, 4, 8, 12})
        {
            a = rotl(a + f(b, c, d) + x[i], 3);
            d = rotl(d + f(a, b, c) + x[i + 1], 7);
            c = rotl(c + f(d, a, b) + x[i + 2], 11);
            b = rotl(b + f(c, d, a) + x[i + 3], 19);
        }
        for(int i : {0, 1, 2, 3})
        {
            a = rotl(a + g(b, c, d) + x[i] + 0x5a827999, 3);
            d = rotl(d + g(a, b, c) + x[i + 4] + 0x5a827999, 5);
            c = rotl(c + g(d, a, b) + x[i + 8] + 0x5a827999, 9);
            b = rotl(b + g(c, d, a) + x[i + 12] + 0x5a827999, 13);
        }
        for(int i : {0, 2, 1, 3})
        {
            a = rotl(a + h(b, c, d) + x[i] + 0x6ed9eba1, 3);
            d = rotl(d + h(a, b, c) + x[i + 8] + 0x6ed9eba1, 9);
            c = rotl(c + h(d, a, b) + x[i + 4] + 0x6ed9eba1, 11);
            b = rotl(b + h(c, d, a) + x[i + 12] + 0x6ed9eba1, 15);
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
    }

   public:
    static std::array<unsigned char, 16> digest (const unsigned char* data, size_t size)
    {
        Md4 md4;
        size_t full = size / 64 * 64;
        for(size_t i = 0; i < full; i += 64) md4.block(data + i);
        std::array<unsigned char, 128> tail{};
        size_t rest = size - full;
        std::memcpy(tail.data(), data + full, rest);
        tail[rest] = 0x80;
        size_t tailSize = rest < 56 ? 64 : 128;
        uint64_t bits = uint64_t(size) * 8;
        for(int i = 0; i < 8; ++i) tail[tailSize - 8 + i] = static_cast<unsigned char>(bits >> (8 * i));
        for(size_t i = 0; i < tailSize; i += 64) md4.block(tail.data() + i);
        std::array<unsigned char, 16> result;
        for(int i = 0; i < 16; ++i) result[i] = static_cast<unsigned char>(md4.state[i / 4] >> (8 * (i % 4)));
        return result;
    }
};

// zsync's rolling checksum: a = sum of the bytes, b = sum of the running values of a (both mod 2^16)
struct Rsum
{
    uint16_t a = 0;
    uint16_t b = 0;

    static Rsum of (const unsigned char* data, size_t size)
    {
        Rsum r;
        for(size_t n = size; n > 0; --n, ++data)
        {
            r.a = static_cast<uint16_t>(r.a + *data);
            r.b = static_cast<uint16_t>(r.b + n * *data);
        }
        return r;
    }

    // Moves the window one byte on; shift = log2(block size)
    void roll (unsigned char out, unsigned char in, int shift)
    {
        a = static_cast<uint16_t>(a + in - out);
        b = static_cast<uint16_t>(b + a - (uint32_t(out) << shift));
    }

    // The control file stores only the last rsumBytes of a (big endian) followed by b
    uint32_t stored (int rsumBytes) const
    {
        uint32_t value = uint32_t(a) << 16 | b;
        return rsumBytes >= 4 ? value : value & ((uint32_t(1) << (8 * rsumBytes)) - 1);
    }
};

inline std::optional<uint64_t> toNumber (const std::string& text)
{
    if(text.empty() || text.size() > 19 || !std::all_of(text.begin(), text.end(), [] (char c) { return c >= '0' && c <= '9'; })) return std::nullopt;
    return std::stoull(text);
}

}  // namespace detail

inline std::optional<ControlFile> parse (const std::string& data)
{
    size_t headerEnd = data.find("\n\n");
    if(headerEnd == std::string::npos) return std::nullopt;

    ControlFile control;
    bool hasBlockSize = false, hasLength = false, hasHashLengths = false;
    std::istringstream header(data.substr(0, headerEnd));
    std::string line;
    while(std::getline(header, line))
    {
        size_t colon = line.find(": ");
        if(colon == std::string::npos) return std::nullopt;
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 2);
        if(key == "Blocksize")
        {
            auto size = detail::toNumber(value);
            // The rolling checksum needs a power of two
            if(!size || *size < 16 || *size > (1u << 20) || (*size & (*size - 1)) != 0) return std::nullopt;
            control.blockSize = *size;
            hasBlockSize = true;
        }
        else if(key == "Length")
        {
            auto length = detail::toNumber(value);
            if(!length || *length == 0) return std::nullopt;
            control.length = *length;
            hasLength = true;
        }
        else if(key == "Hash-Lengths")
        {
            std::array<std::optional<uint64_t>, 3> parts;
            std::istringstream values(value);
            std::string part;
            for(auto& number : parts)
                if(std::getline(values, part, ',')) number = detail::toNumber(part);
            if(!parts[0] || !parts[1] || !parts[2] || *parts[0] < 1 || *parts[0] > 2 || *parts[1] < 1 || *parts[1] > 4 || *parts[2] < 3 ||
               *parts[2] > 16)
                return std::nullopt;
            control.seqMatches = static_cast<int>(*parts[0]);
            control.rsumBytes = static_cast<int>(*parts[1]);
            control.checksumBytes = static_cast<int>(*parts[2]);
            hasHashLengths = true;
        }
        // Compressed targets need zsync's gzip handling, which AppImages never use
        else if(key == "Z-Map2" || key == "Recompress" || key == "Z-URL")
            return std::nullopt;
    }
    if(!hasBlockSize || !hasLength || !hasHashLengths) return std::nullopt;

    uint64_t blocks = (control.length + control.blockSize - 1) / control.blockSize;
    size_t entry = control.rsumBytes + control.checksumBytes;
    if(data.size() - headerEnd - 2 != blocks * entry) return std::nullopt;

    const auto* p = reinterpret_cast<const unsigned char*>(data.data()) + headerEnd + 2;
    control.rsums.resize(blocks);
    control.checksums.resize(blocks);
    for(uint64_t i = 0; i < blocks; ++i)
    {
        uint32_t rsum = 0;
        for(int j = 0; j < control.rsumBytes; ++j) rsum = rsum << 8 | *p++;
        control.rsums[i] = rsum;
        control.checksums[i] = {};
        std::memcpy(control.checksums[i].data(), p, control.checksumBytes);
        p += control.checksumBytes;
    }
    return control;
}

// Writes every block of the target that `seed` contains into `target` (created with the target's length) and
// returns the ranges that still have to be downloaded. nullopt if a file cannot be read or written.
inline std::optional<std::vector<Range>> reuse (const ControlFile& control, const std::filesystem::path& seed, const std::filesystem::path& target)
{
    const size_t bs = control.blockSize;
    const size_t blocks = control.rsums.size();
    int shift = 0;
    while((size_t(1) << shift) < bs) ++shift;

    std::vector<unsigned char> data;
    {
        std::ifstream file(seed, std::ios::binary);
        if(!file.is_open()) return std::nullopt;
        data.assign(std::istreambuf_iterator<char>(file), {});
        if(file.bad()) return std::nullopt;
    }
    const size_t seedSize = data.size();
    // The last block of the target is padded with zeros, so are the windows at the end of the seed
    data.resize(seedSize + 2 * bs, 0);

    std::unordered_map<uint32_t, std::vector<uint32_t>> byRsum;
    for(size_t i = 0; i < blocks; ++i) byRsum[control.rsums[i]].push_back(static_cast<uint32_t>(i));

    auto sameChecksum = [&] (size_t block, const std::array<unsigned char, 16>& digest)
    { return std::memcmp(control.checksums[block].data(), digest.data(), control.checksumBytes) == 0; };

    std::vector<int64_t> source(blocks, -1);
    size_t found = 0;
    size_t x = 0;
    auto r0 = detail::Rsum::of(data.data(), bs);
    auto r1 = detail::Rsum::of(data.data() + bs, bs);
    while(x < seedSize && found < blocks)
    {
        bool matched = false;
        auto candidates = byRsum.find(r0.stored(control.rsumBytes));
        if(candidates != byRsum.end())
        {
            std::optional<std::array<unsigned char, 16>> digest;
            std::optional<std::array<unsigned char, 16>> nextDigest;
            for(uint32_t block : candidates->second)
            {
                // With seqMatches 2 the short checksums only count together with the following block's
                bool needsNext = control.seqMatches > 1 && block + 1 < blocks;
                if(needsNext && r1.stored(control.rsumBytes) != control.rsums[block + 1]) continue;
                if(!digest) digest = detail::Md4::digest(data.data() + x, bs);
                if(!sameChecksum(block, *digest)) continue;
                if(needsNext)
                {
                    if(!nextDigest) nextDigest = detail::Md4::digest(data.data() + x + bs, bs);
                    if(!sameChecksum(block + 1, *nextDigest)) continue;
                }
                matched = true;
                if(source[block] < 0) ++found, source[block] = static_cast<int64_t>(x);
                if(needsNext && source[block + 1] < 0) ++found, source[block + 1] = static_cast<int64_t>(x + bs);
            }
        }
        if(matched)
        {
            x += bs;
            if(x >= seedSize) break;
            r0 = detail::Rsum::of(data.data() + x, bs);
            r1 = detail::Rsum::of(data.data() + x + bs, bs);
        }
        else
        {
            r0.roll(data[x], data[x + bs], shift);
            r1.roll(data[x + bs], data[x + 2 * bs], shift);
            ++x;
        }
    }

    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    if(!out.is_open()) return std::nullopt;
    std::vector<Range> missing;
    for(size_t i = 0; i < blocks; ++i)
    {
        uint64_t begin = uint64_t(i) * bs;
        uint64_t end = std::min<uint64_t>(begin + bs, control.length);
        out.seekp(static_cast<std::streamoff>(begin));
        if(source[i] >= 0)
            out.write(reinterpret_cast<const char*>(data.data() + source[i]), static_cast<std::streamsize>(end - begin));
        else
        {
            static const std::vector<char> zeros(1u << 20, 0);
            out.write(zeros.data(), static_cast<std::streamsize>(end - begin));
            if(!missing.empty() && missing.back().end == begin)
                missing.back().end = end;
            else
                missing.push_back({begin, end});
        }
    }
    out.close();
    if(!out) return std::nullopt;
    return missing;
}

// Joins the ranges with the smallest gaps until at most maxRanges are left: each range is one HTTP request, and
// downloading a few reused bytes again is cheaper than many round trips
inline std::vector<Range> merge (std::vector<Range> ranges, size_t maxRanges)
{
    if(maxRanges == 0) maxRanges = 1;
    if(ranges.size() <= maxRanges) return ranges;
    std::vector<uint64_t> gaps;
    for(size_t i = 1; i < ranges.size(); ++i) gaps.push_back(ranges[i].begin - ranges[i - 1].end);
    std::vector<uint64_t> sorted = gaps;
    std::nth_element(sorted.begin(), sorted.begin() + (ranges.size() - maxRanges - 1), sorted.end());
    // Every gap up to this size is closed; ties may close a few more than needed
    uint64_t limit = sorted[ranges.size() - maxRanges - 1];

    std::vector<Range> merged{ranges.front()};
    for(size_t i = 1; i < ranges.size(); ++i)
    {
        if(gaps[i - 1] <= limit)
            merged.back().end = ranges[i].end;
        else
            merged.push_back(ranges[i]);
    }
    return merged;
}

}  // namespace Zsync
