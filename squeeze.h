#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace squeeze {

template <bool AllowOverlapping = false> class LzDecompressor
{
public:
    void reset(const uint8_t* data, const size_t size, const uint8_t* preData = nullptr,
               const size_t preSize = 0)
    {
        m_compressed = data;
        m_size = size;
        m_position = 0;
        m_decompressed.clear();
        if (preData && preSize > 0)
        {
            m_decompressed.insert(m_decompressed.begin(), preData, preData + preSize);
        }
    }

    void emitMatch(const size_t offset, const size_t length)
    {
        auto const oldSize = m_decompressed.size();
        m_decompressed.resize(m_decompressed.size() + length);
        if constexpr (AllowOverlapping)
        {
            for (size_t i = 0; i < length; ++i)
            {
                m_decompressed[oldSize + i] = m_decompressed[oldSize + i - offset];
            }
        }
        else
        {
            std::memcpy(m_decompressed.data() + oldSize, m_decompressed.data() + oldSize - offset,
                        length);
        }
    }

    void emitLiterals(const size_t length)
    {
        auto const oldSize = m_decompressed.size();
        m_decompressed.resize(m_decompressed.size() + length);
        std::memcpy(m_decompressed.data() + oldSize, m_compressed + m_position, length);
        m_position += length;
    }

    void emitLiterals(size_t count, uint8_t value)
    {
        auto const oldSize = m_decompressed.size();
        m_decompressed.resize(m_decompressed.size() + count);
        std::memset(m_decompressed.data() + oldSize, static_cast<int>(value), count);
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
    explicit BruteForceMatcher(const size_t windowLength)
        : m_windowLength{windowLength}
    {
    }

    template <class Iterator> bool findMatches(Iterator begin, Iterator end, Iterator pos)
    {
        this->resetMatches();

        bool matchFound{false};
        auto const searchLength = std::min(static_cast<size_t>(pos - begin), m_windowLength);
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
            if (length > 1)
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

    template <class Iterator> void advance(Iterator, Iterator, Iterator, const size_t)
    {
    }

private:
    size_t m_windowLength;
};

template <unsigned int MatchClasses> class BinaryTreeMatcher : public StringMatcher<MatchClasses>
{
public:
    using StringMatcher<MatchClasses>::maxMatchLength;
    using StringMatcher<MatchClasses>::matchClassCount;
    using StringMatcher<MatchClasses>::resetMatches;

    explicit BinaryTreeMatcher(const size_t windowLength)
        : m_nodes(windowLength, Node{EmptyNode, EmptyNode, EmptyNode})
    {
    }

    template <class Iterator> bool findMatches(Iterator begin, Iterator end, Iterator pos)
    {
        resetMatches();

        bool matchFound{false};
        auto i = m_root;
        unsigned int maxedClasses{0};
        unsigned int tries{0};

        while (i != EmptyNode)
        {
            auto const offset = nodeIndexToOffset(i);
            auto const nodePos = pos - offset;
            auto const patternEnd =
                pos + std::min(maxMatchLength(), static_cast<size_t>(end - pos));
            auto const [comparison, length] =
                compare(pos, patternEnd, nodePos, nodePos + (patternEnd - pos));

            if (length > 1)
            {
                for (unsigned int cls = 0; cls < this->matchClassCount(); ++cls)
                {
                    auto const& matchCls = this->matchClass(cls);
                    auto const maxMatch = std::min(length, matchCls.length.max);
                    if (matchCls.offset.contains(offset) && length >= matchCls.length.min &&
                        maxMatch > this->m_matches[cls].length)
                    {
                        this->m_matches[cls].length = maxMatch;
                        this->m_matches[cls].offset = offset;

                        if (matchCls.length.max == maxMatch)
                        {
                            maxedClasses += 1;
                        }
                        matchFound = true;
                    }
                }
                if (maxedClasses == matchClassCount())
                {
                    break;
                }
            }

            if (comparison >= 0)
            {
                i = m_nodes[i].right;
            }
            else if (comparison < 0)
            {
                i = m_nodes[i].left;
            }

            if (tries++ > 4096)
            {
                break;
            }
        }
        return matchFound;
    }

    template <class Iterator>
    void advance(Iterator begin, Iterator end, Iterator pos, const size_t steps)
    {
        for (size_t i = 0; i < steps; ++i)
        {
            if (pos - begin >= windowLength())
            {
                remove(m_positionBase);
            }

            insert(begin, end, pos);

            ++pos;
            m_positionBase = (m_positionBase + 1) % windowLength();
        }
    }

private:
    static constexpr unsigned int EmptyNode = ~static_cast<unsigned int>(0);

    template <class Iterator>
    auto compare(Iterator begin_a, Iterator end_a, Iterator begin_b, Iterator end_b) const
        -> std::pair<int, size_t>
    {
        auto const length = static_cast<size_t>(end_a - begin_a);
        for (size_t i = 0; begin_a != end_a; ++begin_a, ++begin_b, ++i)
        {
            auto const result = *begin_a - *begin_b;
            if (result != 0)
            {
                return std::make_pair(result > 0 ? 1 : -1, i);
            }
        }
        return std::make_pair(0, length);
    }

    template <class Iterator> void insert(Iterator begin, Iterator end, Iterator pos)
    {
        if (m_root == EmptyNode)
        {
            m_root = m_positionBase;
            m_nodes[m_positionBase].parent = EmptyNode;
            return;
        }

        const size_t searchLength = pos - begin;
        const size_t matchLength = std::min(static_cast<size_t>(end - pos), maxMatchLength());

        auto i = m_root;
        while (true)
        {
            auto const offset = nodeIndexToOffset(i);
            auto const nodePos = pos - offset;
            auto const [result, length] =
                compare(pos, pos + matchLength, nodePos, nodePos + matchLength);
            if (result == 0)
            {
                replace(i, m_positionBase);
                setRight(m_positionBase, i);
                setLeft(m_positionBase, m_nodes[i].left);
                setLeft(i, EmptyNode);
                return;
            }
            else if (result > 0)
            {
                if (m_nodes[i].hasRight())
                {
                    i = m_nodes[i].right;
                }
                else
                {
                    setRight(i, m_positionBase);
                    return;
                }
            }
            else
            {
                if (m_nodes[i].hasLeft())
                {
                    i = m_nodes[i].left;
                }
                else
                {
                    setLeft(i, m_positionBase);
                    return;
                }
            }
        }
    }

    auto inorderPredecessor(const unsigned int n) const -> unsigned int
    {
        auto min = n;
        while (true)
        {
            if (m_nodes[min].left == EmptyNode)
            {
                return min;
            }
            else
            {
                min = m_nodes[min].left;
            }
        }
    }

    void remove(const unsigned int n)
    {
        auto const toDelete = n;
        unsigned int replacement;
        if (!m_nodes[toDelete].hasLeft())
        {
            replacement = m_nodes[toDelete].right;
        }
        else if (!m_nodes[toDelete].hasRight())
        {
            replacement = m_nodes[toDelete].left;
        }
        else
        {
            replacement = inorderPredecessor(m_nodes[toDelete].right);
            setLeft(replacement, m_nodes[toDelete].left);
            if (replacement != m_nodes[toDelete].right)
            {
                setLeft(m_nodes[replacement].parent, m_nodes[replacement].right);
                setRight(replacement, m_nodes[toDelete].right);
            }
        }
        replace(toDelete, replacement);
        m_nodes[toDelete].clear();
    }

    void setLeft(const unsigned int n, const unsigned int left)
    {
        m_nodes[n].left = left;
        if (left != EmptyNode)
        {
            m_nodes[left].parent = n;
        }
    }

    void setRight(const unsigned int n, const unsigned int right)
    {
        m_nodes[n].right = right;
        if (right != EmptyNode)
        {
            m_nodes[right].parent = n;
        }
    }

    void replace(const unsigned int n, const unsigned int replacement)
    {
        if (n != m_root)
        {
            if (m_nodes[m_nodes[n].parent].left == n)
            {
                setLeft(m_nodes[n].parent, replacement);
            }
            else
            {
                setRight(m_nodes[n].parent, replacement);
            }
        }
        else
        {
            m_root = replacement;
            m_nodes[replacement].parent = EmptyNode;
        }
    }

    auto windowLength() const -> size_t
    {
        return m_nodes.size();
    }

    auto nodeIndexToOffset(const unsigned int i) const -> size_t
    {
        return ((m_positionBase - i - 1) % windowLength()) + 1;
        // auto const offset =
        // m_positionPos > i ? (m_positionBase - i) : (windowLength() + (m_positionBase - i));
    }

    struct Node
    {
        unsigned int left{0}, right{0};
        unsigned int parent{0};

        void clear()
        {
            left = right = parent = EmptyNode;
        }

        bool hasLeft() const
        {
            return left != EmptyNode;
        }

        bool hasRight() const
        {
            return right != EmptyNode;
        }
    };

    std::vector<Node> m_nodes;
    unsigned int m_root{EmptyNode};
    size_t m_positionBase{0};
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
    void compress(const uint8_t* data, const size_t size, Processor& processor,
                  size_t startOffset = 0)
    {
        auto const* begin = data;
        auto const* pos = data;
        auto const* end = data + size;
        m_matcher.advance(begin, end, pos, startOffset);
        pos += startOffset;
        while (pos < end)
        {
            if (m_matcher.findMatches(begin, end, pos))
            {
                auto const bestClass = m_matcher.bestMatch();
                auto const& match = m_matcher.match(bestClass);
                processor.consumeMatch(pos, pos + match.length, bestClass, match);
                m_matcher.advance(begin, end, pos, match.length);
                pos += match.length;
            }
            else
            {
                processor.consumeLiteral(pos);
                m_matcher.advance(begin, end, pos, 1);
                pos += 1;
            }
        }
    }

private:
    Matcher m_matcher;
};

} // namespace squeeze