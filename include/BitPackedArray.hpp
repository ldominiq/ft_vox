
#ifndef BIT_PACKED_ARRAY_H
#define BIT_PACKED_ARRAY_H

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <cmath>
#include <limits>
#include <fstream>
#include <unordered_map>
#include "Block.hpp"

//TODO ADD RLE COMPRESSION
class BitPackedArray {
public:

    BitPackedArray(size_t size, uint8_t bitsPerEntry);

    void set(size_t index, uint32_t value);
    uint32_t get(size_t index) const;

	//unused FOR NOW. Should be for performance reasons instead of single gets/sets
	void decodeAll(std::vector<uint32_t>& out) const;
	void encodeAll(
		const std::vector<BlockType>& blocks,
		std::vector<BlockType>& palette,
		std::unordered_map<BlockType, uint32_t>& paletteMap);

    size_t size() const { return m_size; }
    uint8_t bitsPerEntry() const { return m_bitsPerEntry; }

	void saveToStream(std::ostream& out) const;
	void loadFromStream(std::istream& in);

private:
    std::vector<uint32_t> m_data;
    size_t m_size;
    uint8_t m_bitsPerEntry;
};

#endif