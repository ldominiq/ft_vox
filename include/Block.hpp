#ifndef BLOCK_HPP
#define BLOCK_HPP

enum class BlockType {
    AIR,
    GRASS,
    DIRT,
    STONE,
    SAND,
    SNOW

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