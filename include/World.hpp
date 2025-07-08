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

	std::vector<Chunk*> getRenderedChunks();

    void updateVisibleChunks(const glm::vec3& cameraPos);
    void render(const Shader* shaderProgram) const;

	void globalCoordsToLocalCoords(int &x, int &y, int &z, int globalX, int globalY, int globalZ, int &chunkX, int &chunkZ);
    Chunk* getChunk(int chunkX, int chunkZ);
	BlockType getBlockWorld(glm::ivec3 globalCoords); //unused for now
	void setBlockWorld(glm::ivec3 globalCoords, BlockType type);
	bool isBlockVisibleWorld(glm::ivec3 globalCoords);

private:
    using ChunkKey = std::pair<int, int>; // (chunkX, chunkZ)
    std::unordered_map<ChunkPos, Chunk*> chunks;
    std::vector<Chunk*> renderedChunks;
    std::vector<std::pair<int, int>> chunksToGenerate;

    void linkNeighbors(int chunkX, int chunkZ, Chunk* chunk);
    static ChunkKey toKey(int chunkX, int chunkZ);
};

#endif //WORLD_HPP
