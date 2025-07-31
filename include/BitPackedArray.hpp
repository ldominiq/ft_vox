
#ifndef BIT_PACKED_ARRAY_H
#define BIT_PACKED_ARRAY_H

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <cmath>
#include <limits>

//TODO ADD RLE COMPRESSION
class BitPackedArray {
public:
	std::vector<uint32_t> getData(); // USED FOR TESTING RAM USAGE.

    BitPackedArray(size_t size, uint8_t bitsPerEntry);

    void set(size_t index, uint32_t value);
    uint32_t get(size_t index) const;
	void decodeAll(std::vector<uint32_t>& out) const;
	void encodeAll(const std::vector<uint32_t>& in);

    size_t size() const { return m_size; }
    uint8_t bitsPerEntry() const { return m_bitsPerEntry; }

private:
    std::vector<uint32_t> m_data;
    size_t m_size;
    uint8_t m_bitsPerEntry;
};

#endif