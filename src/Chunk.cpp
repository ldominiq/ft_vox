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
            int height = 1 + (rand() % 16); // Random height between 1 and 4
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

void Chunk::buildMesh() {
    meshVertices.clear();
    for (int x = 0; x < WIDTH; ++x) {
        for (int y = 0; y < HEIGHT; ++y) {
            for (int z = 0; z < DEPTH; ++z) {
                if (blocks[x][y][z] == BlockType::AIR) continue;
                // Check each face of the block
                if (z == DEPTH - 1 || blocks[x][y][z + 1] == BlockType::AIR) addFace(x, y, z, 0); // Front face (Z+)
                if (z == 0 || blocks[x][y][z - 1] == BlockType::AIR) addFace(x, y, z, 1); // Back face (Z-)
                if (y == HEIGHT - 1 || blocks[x][y + 1][z] == BlockType::AIR) addFace(x, y, z, 2); // Top face (Y+)
                if (y == 0 || blocks[x][y - 1][z] == BlockType::AIR) addFace(x, y, z, 3); // Bottom face (Y-)
                if (x == WIDTH - 1 || blocks[x + 1][y][z] == BlockType::AIR) addFace(x, y, z, 4); // Right face (X+)
                if (x == 0 || blocks[x - 1][y][z] == BlockType::AIR) addFace(x, y, z, 5); // Left face (X-)
            }
        }
    }

    // Upload mesh data to OpenGL
    if (VAO == 0)
        glGenVertexArrays(1, &VAO);
    if (VBO == 0)
        glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(float), meshVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

void Chunk::addFace(int x, int y, int z, int face) {
    float faceX = static_cast<float>(originX + x);
    float faceY = static_cast<float>(y);
    float faceZ = static_cast<float>(originZ + z);

    static const float faceData[6][18] = {
        // FRONT face (Z+)
        { 0,0,1,  1,0,1,  1,1,1,
        1,1,1,  0,1,1,  0,0,1 },

        // BACK face (Z-)
        { 1,0,0,  0,0,0,  0,1,0,
        0,1,0,  1,1,0,  1,0,0 },

        // TOP face (Y+)
        { 0,1,1,  1,1,1,  1,1,0,
        1,1,0,  0,1,0,  0,1,1 },

        // BOTTOM face (Y-)
        { 0,0,0,  1,0,0,  1,0,1,
        1,0,1,  0,0,1,  0,0,0 },

        // RIGHT face (X+)
        { 1,0,1,  1,0,0,  1,1,0,
        1,1,0,  1,1,1,  1,0,1 },

        // LEFT face (X-)
        { 0,0,0,  0,0,1,  0,1,1,
        0,1,1,  0,1,0,  0,0,0 }
    };


    for (int i = 0; i < 18; i += 3) {
        meshVertices.push_back(faceX + faceData[face][i]);
        meshVertices.push_back(faceY + faceData[face][i + 1]);
        meshVertices.push_back(faceZ + faceData[face][i + 2]);
    }

}

void Chunk::draw(GLuint shaderProgram) const {
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, meshVertices.size() / 3);
}

#endif