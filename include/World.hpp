//
// Created by lucas on 7/1/25.
//

#ifndef WORLD_HPP
#define WORLD_HPP

#include "Chunk.hpp"
#include <unordered_map>
#include <cmath>

#include <future>

using ChunkPos = std::pair<int, int>; // (chunkX, chunkZ)

template <>
struct std::hash<ChunkPos> {
    std::size_t operator()(const ChunkPos& p) const noexcept {
        return std::hash<int>()(p.first) ^ std::hash<int>()(p.second) << 1;
    }
};


class World {
public:
    World();
    ~World();

    void updateVisibleChunks(const glm::vec3& cameraPos);
    void render(const Shader* shaderProgram) const;

    Chunk* getChunk(int chunkX, int chunkZ);

private:
    using ChunkKey = std::pair<int, int>; // (chunkX, chunkZ)
    std::unordered_map<ChunkPos, Chunk*> chunks;
    std::vector<Chunk*> renderedChunks;
    std::vector<std::pair<int, int>> chunksToGenerate;

    void linkNeighbors(int chunkX, int chunkZ, Chunk* chunk);
    Chunk* getOrCreateChunk(int chunkX, int chunkZ);
    static ChunkKey toKey(int chunkX, int chunkZ);
};

#endif //WORLD_HPP
