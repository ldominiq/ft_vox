#ifndef BLOCK_HPP
#define BLOCK_HPP

#include <glm/glm.hpp>

enum class BlockType {
    AIR,
    GRASS,
    DIRT,
    STONE
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