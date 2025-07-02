//
// Created by lucas on 7/1/25.
//

#include "World.hpp"

World::World() {
}

World::~World() {
    for (auto& pair : chunks) {
        delete pair.second;
    }
}

World::ChunkKey World::toKey(int x, int z) {
    return std::make_pair(x, z);
}

Chunk* World::getOrCreateChunk(int chunkX, int chunkZ) {
    ChunkKey key = toKey(chunkX, chunkZ);
    if (chunks.count(key) == 0) {
        Chunk* chunk = new Chunk(chunkX, chunkZ);
        chunks[key] = chunk;
    }

    return chunks[key];
}

void World::updateVisibleChunks(const glm::vec3& cameraPos) {
    const int radius = 8; // Load chunks around the player

    int currentChunkX = static_cast<int>(std::floor(cameraPos.x / Chunk::WIDTH));
    int currentChunkZ = static_cast<int>(std::floor(cameraPos.z / Chunk::DEPTH));

    for (int x = -radius; x <= radius; ++x) {
        for (int z = -radius; z <= radius; ++z) {
            getOrCreateChunk(currentChunkX + x, currentChunkZ + z);
        }
    }
}

void World::render(GLuint shaderProgram) {
    int count = 0;
    for (auto& pair : chunks) {
        count++;
        pair.second->draw(shaderProgram);
    }
    std::cout << "Chunks rendered: " << count << std::endl;

}