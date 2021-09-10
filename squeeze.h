#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace squeeze {

class LzDecompressor
{
public:
    void reset(const uint8_t* data, const size_t size)
    {
        m_compressed = data;
        m_size = size;
        m_position = 0;
        m_decompressed.clear();
    }

    void emitMatch(const size_t offset, const size_t length)
    {
        auto const oldSize = m_decompressed.size();
        m_decompressed.resize(m_decompressed.size() + length);
        std::memcpy(m_decompressed.data() + oldSize, m_decompressed.data() + oldSize - offset,
                    length);
    }

    void emitLiterals(const size_t length)
    {
        auto const oldSize = m_decompressed.size();
        m_decompressed.resize(m_decompressed.size() + length);
        std::memcpy(m_decompressed.data() + oldSize, m_compressed + m_position, length);
        m_position += length;
    }

    auto fetch() -> uint8_t
    {
        return m_compressed[m_position++];
    }

    bool isAtEnd() const
    {
        return m_position >= m_size;
    }

    auto finish() -> std::vector<uint8_t>
    {
        return std::move(m_decompressed);
    }

    auto position() const -> size_t
    {
        return m_position;
    }

    auto decompressedPosition() const -> size_t
    {
        return m_decompressed.size();
    }

private:
    const uint8_t* m_compressed{nullptr};
    size_t m_size{0};
    size_t m_position{0};
    std::vector<uint8_t> m_decompressed;
};

struct Match
{
    size_t offset;
    size_t length;

    bool isValid() const
    {
        return length > 0;
    }
};

struct Range
{
    size_t min{0};
    size_t max{0};

    bool contains(const size_t value) const
    {
        return value >= min && value <= max;
    }
};

struct MatchClass
{
    size_t overhead{0};
    Range length;
    Range offset;

    auto quality(const Match& match) const -> int
    {
        return match.length - overhead;
    }
};

template <unsigned int MatchClasses> class StringMatcher
{
public:
    constexpr auto matchClassCount() const -> unsigned int
    {
        return MatchClasses;
    }

    void configureMatchClass(const unsigned int index, const MatchClass& matchClass)
    {
        m_matchClasses[index] = matchClass;

        m_maxMatchLength = 0;
        for (auto const& cls : m_matchClasses)
        {
            m_maxMatchLength = std::max(m_maxMatchLength, cls.length.max);
        }
    }

    auto matchClass(const unsigned int index) const -> const MatchClass&
    {
        return m_matchClasses[index];
    }

    auto match(const unsigned int index) const -> const Match&
    {
        return m_matches[index];
    }

    auto bestMatch() const -> unsigned int
    {
        unsigned int best_i{0};
        int quality{0};
        for (unsigned int i = 0; i < matchClassCount(); ++i)
        {
            if (match(i).isValid())
            {
                auto const thisQuality = matchClass(i).quality(match(i));
                if (thisQuality > quality)
                {
                    best_i = i;
                    quality = thisQuality;
                }
            }
        }
        return best_i;
    }

    auto maxMatchLength() const -> size_t
    {
        return m_maxMatchLength;
    }

protected:
    void resetMatches()
    {
        for (auto& match : m_matches)
        {
            match.length = 0;
            match.offset = 0;
        }
    }

    std::array<Match, MatchClasses> m_matches;

private:
    std::array<MatchClass, MatchClasses> m_matchClasses;
    size_t m_maxMatchLength{0};
};

template <unsigned int MatchClasses> class BruteForceMatcher : public StringMatcher<MatchClasses>
{
public:
    template <class Iterator> bool findMatches(Iterator begin, Iterator end, Iterator pos)
    {
        this->resetMatches();

        bool matchFound{false};
        auto const searchLength = pos - begin;
        auto const lookAheadLength = end - pos;
        for (size_t offset = 1; offset < searchLength; ++offset)
        {
            size_t length;
            for (length = 0; length < lookAheadLength && length < this->maxMatchLength(); ++length)
            {
                if (pos[-offset + length] != pos[length])
                {
                    break;
                }
            }
            if (length > 0)
            {
                for (unsigned int cls = 0; cls < this->matchClassCount(); ++cls)
                {
                    auto const& matchCls = this->matchClass(cls);
                    if (matchCls.offset.contains(offset) && matchCls.length.contains(length) &&
                        length > this->m_matches[cls].length)
                    {
                        this->m_matches[cls].length = length;
                        this->m_matches[cls].offset = offset;
                        matchFound = true;
                    }
                }
            }
        }

        return matchFound;
    }

    template <class Iterator> void advance(Iterator, Iterator)
    {
    }
};

template <class Matcher> class LzCompressor
{
public:
    LzCompressor() = default;

    explicit LzCompressor(const Matcher& matcher)
        : m_matcher{matcher}
    {
    }

    explicit LzCompressor(Matcher&& matcher)
        : m_matcher{std::move(matcher)}
    {
    }

    auto matcher() const -> const Matcher&
    {
        return m_matcher;
    }

    auto matcher() -> Matcher&
    {
        return m_matcher;
    }

    template <class Processor>
    void compress(const uint8_t* data, const size_t size, Processor& processor)
    {
        auto const* begin = data;
        auto const* pos = data;
        auto const* end = data + size;
        while (pos != end)
        {
            if (m_matcher.findMatches(begin, end, pos))
            {
                auto const bestClass = m_matcher.bestMatch();
                auto const& match = m_matcher.match(bestClass);
                processor.consumeMatch(pos, pos + match.length, bestClass, match);
                m_matcher.advance(pos, pos + match.length);
                pos += match.length;
            }
            else
            {
                processor.consumeLiteral(pos);
                m_matcher.advance(pos, pos + 1);
                pos += 1;
            }
        }
    }

private:
    Matcher m_matcher;
};

} // namespace squeeze