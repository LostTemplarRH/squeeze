#include "Lz80.h"
#include <iostream>
#include <squeeze.h>
#include <stdexcept>

namespace squeeze {

class Lz80Decompressor
{
public:
    auto decompress(const uint8_t* data, const size_t size) -> std::vector<uint8_t>;

    bool copyUncompressed(const uint8_t flags);
    void copyFromRingBuffer1(const uint8_t flags);
    void copyFromRingBuffer2(const uint8_t flags);
    void copyFromRingBuffer3(const uint8_t flags);

    void emitLiterals(const size_t length);
    void emitMatch(const size_t offset, const size_t length);

private:
    squeeze::LzDecompressor m_lzss;
};

auto Lz80Decompressor::decompress(const uint8_t* data, const size_t size) -> std::vector<uint8_t>
{
    m_lzss.reset(data, size);

    while (!m_lzss.isAtEnd())
    {
        auto const flags = m_lzss.fetch();
        switch (flags >> 6)
        {
        case 0:
            if (copyUncompressed(flags))
            {
                return m_lzss.finish();
            }
            break;
        case 1: copyFromRingBuffer1(flags); break;
        case 2: copyFromRingBuffer2(flags); break;
        case 3: copyFromRingBuffer3(flags); break;
        default: throw std::runtime_error{"Lz80Decompressor: encountered invalid flags byte"};
        }
    }
    return m_lzss.finish();
}

bool Lz80Decompressor::copyUncompressed(const uint8_t flags)
{
    size_t length = flags & 0x3f;
    //    0  < length < 0x40   : only flags byte
    // 0x40 <= length < 0xC0   : flags byte + one byte
    // 0xC0 <= length < 0x81C0 : flags byte + two bytes
    if (length == 0)
    {
        auto const firstByte = m_lzss.fetch();
        if (firstByte >> 7 == 0)
        {
            auto const secondByte = m_lzss.fetch();
            if (firstByte == 0 && secondByte == 0)
            {
                // indicator for completion
                return true;
            }
            length = 0xbf + ((firstByte << 8) | secondByte);
        }
        else
        {
            length = 0x40 + (firstByte & 0x7f);
        }
    }

    emitLiterals(length);

    // continue decompressing
    return false;
}

void Lz80Decompressor::copyFromRingBuffer1(const uint8_t flags)
{
    // both only from flags
    // 2 <= length < 6
    const size_t length = 2 + ((flags >> 4) & 0x3);
    // 1 <= offset < 17
    const size_t offset = 1 + (flags & 0xf);
    emitMatch(offset, length);
}

void Lz80Decompressor::copyFromRingBuffer2(const uint8_t flags)
{
    auto const lsb = m_lzss.fetch();
    // length only from flags, offset from combination
    // 3 <= length < 19
    const size_t length = 3 + ((flags >> 2) & 0xf);
    // 1 <= offset < 1025
    const size_t offset = 1 + (((flags & 0x3) << 8) | lsb);
    emitMatch(offset, length);
}

void Lz80Decompressor::copyFromRingBuffer3(const uint8_t flags)
{
    auto const lsb1 = m_lzss.fetch();
    auto const lsb2 = m_lzss.fetch();
    // from flags and next byte; 4 <= length < 132
    const size_t length = 4 + (((flags & 0x3F) << 1) | (lsb1 >> 7));
    // from two next bytes; 1 <= offset < 32769
    const size_t offset = 1 + (((lsb1 & 0x7F) << 8) | lsb2);
    emitMatch(offset, length);
}

void Lz80Decompressor::emitLiterals(const size_t length)
{
    std::cout << m_lzss.position() << " / " << m_lzss.decompressedPosition()
              << " Literals: " << length << "\n";
    m_lzss.emitLiterals(length);
}

void Lz80Decompressor::emitMatch(const size_t offset, const size_t length)
{
    std::cout << m_lzss.position() << " / " << m_lzss.decompressedPosition()
              << " Match   : " << offset << ", " << length << "\n";
    m_lzss.emitMatch(offset, length);
}

auto decompressLz80(const uint8_t* data, const size_t size) -> std::vector<uint8_t>
{
    return Lz80Decompressor{}.decompress(data, size);
}

class Lz80Compressor
{
public:
    explicit Lz80Compressor(const uint8_t* data, const size_t size)
        : m_literalStart{data}
        , m_literalEnd{data}
        , m_end{data + size}
    {
    }

    void consumeMatch(const uint8_t* begin, const uint8_t* end, const unsigned int cls,
                      const Match& match)
    {
        if (m_literalStart != m_literalEnd)
        {
            encodeUncompressed(end);
        }

        switch (cls)
        {
        case 0: encodeMatch0(match); break;
        case 1: encodeMatch1(match); break;
        case 2: encodeMatch2(match); break;
        default: break;
        }

        m_literalStart = m_literalEnd = end;
    }

    void encodeMatch0(const Match& match)
    {
        uint8_t flags = match.offset - 1;
        flags |= (match.length - 2) << 4;
        flags |= 1 << 6;
        m_compressed.push_back(flags);
    }

    void encodeMatch1(const Match& match)
    {
        uint8_t flags = (match.length - 3) << 2;
        flags |= (match.offset >> 8);
        flags |= 2 << 6;
        m_compressed.push_back(flags);
        m_compressed.push_back(static_cast<uint8_t>(match.offset - 1));
    }

    void encodeMatch2(const Match& match)
    {
        auto const adjustedLength = match.length - 4;
        uint8_t flags = static_cast<uint8_t>(adjustedLength >> 1);
        flags |= 3 << 6;
        m_compressed.push_back(flags);
        auto const adjustedOffset = match.offset - 1;
        m_compressed.push_back(((adjustedOffset >> 8) & 0x7f) | ((adjustedLength & 1) << 7));
        m_compressed.push_back(static_cast<uint8_t>(adjustedOffset));
    }

    void consumeLiteral(const uint8_t* pos)
    {
        m_literalEnd += 1;

        if (m_literalEnd - m_literalStart == 0x8000)
        {
            encodeUncompressed(pos + 1);
            m_literalStart = m_literalEnd;
        }
    }

    void encodeUncompressed(const uint8_t* pos)
    {
        auto const length = m_literalEnd - m_literalStart;
        if (length < 0x40)
        {
            m_compressed.push_back(static_cast<uint8_t>(length));
        }
        else if (length < 0xC0)
        {
            auto const adjustedLength = static_cast<uint8_t>(length - 0x40);
            m_compressed.push_back(0x80 | adjustedLength);
        }
        else
        {
            auto const adjustedLength = length - 0xbf;
            m_compressed.push_back(static_cast<uint8_t>(adjustedLength >> 8));
            m_compressed.push_back(static_cast<uint8_t>(adjustedLength));
        }

        auto const oldSize = m_compressed.size();
        m_compressed.resize(oldSize + length);
        std::memcpy(m_compressed.data() + oldSize, m_literalStart, length);
    }

    auto finish() -> std::vector<uint8_t>
    {
        if (m_literalStart != m_literalEnd)
        {
            encodeUncompressed(m_end);
        }

        // end of compressed stream
        m_compressed.push_back(0x00);
        m_compressed.push_back(0x00);
        m_compressed.push_back(0x00);

        return std::move(m_compressed);
    }

private:
    std::vector<uint8_t> m_compressed;
    const uint8_t* m_literalStart{nullptr};
    const uint8_t* m_literalEnd{nullptr};
    const uint8_t* m_end{nullptr};
};

auto compressLz80(const uint8_t* data, const size_t size) -> std::vector<uint8_t>
{
    Lz80Compressor lz80(data, size);
    squeeze::LzCompressor<squeeze::BruteForceMatcher<3>> lz;
    lz.matcher().configureMatchClass(0, MatchClass{0, {2, 6}, {1, 17}});
    lz.matcher().configureMatchClass(1, MatchClass{1, {3, 18}, {1, 1025}});
    lz.matcher().configureMatchClass(2, MatchClass{2, {4, 131}, {1, 32768}});
    lz.compress(data, size, lz80);
    return lz80.finish();
}

} // namespace squeeze