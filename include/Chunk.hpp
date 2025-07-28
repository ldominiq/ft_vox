#ifndef CHUNK_HPP
#define CHUNK_HPP

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <glad/glad.h>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <ostream>
#include "Block.hpp"
#include "Shader.hpp"
#include "FastNoiseLite.h"

#include <random>
#include <unordered_set>
#include <queue>

class World;

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

enum Direction {
	NORTH = 0,
	SOUTH,
	EAST,
	WEST
};

enum class BiomeType {
    PLAINS,
    DESERT,
    FOREST,
    MOUNTAIN,
    SNOW
};

struct Node { int x, y, z, light; };

class Chunk {
public:
    static constexpr int WIDTH = 16; // Size of the chunck in blocks
    static constexpr int HEIGHT = 256; // Height of the chunck in blocks
    static constexpr int DEPTH = 16; // Depth of the chunck in blocks

    Chunk(int chunkX, int chunkZ);
    
    void carveWorm(Worm &worm);
    void generate();

    BlockType getBlock(int x, int y, int z) const;
    void setBlock(World *world, int x, int y, int z, BlockType type);

    const std::vector<glm::ivec3>& getVisibleBlocks() const;

	bool isBlockVisible(glm::ivec3 blockPos);

    void draw(const Shader* shaderProgram) const; // Draw the chunk using the given shader program

	void setAdjacentChunks(int direction, Chunk *chunk);
	bool hasAllAdjacentChunkLoaded() const;

	void updateChunk(); //updateVisibleBlocks + buildMesh. Used when updating blocks in a chunk

	void initializeSkyLight();
	void propagateSkyLight();

	float getLight(int x, int y, int z) const;

private:
	Chunk *adjacentChunks[4] = {};

    Voxel voxels[WIDTH][HEIGHT][DEPTH]; // 3D array of blocks
    std::vector<glm::ivec3> visibleBlocks; // List of visible blocks for rendering
	std::unordered_set<glm::ivec3, IVec3Hash> visibleBlocksSet; //copy of visibly blocks for 0(1) Lookup
	
    int originX; // X coordinate of the chunck origin
    int originZ; // Z coordinate of the chunck origin
    GLuint VAO = 0, VBO = 0;
    std::vector<float> meshVertices; // Vertices for the mesh

    void addFace(int x, int y, int z, int face); // Add a face to the mesh vertices

	void updateVisibleBlocks();
	void buildMesh(); // Build the mesh for rendering
};

#endif