#ifndef CHUNK_HPP
#define CHUNK_HPP

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/noise.hpp>

#include <glad/glad.h>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <ostream>
#include "Shader.hpp"
#include "FastNoiseLite.h"

#include <random>
#include <unordered_set>
#include <queue>
#include <memory>

#include "Block.hpp"
#include "BitPackedArray.hpp"

class World;
class BlockStorage;

struct IVec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        return std::hash<int>()(v.x) ^ std::hash<int>()(v.y << 1) ^ std::hash<int>()(v.z << 2);
    }
};

// TODO : REVAMP CAVES. Idea : map the whole world to some 3D noise map. Maybe possible and efficient?
struct Worm {
    glm::vec3 pos;
	float radius = 2.0f;
	int steps = 120;
	// FastNoiseLite noise;

	Worm(const glm::vec3& p, float r, int s)
		: pos(p), radius(r), steps(s) {}

};

// ==BIOME PARAMETERS==
// scaleX/scaleZ = how stretched the biome is
// amplitude = how high the terrain will be
// offset = how much the terrain is offset from 0
// blendMin/blendMax = how much the biome blends with the next one
// topBlock = the block type at the top of the biome
struct BiomeParams {
    float scaleX, scaleZ;
    float amplitude;
    float offset;
    float blendMin, blendMax;
    BlockType topBlock;
	bool useBaseNoise;
	
};

enum Direction {
	NORTH = 0,
	SOUTH,
	EAST,
	WEST,
	NONE
};

enum class BiomeType {
    PLAINS,
    DESERT,
    FOREST,
    MOUNTAIN,
    SNOW
};

class Chunk {
public:
	static constexpr int WIDTH = 16; // Size of the chunck in blocks
	static constexpr int HEIGHT = 256; // Height of the chunck in blocks
	static constexpr int DEPTH = 16; // Depth of the chunck in blocks
    static constexpr int BLOCK_COUNT = WIDTH * HEIGHT * DEPTH;

    Chunk(const int chunkX, const int chunkZ, int seed, const bool doGenerate = true);
	Chunk() = default;
    
    void carveWorm(Worm& worm, BlockStorage &blocks);
    void generate();

    BlockType getBlock(int x, int y, int z) const;
	void setBlock(int x, int y, int z, BlockType block);

	bool isBlockVisible(glm::ivec3 blockPos);

    void draw(const std::shared_ptr<Shader> &shaderProgram) const; // Draw the chunk using the given shader program

	void setAdjacentChunks(int direction, std::shared_ptr<Chunk> &chunk);
	bool hasAllAdjacentChunkLoaded() const;

	bool needsUpdate() const;
	void updateChunk(); //updateVisibleBlocks + buildMesh. Used when updating blocks in a chunk

	void saveToStream(std::ostream& out) const;
	void loadFromStream(std::istream& in);

	BlockType selectBlockType(int y, int surfaceHeight, float blend, const std::vector<BiomeParams>& biomes, const std::vector<float>& weights, const std::vector<float>& heights);

private:
	int SEED;
	bool m_needsUpdate = true;
	std::weak_ptr<Chunk> adjacentChunks[4] = {};

	std::vector<BlockType> palette; // Index -> BlockType
	std::unordered_map<BlockType, uint32_t> paletteMap; // BlockType -> Index
    BitPackedArray blockIndices;
	
    int originX; // X coordinate of the chunck origin
    int originZ; // Z coordinate of the chunck origin
    GLuint VAO = 0, VBO = 0;
	uint meshVerticesSize;
    std::vector<float> meshVertices; // Vertices for the mesh

    void addFace(int x, int y, int z, int face); // Add a face to the mesh vertices

	void buildMesh(); // Build the mesh for rendering	
};

class BlockStorage {
	public:
		BlockStorage() : data(Chunk::WIDTH * Chunk::HEIGHT * Chunk::DEPTH, BlockType::AIR) {}

		BlockType& at(int x, int y, int z) {
			return data[x + Chunk::WIDTH * (y + Chunk::HEIGHT * z)];
		}
		const BlockType& at(int x, int y, int z) const {
			return data[x + Chunk::WIDTH * (y + Chunk::HEIGHT * z)];
		}

		const std::vector<BlockType> &getData() const {
			return data;
		}

	private:
		std::vector<BlockType> data;
};

#endif