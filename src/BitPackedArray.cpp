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

void BitPackedArray::encodeAll(const std::vector<uint32_t>& in) {
    // if (in.size() != m_size)
    //     throw std::invalid_argument("encodeAll - input size mismatch");

    // size_t bits = m_bitsPerEntry;
    // uint32_t mask = (1u << bits) - 1;
    // size_t bitPos = 0;

    // // Clear current data
    // std::fill(m_data.begin(), m_data.end(), 0);

    // const size_t n = m_size;
    // for (size_t i = 0; i < n; ++i) {
    //     uint32_t value = in[i];
    //     if (value > mask) {
    //         throw std::invalid_argument("encodeAll - value exceeds bit capacity");
    //     }

    //     size_t wordIndex = bitPos >> 5;  // bitPos / 32
    //     size_t bitOffset = bitPos & 31;  // bitPos % 32

    //     // Write into first word
    //     m_data[wordIndex] |= (value & mask) << bitOffset;

    //     // Handle overflow into next word
    //     if (bitOffset + bits > 32) {
    //         size_t bitsInNextWord = (bitOffset + bits) - 32;
    //         m_data[wordIndex + 1] |= value >> (bits - bitsInNextWord);
    //     }

    //     bitPos += bits;
    // }
}

std::vector<uint32_t> BitPackedArray::getData()
{
	return m_data;
}