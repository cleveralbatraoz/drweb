// Aho-Corasick scanner over a fixed keyword dictionary
// O(length + matches)

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <ranges>
#include <string_view>
#include <type_traits>

using namespace std::string_view_literals;

inline constexpr std::array kKeywords = {
    "company"sv, "atnauthor"sv, "datafield"sv, "generator"sv, "atndate"sv,
    "gridtbl"sv, "atnid"sv,     "g"sv,         "creatim"sv,   "ftnsepc"sv,
};

// Assumes non-empty, distinct keywords
static_assert(std::ranges::none_of(kKeywords,
                                   [](std::string_view keyword) { return keyword.empty(); }));
static_assert([] {
    auto sorted = kKeywords;
    std::ranges::sort(sorted);
    return std::ranges::adjacent_find(sorted) == sorted.end();
}());

class KeywordScanner
{
  public:
    struct Match
    {
        std::size_t keyword;
        std::size_t offset;
    };

    constexpr KeywordScanner() noexcept
    {
        transitions_.fill(kNoState);
        terminal_.fill(kNoKeyword);
        suffixLink_.fill(kNoState);

        State nextState = kRoot + 1;
        for (const auto [index, keyword] : std::views::enumerate(kKeywords))
        {
            State state = kRoot;
            for (const char ch : keyword)
            {
                State & edge = transition(state, static_cast<unsigned char>(ch));
                if (edge == kNoState)
                {
                    edge = nextState++;
                }
                state = edge;
            }
            terminal_[state] = static_cast<std::size_t>(index);
        }

        std::array<State, kStateCapacity> fail{};
        std::array<State, kStateCapacity> queue{};
        std::size_t head = 0;
        std::size_t tail = 0;
        for (const std::size_t byte : std::views::iota(0uz, kAlphabetSize))
        {
            State & edge = transition(kRoot, byte);
            if (edge == kNoState)
            {
                edge = kRoot;
            }
            else
            {
                fail[edge] = kRoot;
                queue[tail++] = edge;
            }
        }
        while (head < tail)
        {
            const State state = queue[head++];
            const State suffix = fail[state];
            suffixLink_[state] = terminal_[suffix] != kNoKeyword ? suffix : suffixLink_[suffix];
            for (const std::size_t byte : std::views::iota(0uz, kAlphabetSize))
            {
                State & edge = transition(state, byte);
                if (edge == kNoState)
                {
                    edge = transition(suffix, byte);
                }
                else
                {
                    fail[edge] = transition(suffix, byte);
                    queue[tail++] = edge;
                }
            }
        }
    }

    template <typename Callback>
        requires std::is_nothrow_invocable_v<Callback &, Match>
    constexpr std::size_t scan(std::string_view text, Callback && onMatch) const noexcept
    {
        State state = kRoot;
        std::size_t matches = 0;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            state = transition(state, static_cast<unsigned char>(text[i]));

            if (const std::size_t k = terminal_[state]; k != kNoKeyword)
            {
                onMatch(Match{.keyword = k, .offset = i + 1 - kKeywords[k].size()});
                ++matches;
            }

            for (State suffix = suffixLink_[state]; suffix != kNoState;
                 suffix = suffixLink_[suffix])
            {
                const std::size_t k = terminal_[suffix]; // terminal by construction
                onMatch(Match{.keyword = k, .offset = i + 1 - kKeywords[k].size()});
                ++matches;
            }
        }

        return matches;
    }

  private:
    using State = std::uint32_t;

    static constexpr std::size_t kStateCapacity = std::ranges::fold_left(
        kKeywords | std::views::transform([](std::string_view keyword) { return keyword.size(); }),
        1uz, std::plus{});
    static constexpr std::size_t kAlphabetSize = std::numeric_limits<unsigned char>::max() + 1uz;

    static constexpr State kRoot = 0;
    static constexpr State kNoState = kStateCapacity;
    static constexpr std::size_t kNoKeyword = kKeywords.size();

    constexpr State & transition(State state, std::size_t byte) noexcept
    {
        return transitions_[state * kAlphabetSize + byte];
    }

    constexpr State transition(State state, std::size_t byte) const noexcept
    {
        return transitions_[state * kAlphabetSize + byte];
    }

    std::array<State, kStateCapacity * kAlphabetSize> transitions_;
    std::array<std::size_t, kStateCapacity> terminal_;
    std::array<State, kStateCapacity> suffixLink_;
};

static_assert(KeywordScanner{}.scan("\\generator"sv, [](KeywordScanner::Match) noexcept {}) == 2);
