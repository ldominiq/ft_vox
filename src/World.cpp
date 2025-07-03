//
// Created by lucas on 7/1/25.
//

#include "World.hpp"
#include <utility>

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

Chunk* World::getChunk(int chunkX, int chunkZ) {
    ChunkKey key = toKey(chunkX, chunkZ);
    if (chunks.count(key) == 0) return NULL;
    return chunks[key];
}


// std::vector<std::future<std::pair<ChunkKey, Chunk*>>> futures;

// for (auto [chunkX, chunkZ] : chunksToGenerate) {
//     ChunkKey key = toKey(chunkX, chunkZ);
//     futures.push_back(std::async(std::launch::async, [=]() {
//         Chunk* chunk = new Chunk(chunkX, chunkZ);
//         return std::make_pair(key, chunk);
//     }));
// }

// // Wait for all to finish and insert into map
// for (auto& future : futures) {
//     auto [key, chunk] = future.get();
//     chunks[key] = chunk;
// }

void World::linkNeighbors(int chunkX, int chunkZ, Chunk* chunk) {
    
    const int dirX[] = { 0, 0, 1, -1 };
    const int dirZ[] = { 1, -1, 0, 0 };
    const int opp[]  = { SOUTH, NORTH, WEST, EAST };

    for (int dir = 0; dir < 4; ++dir) {
        int nx = chunkX + dirX[dir];
        int nz = chunkZ + dirZ[dir];

        Chunk* neighbor = getChunk(nx, nz);

        chunk->setAdjacentChunks(static_cast<Direction>(dir), neighbor);
        if (neighbor) {
            neighbor->setAdjacentChunks(static_cast<Direction>(opp[dir]), chunk);
            if (neighbor->hasAllAdjacentChunkLoaded()) {
                neighbor->updateVisibleBlocks();
                neighbor->buildMesh();
            }
        }
    }
}

Chunk* World::getOrCreateChunk(int chunkX, int chunkZ) {
    ChunkKey key = toKey(chunkX, chunkZ);
    if (chunks.count(key) > 0)
        return chunks[key];

    return NULL;

    Chunk* chunk = new Chunk(chunkX, chunkZ);
    chunks[key] = chunk;

    linkNeighbors(chunkX, chunkZ, chunk);

    return chunk;
}

void World::updateVisibleChunks(const glm::vec3& cameraPos) {
    chunksToGenerate.clear();
    renderedChunks.clear();
    const int radius = 16; // Load chunks around the player

    int currentChunkX = static_cast<int>(std::floor(cameraPos.x / Chunk::WIDTH));
    int currentChunkZ = static_cast<int>(std::floor(cameraPos.z / Chunk::DEPTH));

    for (int x = -radius; x <= radius; ++x) {
        for (int z = -radius; z <= radius; ++z) {
            Chunk *chunk = getOrCreateChunk(currentChunkX + x, currentChunkZ + z);

            if (!chunk)
                chunksToGenerate.emplace_back(std::make_pair(currentChunkX + x, currentChunkZ + z));
            else
                renderedChunks.emplace_back(chunk);
        }
    }

    std::vector<std::future<std::pair<ChunkKey, Chunk*>>> futures;

    for (auto [chunkX, chunkZ] : chunksToGenerate) {
        ChunkKey key = toKey(chunkX, chunkZ);

        futures.push_back(std::async(std::launch::async, [=]() {
            // Make sure this does not access shared state!
            Chunk* chunk = new Chunk(chunkX, chunkZ);
            return std::make_pair(key, chunk);
        }));
    }

    // Wait for all threads to finish, insert into the map (single-threaded section)
    for (auto& future : futures) {
        auto [key, chunk] = future.get();
        chunks[key] = chunk;  // Safe: chunks modified in main thread only
    }

    // Now safely link neighbors
    for (auto [chunkX, chunkZ] : chunksToGenerate) {
        linkNeighbors(chunkX, chunkZ, getChunk(chunkX, chunkZ));
    }

}

void World::render(Shader* shaderProgram) {
    int count = 0;
    for (auto& pair : renderedChunks) {
        count++;
        pair->draw(shaderProgram);
    }
    // std::cout << "Chunks rendered: " << count << std::endl;
}
