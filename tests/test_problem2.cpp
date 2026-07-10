#include "problem2.cpp"

#include <catch_amalgamated.hpp>
#include <cstdlib>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace
{

constexpr KeywordScanner kScanner{};
constexpr auto kIgnore = [](KeywordScanner::Match) noexcept {};

static_assert(kScanner.scan(""sv, kIgnore) == 0);
static_assert(kScanner.scan("company"sv, kIgnore) == 1);
static_assert(kScanner.scan("gg"sv, kIgnore) == 2);

template <typename Callback>
constexpr bool scannable =
    requires(const KeywordScanner & s, std::string_view text, Callback cb) { s.scan(text, cb); };
static_assert(scannable<decltype(kIgnore)>);
static_assert(!scannable<decltype([](KeywordScanner::Match) {})>);

using Hit = std::pair<std::size_t, std::size_t>; // (offset, keyword)

std::vector<Hit> scanAll(std::string_view text)
{
    std::vector<Hit> hits;
    kScanner.scan(text, [&hits](KeywordScanner::Match match) noexcept {
        hits.emplace_back(match.offset, match.keyword);
    });
    return hits;
}

} // namespace

TEST_CASE("placement new", "[problem2]")
{
    static_assert(alignof(KeywordScanner) <= alignof(std::max_align_t));
    void * memory = std::malloc(sizeof(KeywordScanner));
    REQUIRE(memory != nullptr);

    using Destroy = decltype([](KeywordScanner * scanner) noexcept {
        scanner->~KeywordScanner();
        std::free(scanner);
    });
    const std::unique_ptr<KeywordScanner, Destroy> scanner{::new (memory) KeywordScanner{}};
    CHECK(scanner->scan("\\creatim "sv, kIgnore) == 1);
}

TEST_CASE("reference buffer", "[problem2]")
{
    const std::string text =
        std::string("{\\rtf1\\generator MS Word}{\\company Dr.Web}") + '\0' +
        "\\gridtbl \\atnid \\atndate \\creatim \\ftnsepc \\datafield \\atnauthor";
    const std::vector<Hit> expected = {
        {7, 7},  // g
        {7, 3},  // generator
        {27, 0}, // company
        {44, 7}, // g
        {44, 5}, // gridtbl
        {53, 6}, // atnid
        {60, 4}, // atndate
        {69, 8}, // creatim
        {78, 9}, // ftnsepc
        {87, 2}, // datafield
        {98, 1}, // atnauthor
    };
    CHECK(scanAll(text) == expected);
}

TEST_CASE("edges", "[problem2]")
{
    SECTION("single byte")
    {
        CHECK(kScanner.scan("g"sv, kIgnore) == 1);
        CHECK(kScanner.scan("x"sv, kIgnore) == 0);
    }
    SECTION("match offsets")
    {
        const std::vector<Hit> expected = {{0, 0}, {8, 0}};
        CHECK(scanAll("company company"sv) == expected);
    }
    SECTION("high-bit bytes")
    {
        CHECK(kScanner.scan("\x80\xC3g"sv, kIgnore) == 1);
    }
    SECTION("overlaps")
    {
        CHECK(kScanner.scan("ggg"sv, kIgnore) == 3);
    }
    SECTION("nested keywords")
    {
        const std::vector<Hit> expected = {{1, 7}, {1, 3}}; // g, generator
        CHECK(scanAll("\\generator"sv) == expected);
    }
    SECTION("shared prefixes")
    {
        const std::vector<Hit> expected = {{0, 6}, {5, 4}, {12, 1}}; // atnid, atndate, atnauthor
        CHECK(scanAll("atnidatndateatnauthor"sv) == expected);
    }
    SECTION("edges")
    {
        const std::vector<Hit> atStart = {{0, 0}};
        const std::vector<Hit> atEnd = {{4, 0}};
        CHECK(scanAll("companyXXXX"sv) == atStart);
        CHECK(scanAll("XXXXcompany"sv) == atEnd);
    }
    SECTION("NULL")
    {
        CHECK(kScanner.scan(std::string(1024, '\0'), kIgnore) == 0);
        const std::string text = std::string("\0\0", 2) + "atnid" + '\0';
        const std::vector<Hit> expected = {{2, 6}};
        CHECK(scanAll(text) == expected);
    }
}
