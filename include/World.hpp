//
// Created by lucas on 7/1/25.
//

#ifndef WORLD_HPP
#define WORLD_HPP

#include "Chunk.hpp"
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <memory>

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

	std::vector<std::weak_ptr<Chunk>> getRenderedChunks();

    void updateVisibleChunks(const glm::vec3& cameraPos, const glm::vec3& cameraDir);
    void render(const Shader* shaderProgram) const;

    // Return the number of chunks currently in the rendered list.
    std::size_t getRenderedChunkCount() const;
    // Return the total number of chunks currently loaded in the world.
    std::size_t getTotalChunkCount() const;

    // Get or set the current chunk load radius.  The radius determines how
    // many chunks around the camera are loaded.  Values below 1 are clamped.
    int getLoadRadius() const { return loadRadius; }
    void setLoadRadius(int radius) { loadRadius = std::max(1, radius); }

    // Get or set the maximum number of chunk generation tasks that can run
    // simultaneously.  Lower values reduce CPU spikes at the cost of slower
    // world loading.  Must be at least 1.
    std::size_t getMaxConcurrentGeneration() const { return maxConcurrentGeneration; }
    void setMaxConcurrentGeneration(std::size_t n) { maxConcurrentGeneration = std::max<std::size_t>(1, n); }

	void globalCoordsToLocalCoords(int &x, int &y, int &z, int globalX, int globalY, int globalZ, int &chunkX, int &chunkZ);
    std::shared_ptr<Chunk> getChunk(int chunkX, int chunkZ);
	BlockType getBlockWorld(glm::ivec3 globalCoords); //unused for now
	void setBlockWorld(glm::ivec3 globalCoords, BlockType type);
	bool isBlockVisibleWorld(glm::ivec3 globalCoords);

private:
    using ChunkKey = std::pair<int, int>; // (chunkX, chunkZ)
    std::unordered_map<ChunkPos, std::shared_ptr<Chunk>> chunks;
    std::vector<std::weak_ptr<Chunk>> renderedChunks;
    std::vector<std::pair<int, int>> chunksToGenerate;

    // Set of chunks currently being generated asynchronously.  We use
    // ChunkKey pairs to avoid scheduling the same chunk multiple times.
    std::unordered_set<ChunkKey> generatingChunks;

    // Pending futures representing asynchronous chunk generation tasks.
    std::vector<std::future<std::pair<ChunkKey, Chunk*>>> generationFutures;

    // Maximum number of chunk futures to process (integrate into the world)
    // per updateVisibleChunks() call.  Limiting this reduces frame spikes.
    std::size_t maxChunkProcessPerFrame = 10;

    // The radius (in chunks) around the camera in which to load chunks.  This
    // value can be changed at runtime via the UI.
    int loadRadius = 16;

    // Maximum number of chunk generation tasks that can be running at the
    // same time.  Limiting concurrency prevents CPU oversubscription and
    // reduces frame drops when many chunks need to be generated.  This
    // value can be tuned based on the number of available CPU cores.
    std::size_t maxConcurrentGeneration = 1;

    void linkNeighbors(int chunkX, int chunkZ, std::shared_ptr<Chunk> chunk);
    static ChunkKey toKey(int chunkX, int chunkZ);
};

#endif //WORLD_HPP
