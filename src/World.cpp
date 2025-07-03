//
// Created by lucas on 7/1/25.
//

#include "World.hpp"

World::World() {
}

World::~World() {
    for (const auto& pair : chunks) {
        delete pair.second;
    }
}

World::ChunkKey World::toKey(int chunkX, int chunkZ) {
    return std::make_pair(chunkX, chunkZ);
}

Chunk* World::getChunk(const int chunkX, const int chunkZ) {
    const ChunkKey key = toKey(chunkX, chunkZ);
    if (chunks.count(key) == 0) return nullptr;
    return chunks[key];
}

Chunk* World::getOrCreateChunk(int chunkX, int chunkZ) {
    ChunkKey key = toKey(chunkX, chunkZ);
    if (chunks.count(key) > 0)
        return chunks[key];

    Chunk* chunk = new Chunk(chunkX, chunkZ);
    chunks[key] = chunk;

    // Directions:     NORTH      SOUTH      EAST       WEST
    const int dirX[] = { 0,        0,         1,        -1 };
    const int dirZ[] = { 1,       -1,         0,         0 };
    const int opp[]  = { SOUTH,   NORTH,     WEST,      EAST };

    for (int dir = 0; dir < 4; ++dir) {
        int nx = chunkX + dirX[dir];
        int nz = chunkZ + dirZ[dir];

        Chunk* neighbor = getChunk(nx, nz);
        chunk->setAdjacentChunks(dir, neighbor);

        if (neighbor) {
            neighbor->setAdjacentChunks(opp[dir], chunk);

            if (neighbor->hasAllAdjacentChunkLoaded()) {
                neighbor->updateVisibleBlocks();
                neighbor->buildMesh();
            }
        }
    }

    return chunk;
}


void World::updateVisibleChunks(const glm::vec3& cameraPos) {
    renderedChunks.clear();
    constexpr int radius = 8; // Load chunks around the player

    const int currentChunkX = static_cast<int>(std::floor(cameraPos.x / Chunk::WIDTH));
    const int currentChunkZ = static_cast<int>(std::floor(cameraPos.z / Chunk::DEPTH));

    for (int x = -radius; x <= radius; ++x) {
        for (int z = -radius; z <= radius; ++z) {
            Chunk *chunk = getOrCreateChunk(currentChunkX + x, currentChunkZ + z);
            renderedChunks.emplace_back(chunk);
        }
    }
}

void World::render(const Shader* shaderProgram) const {
    int count = 0;
    for (auto& pair : renderedChunks) {
        count++;
        pair->draw(shaderProgram);
    }
    std::cout << "Chunks rendered: " << count << std::endl;
}
