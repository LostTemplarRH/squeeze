#include "Lz0103.h"
#include "Lz0103Data.h"
#include <squeeze.h>
#include <stdexcept>

namespace squeeze {

class Lz0103Decompressor
{
public:
    explicit Lz0103Decompressor(bool rle);

    [[nodiscard]] auto decompress(const uint8_t* data, const size_t size) -> std::vector<uint8_t>;

    void emitLiterals(size_t length);
    void emitLiterals(size_t length, uint8_t value);
    void emitMatch(size_t offset, size_t length);
    void advance(size_t length);

private:
    bool m_rle{false};
    std::vector<uint8_t> m_preData;
    size_t m_zeroOffset;
    size_t m_ringBufferOffset;
    squeeze::LzDecompressor<true> m_lzss;
};

Lz0103Decompressor::Lz0103Decompressor(bool rle)
    : m_rle{rle}
{
    m_preData.clear();
    m_preData.resize(4096);
    std::memcpy(m_preData.data(), RingbufferPrefill + (m_rle ? 1 : 0), 4096);
}

auto Lz0103Decompressor::decompress(const uint8_t* data, const size_t size) -> std::vector<uint8_t>
{
    m_ringBufferOffset = 0;
    m_zeroOffset = 0x1000 - (m_rle ? 0xfef : 0xfee);
    uint8_t control{0xff};
    unsigned int bitsRemaining{0};
    m_lzss.reset(data, size, m_preData.data(), m_preData.size());

    while (!m_lzss.isAtEnd())
    {
        if (bitsRemaining == 0)
        {
            control = m_lzss.fetch();
            bitsRemaining = 8;
        }
        bitsRemaining -= 1;

        if (control & 1)
        {
            emitLiterals(1);
        }
        else
        {
            auto const nextControl1 = m_lzss.fetch();
            auto const nextControl2 = m_lzss.fetch();
            const uint8_t control1 = nextControl2 & 0x0F;
            const uint8_t control2 = nextControl2 >> 4;
            if (m_rle && control1 == 0x0F)
            {
                uint16_t runLength;
                uint8_t value;
                if (control2 == 0x00)
                {
                    runLength = nextControl1 + 19;
                    value = m_lzss.fetch();
                }
                else
                {
                    runLength = control2 + 3;
                    value = nextControl1;
                }
                emitLiterals(runLength, value);
            }
            else
            {
                const uint16_t referenceLength = 3 + control1;
                const uint16_t referenceOffset = nextControl1 | (control2 << 8);
                auto const absoluteOffset =
                    (m_zeroOffset + referenceOffset - m_ringBufferOffset) % 4096;
                auto const offset = 4096 - absoluteOffset;
                emitMatch(offset, referenceLength);
            }
        }
        control >>= 1;
    }
    auto decompressed = m_lzss.finish();
    // decompressed.erase(decompressed.begin(), decompressed.begin() + m_preData.size());
    return decompressed;
}

void Lz0103Decompressor::emitLiterals(size_t length)
{
    m_lzss.emitLiterals(length);
    advance(length);
}

void Lz0103Decompressor::emitLiterals(size_t length, uint8_t value)
{
    m_lzss.emitLiterals(length, value);
    advance(length);
}

void Lz0103Decompressor::emitMatch(size_t offset, size_t length)
{
    m_lzss.emitMatch(offset, length);
    advance(length);
}

void Lz0103Decompressor::advance(size_t length)
{
    m_ringBufferOffset += length;
    if (m_ringBufferOffset >= m_zeroOffset)
    {
        m_zeroOffset += 4096;
    }
}

auto decompressLz01(const uint8_t* data, const size_t size) -> std::vector<uint8_t>
{
    Lz0103Decompressor lz{false};
    return lz.decompress(data, size);
}

auto decompressLz03(const uint8_t* data, const size_t size) -> std::vector<uint8_t>
{
    Lz0103Decompressor lz{true};
    return lz.decompress(data, size);
}

class Lz0103Compressor
{
public:
    explicit Lz0103Compressor(const uint8_t* data, const size_t size, size_t zeroOffset)
        : m_data{data}
        , m_end{data + size}
        , m_zeroOffset{zeroOffset}
        , m_ringBufferOffset{0}
    {
        m_compressed.push_back(0x00);
        m_lastFlag = 0;
    }

    void consumeMatch(const uint8_t* begin, const uint8_t* end, const unsigned int cls,
                      const Match& match)
    {
        m_compressed[m_lastFlag] >>= 1;

        encodeMatch(match);
        advance(match.length);
    }

    void encodeMatch(const Match& match)
    {
        auto const offset = 4096 - match.offset + m_ringBufferOffset;
        auto const blub = (offset - m_zeroOffset) % 4096;

        const uint8_t b = (match.length - 3) | ((blub >> 8) << 4);
        const uint8_t a = blub & 0xff;
        m_compressed.push_back(a);
        m_compressed.push_back(b);
    }

    void consumeLiteral(const uint8_t* pos)
    {
        m_compressed[m_lastFlag] >>= 1;
        m_compressed[m_lastFlag] |= 0x80;
        m_compressed.push_back(*pos);
        advance(1);
    }

    void consumeRLE(const uint8_t* pos, size_t length)
    {
    }

    void advance(size_t length)
    {
        m_ringBufferOffset += length;
        if (m_ringBufferOffset >= m_zeroOffset)
        {
            m_zeroOffset += 4096;
        }

        if (--m_flagsLeft == 0)
        {
            m_flagsLeft = 8;
            m_lastFlag = m_compressed.size();
            m_compressed.push_back(0x00);
        }
    }

    auto finish() -> std::vector<uint8_t>
    {
        if (m_flagsLeft == 8)
        {
            m_compressed.pop_back();
        }
        return std::move(m_compressed);
    }

private:
    std::vector<uint8_t> m_compressed;
    const uint8_t* m_data{nullptr};
    const uint8_t* m_end{nullptr};
    size_t m_lastFlag{0};
    uint8_t m_flagsLeft{8};
    size_t m_zeroOffset;
    size_t m_ringBufferOffset;
};

class Lz01Compressor : public Lz0103Compressor
{
public:
    explicit Lz01Compressor(uint8_t* data, size_t size)
        : Lz0103Compressor{data, size, 0x12}
    {
    }
};

class Lz03Compressor : public Lz0103Compressor
{
public:
    explicit Lz03Compressor(uint8_t* data, size_t size)
        : Lz0103Compressor{data, size, 0x11}
    {
    }
};

auto compressLz03(const uint8_t* data, const size_t size) -> std::vector<uint8_t>
{
    std::vector<uint8_t> prefixedData(size + 4096);
    std::memcpy(prefixedData.data(), RingbufferPrefill + 1, 4096);
    std::memcpy(prefixedData.data() + 4096, data, size);

    Lz03Compressor lz0103(prefixedData.data(), prefixedData.size());
    squeeze::LzCompressor<squeeze::BinaryTreeMatcher<1>> lz{squeeze::BinaryTreeMatcher<1>{4096}};
    lz.matcher().configureMatchClass(0, MatchClass{0, {3, 17}, {1, 4096}});
    lz.compress(prefixedData.data(), prefixedData.size(), lz0103, 4096);
    return lz0103.finish();
}

auto compressLz01(const uint8_t* data, const size_t size) -> std::vector<uint8_t>
{
    std::vector<uint8_t> prefixedData(size + 4096);
    std::memcpy(prefixedData.data(), RingbufferPrefill, 4096);
    std::memcpy(prefixedData.data() + 4096, data, size);

    Lz01Compressor lz0103(prefixedData.data(), prefixedData.size());
    squeeze::LzCompressor<squeeze::BinaryTreeMatcher<1>> lz{squeeze::BinaryTreeMatcher<1>{4096}};
    lz.matcher().configureMatchClass(0, MatchClass{0, {3, 18}, {1, 4096}});
    lz.compress(prefixedData.data(), prefixedData.size(), lz0103, 4096);
    return lz0103.finish();
}

} // namespace squeeze