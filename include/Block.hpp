#ifndef BLOCK_HPP
#define BLOCK_HPP
#include <cstdint>

enum class BlockType {
    AIR,
    GRASS,
    DIRT,
    STONE,
    SAND,
    SNOW
};

struct Voxel {
    BlockType type;
    uint8_t skyLight; // 0-15, sunlight propagated from sky
    uint8_t blockLight; // 0-15, emitted from torches, etc.
};

class Block {
public:
    explicit Block(BlockType type = BlockType::AIR);
    BlockType getType() const;
    bool isVisible() const;
private:
    BlockType type;
};

#endif