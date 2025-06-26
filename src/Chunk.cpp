#ifndef CHUNCK_HPP
#define CHUNCK_HPP

#include "Chunk.hpp"
#include <cstdlib>
#include <cmath>

Chunk::Chunk(int chunkX, int chunkZ) : originX(chunkX * WIDTH), originZ(chunkZ * DEPTH){
    generate();
}

void Chunk::generate() {
    for (int x = 0; x < WIDTH; ++x) {
        for (int z = 0; z < DEPTH; ++z) {
            int height = 1 + (rand() % 4); // Random height between 1 and 4
            for (int y = 0; y < HEIGHT; ++y) {
                if (y < height)
                    blocks[x][y][z] = BlockType::GRASS; // Fill with dirt
                else
                    blocks[x][y][z] = BlockType::AIR; // Fill with air above the height
            }
        }
    }

    updateVisibleBlocks();
}

BlockType Chunk::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH) {
        return BlockType::AIR; // Out of bounds returns air
    }
    return blocks[x][y][z];
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH) {
        return; // Out of bounds, do nothing
    }
    blocks[x][y][z] = type;
}


void Chunk::updateVisibleBlocks() {
    visibleBlocks.clear();

    for (int x = 0; x < WIDTH; ++x) {
        for (int y = 0; y < HEIGHT; ++y) {
            for (int z = 0; z < DEPTH; ++z) {
                if (blocks[x][y][z] == BlockType::AIR) continue;

                // naive face exposure test
                bool exposed = false;
                if (x == 0 || blocks[x - 1][y][z] == BlockType::AIR) exposed = true;
                if (x == WIDTH - 1 || blocks[x + 1][y][z] == BlockType::AIR) exposed = true;
                if (y == 0 || blocks[x][y - 1][z] == BlockType::AIR) exposed = true;
                if (y == HEIGHT - 1 || blocks[x][y + 1][z] == BlockType::AIR) exposed = true;
                if (z == 0 || blocks[x][y][z - 1] == BlockType::AIR) exposed = true;
                if (z == DEPTH - 1 || blocks[x][y][z + 1] == BlockType::AIR) exposed = true;

                if (exposed) {
                    visibleBlocks.emplace_back(originX + x, y, originZ + z);
                }
            }
        }
    }
}

const std::vector<glm::vec3>& Chunk::getVisibleBlocks() const {
    return visibleBlocks;
}

#endif