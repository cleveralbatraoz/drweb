#include "problem1.cpp"

#include <catch_amalgamated.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace
{

struct Builder
{
    std::vector<uint8_t> bytes;

    Builder & u8(uint8_t value)
    {
        bytes.push_back(value);
        return *this;
    }

    Builder & u16(uint16_t value)
    {
        return u8((uint8_t)value).u8((uint8_t)(value >> 8));
    }

    Builder & u32(uint32_t value)
    {
        return u16((uint16_t)value).u16((uint16_t)(value >> 16));
    }

    Builder & magic()
    {
        return u8('H').u8('A').u16(1);
    }

    Builder & header(uint8_t method, uint32_t compressedSize, uint32_t originalSize)
    {
        return u8(method).u32(compressedSize).u32(originalSize).u32(0).u32(0);
    }

    Builder & asciiz(std::string_view text)
    {
        for (const char ch : text)
        {
            u8((uint8_t)ch);
        }
        return u8(0);
    }

    Builder & fill(size_t count, uint8_t value)
    {
        bytes.insert(bytes.end(), count, value);
        return *this;
    }

    Builder & uniform(size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            u8((uint8_t)i);
        }
        return *this;
    }
};

ha::ScanResult parse(const std::vector<uint8_t> & archive)
{
    ha::ScanContext ctx(archive.data(), archive.size());
    return ha::ParseHeader(ctx);
}

} // namespace

TEST_CASE("recognized", "[problem1]")
{
    SECTION("CPY")
    {
        Builder b;
        b.magic().header(0, 64, 128).asciiz("docs").asciiz("readme.txt").u8(0).fill(64, 'x');
        ha::ScanContext ctx(b.bytes.data(), b.bytes.size());
        REQUIRE(ha::ParseHeader(ctx) == ha::kRecognized);
        CHECK(ctx.method == ha::kMethodCpy);
        CHECK(ctx.compressedSize == 64);
        CHECK(ctx.originalSize == 128);
        CHECK(std::string_view{ctx.name} == "readme.txt");
        CHECK(ctx.hostInfo.slots[ha::kInfoSlotFormat] == ha::kInfoFormatHa);
        CHECK(ctx.hostInfo.slots[ha::kInfoSlotArchiveSize] == b.bytes.size());
    }
    SECTION("ASC")
    {
        Builder b;
        b.magic().header(1, 2048, 4096).asciiz("src").asciiz("main.cpp").u8(0).uniform(2048);
        CHECK(parse(b.bytes) == ha::kRecognized);
    }
    SECTION("high method nibble is ignored")
    {
        Builder b;
        b.magic().header(0xF2, 2048, 4096).asciiz("p").asciiz("n").u8(0).uniform(2048);
        CHECK(parse(b.bytes) == ha::kRecognized);
    }
    SECTION("small sample skips the entropy check")
    {
        Builder b;
        b.magic().header(1, 100, 200).asciiz("p").asciiz("n").u8(0).fill(100, 0);
        CHECK(parse(b.bytes) == ha::kRecognized);
    }
    SECTION("empty names")
    {
        Builder b;
        b.magic().header(0, 8, 16).asciiz("").asciiz("").u8(0).fill(8, 'x');
        CHECK(parse(b.bytes) == ha::kRecognized);
    }
    SECTION("path of 255 chars")
    {
        Builder b;
        b.magic().header(0, 8, 16).asciiz(std::string(255, 'p')).asciiz("n").u8(0).fill(8, 'x');
        CHECK(parse(b.bytes) == ha::kRecognized);
    }
}

TEST_CASE("rejected", "[problem1]")
{
    SECTION("unknown method")
    {
        Builder b;
        b.magic().header(3, 64, 128).asciiz("p").asciiz("n").u8(0).fill(64, 'x');
        CHECK(parse(b.bytes) == ha::kNotRecognized);
    }
    SECTION("uSize < cSize")
    {
        Builder b;
        b.magic().header(0, 64, 63).asciiz("p").asciiz("n").u8(0).fill(64, 'x');
        CHECK(parse(b.bytes) == ha::kNotRecognized);
    }
    SECTION("low entropy")
    {
        Builder b;
        b.magic().header(1, 2048, 4096).asciiz("p").asciiz("n").u8(0).fill(2048, 0);
        CHECK(parse(b.bytes) == ha::kNotRecognized);
    }
    SECTION("data does not fit")
    {
        Builder b;
        b.magic().header(0, 65, 130).asciiz("p").asciiz("n").u8(0).fill(64, 'x');
        CHECK(parse(b.bytes) == ha::kNotRecognized);
    }
    SECTION("machine-info block does not fit")
    {
        Builder b;
        b.magic().header(0, 8, 16).asciiz("p").asciiz("n").u8(200).fill(8, 'x');
        CHECK(parse(b.bytes) == ha::kNotRecognized);
    }
    SECTION("path of 256 chars")
    {
        Builder b;
        b.magic().header(0, 8, 16).asciiz(std::string(256, 'p')).asciiz("n").u8(0).fill(8, 'x');
        CHECK(parse(b.bytes) == ha::kNotRecognized);
    }
    SECTION("cut at the path")
    {
        Builder b;
        b.magic().header(0, 8, 16).u8('p').u8('q');
        CHECK(parse(b.bytes) == ha::kNotRecognized);
    }
    SECTION("tiny files")
    {
        CHECK(parse({'H', 'A', 1}) == ha::kNotRecognized);
        CHECK(parse({}) == ha::kNotRecognized);
    }
    SECTION("huge compressed size")
    {
        Builder b;
        b.magic().header(0, 0xFFFFFFF0u, 0xFFFFFFFFu).asciiz("a").asciiz("b").u8(0);
        CHECK(parse(b.bytes) == ha::kNotRecognized);
    }
}

TEST_CASE("corrupt code", "[problem1]")
{
    SECTION("cut at the file name")
    {
        Builder b;
        b.magic().header(0, 8, 16).asciiz("p").u8('n').u8('m');
        CHECK(parse(b.bytes) == ha::kCorruptHeader);
    }
    SECTION("file name of 256 chars")
    {
        Builder b;
        b.magic().header(0, 8, 16).asciiz("p").asciiz(std::string(256, 'n')).u8(0).fill(8, 'x');
        CHECK(parse(b.bytes) == ha::kCorruptHeader);
    }
}
