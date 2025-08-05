#include "BitPackedArray.hpp"

BitPackedArray::BitPackedArray(size_t size, uint8_t bitsPerEntry)
    : m_size(size), m_bitsPerEntry(bitsPerEntry) {

    if (bitsPerEntry == 0 || bitsPerEntry > 32) {
        throw std::invalid_argument("bitsPerEntry must be between 1 and 32");
    }

    size_t totalBits = m_size * m_bitsPerEntry;
    size_t numWords = (totalBits + 31) / 32;
    m_data.resize(numWords, 0);
}

void BitPackedArray::set(size_t index, uint32_t value) {
    if (index >= m_size)
        throw std::out_of_range("BitPackedArray::set - index out of range");

    if (value >= (1u << m_bitsPerEntry))
        throw std::invalid_argument("Value exceeds bit capacity");

    size_t bitIndex = index * m_bitsPerEntry;
    size_t wordIndex = bitIndex / 32;
    size_t bitOffset = bitIndex % 32;

    m_data[wordIndex] &= ~(((1u << m_bitsPerEntry) - 1) << bitOffset);
    m_data[wordIndex] |= (value & ((1u << m_bitsPerEntry) - 1)) << bitOffset;

    // If the value crosses into the next word
    if (bitOffset + m_bitsPerEntry > 32) {
        size_t bitsInNextWord = (bitOffset + m_bitsPerEntry) - 32;
        m_data[wordIndex + 1] &= ~((1u << bitsInNextWord) - 1);
        m_data[wordIndex + 1] |= (value >> (m_bitsPerEntry - bitsInNextWord));
    }
}

uint32_t BitPackedArray::get(size_t index) const {
    if (index >= m_size)
        throw std::out_of_range("BitPackedArray::get - index out of range");

    size_t bitIndex = index * m_bitsPerEntry;
    size_t wordIndex = bitIndex / 32;
    size_t bitOffset = bitIndex % 32;

    uint32_t value = (m_data[wordIndex] >> bitOffset) & ((1u << m_bitsPerEntry) - 1);

    if (bitOffset + m_bitsPerEntry > 32) {
        size_t bitsInNextWord = (bitOffset + m_bitsPerEntry) - 32;
        uint32_t nextBits = m_data[wordIndex + 1] & ((1u << bitsInNextWord) - 1);
        value |= nextBits << (m_bitsPerEntry - bitsInNextWord);
    }

    return value;
}

void BitPackedArray::decodeAll(std::vector<uint32_t>& out) const {
    out.resize(m_size);

    size_t bits = m_bitsPerEntry;
    uint32_t mask = (1u << bits) - 1;
    size_t bitPos = 0;

    const size_t n = m_size;
    for (size_t i = 0; i < n; ++i) {
        size_t wordIndex = bitPos >> 5;        // bitPos / 32
        size_t bitOffset = bitPos & 31;        // bitPos % 32

        uint64_t val = m_data[wordIndex] >> bitOffset;
        if (bitOffset + bits > 32) {
            val |= static_cast<uint64_t>(m_data[wordIndex + 1]) << (32 - bitOffset);
        }

        out[i] = static_cast<uint32_t>(val) & mask;
        bitPos += bits;
    }
}

void BitPackedArray::encodeAll(
    const std::vector<BlockType>& blocks,
    std::vector<BlockType>& palette,
    std::unordered_map<BlockType, uint32_t>& paletteMap)
{
    if (blocks.size() != m_size) {
        throw std::invalid_argument("encodeAll: input vector size does not match array size");
    }

    palette.clear();
    paletteMap.clear();

    // Build palette
    for (const auto& block : blocks) {
        if (paletteMap.find(block) == paletteMap.end()) {
            uint32_t index = static_cast<uint32_t>(palette.size());
            palette.push_back(block);
            paletteMap[block] = index;
        }
    }

    // Compute required bits
    uint32_t neededBits = 1;
    while ((1u << neededBits) < palette.size()) {
        ++neededBits;
    }
    if (neededBits > m_bitsPerEntry) {
        throw std::runtime_error("encodeAll: palette requires more bits than this BitPackedArray supports");
    }

    std::fill(m_data.begin(), m_data.end(), 0);

    // Pack values
    size_t bitPos = 0;
    uint32_t mask = (1u << m_bitsPerEntry) - 1;

    for (size_t i = 0; i < blocks.size(); ++i) {
        uint32_t value = paletteMap[blocks[i]] & mask;

        size_t wordIndex = bitPos >> 5;
        size_t bitOffset = bitPos & 31;

        m_data[wordIndex] |= value << bitOffset;

        if (bitOffset + m_bitsPerEntry > 32) {
            size_t bitsInNextWord = (bitOffset + m_bitsPerEntry) - 32;
            m_data[wordIndex + 1] |= value >> (m_bitsPerEntry - bitsInNextWord);
        }

        bitPos += m_bitsPerEntry;
    }
}


void BitPackedArray::saveToStream(std::ostream& out) const {
    // Write header
    out.write(reinterpret_cast<const char*>(&m_size), sizeof(m_size));
    out.write(reinterpret_cast<const char*>(&m_bitsPerEntry), sizeof(m_bitsPerEntry));

    // Write packed data
    size_t dataSize = m_data.size();
    out.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));
    out.write(reinterpret_cast<const char*>(m_data.data()), dataSize * sizeof(uint32_t));
}

void BitPackedArray::loadFromStream(std::istream& in) {
    // Read header
    size_t size;
    uint8_t bitsPerEntry;
    size_t dataSize;

    in.read(reinterpret_cast<char*>(&size), sizeof(size));
    in.read(reinterpret_cast<char*>(&bitsPerEntry), sizeof(bitsPerEntry));
    in.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));

    m_size = size;
    m_bitsPerEntry = bitsPerEntry;
    m_data.resize(dataSize);

    in.read(reinterpret_cast<char*>(m_data.data()), dataSize * sizeof(uint32_t));
}
