#include "Block.hpp"

Block::Block(const BlockType type) : type(type) {}

BlockType Block::getType() const {
    return type;
}

bool Block::isVisible() const {
    return type != BlockType::AIR;
}