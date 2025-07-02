#ifndef CHUNK_HPP
#define CHUNK_HPP

#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <ostream>
#include "Block.hpp"
#include "Shader.hpp"
#include "fastnoise/FastNoiseLite.h"

class Chunk {
public:
    static const int WIDTH = 32; // Size of the chunck in blocks
    static const int HEIGHT = 256; // Height of the chunck in blocks
    static const int DEPTH = 32; // Depth of the chunck in blocks

    Chunk(int chunkX, int chunkZ);
    
    void generate();
    BlockType getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockType type);

    const std::vector<glm::vec3>& getVisibleBlocks() const;

    void buildMesh(); // Build the mesh for rendering
    void draw(Shader* shaderProgram) const; // Draw the chunk using the given shader program

private:
    BlockType blocks[WIDTH][HEIGHT][DEPTH]; // 3D array of blocks
    std::vector<glm::vec3> visibleBlocks; // List of visible blocks for rendering
    int originX; // X coordinate of the chunck origin
    int originZ; // Z coordinate of the chunck origin
    GLuint VAO = 0, VBO = 0;
    std::vector<float> meshVertices; // Vertices for the mesh

    void updateVisibleBlocks();
    void addFace(int x, int y, int z, int face); // Add a face to the mesh vertices
};

#endif