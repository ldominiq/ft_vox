#ifndef CHUNCK_HPP
#define CHUNCK_HPP

#include "Chunk.hpp"


const float TILE_SIZE = 1.0f / 4.0f; // 0.25f for a 4x4 texture atlas

// This function maps block type + face to UV offset
glm::vec2 getTextureOffset(BlockType type, int face) {
    switch (type) {
        case BlockType::GRASS:
            if (face == 2) return { 0.0f, 0.0f }; // top = grass
            else if (face == 3) return { 2.0f * TILE_SIZE, 0.0f }; // bottom = dirt
            else return { 1.0f * TILE_SIZE, 0.0f }; // side = grass-side

        case BlockType::DIRT:
            return { 2.0f * TILE_SIZE, 0.0f };

        case BlockType::STONE:
            return { 3.0f * TILE_SIZE, 0.0f };

        default:
            return { 0.0f, 0.0f };
    }
}


Chunk::Chunk(int chunkX, int chunkZ) : originX(chunkX * WIDTH), originZ(chunkZ * DEPTH){
    generate();
}

void Chunk::generate() {
    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFrequency(0.005f); // Lower frequency = wider terrain

    FastNoiseLite ridgeNoise;
    ridgeNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    ridgeNoise.SetFrequency(0.01f); // Ridge noise for terrain features

    FastNoiseLite warp;
    warp.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    warp.SetFrequency(0.02f);

    for (int x = 0; x < WIDTH; ++x) {
        for (int z = 0; z < DEPTH; ++z) {
            // World-space position
            int worldX = originX + x;
            int worldZ = originZ + z;

            // Warping: displace coordinates for baseNoise
            float wx = worldX + warp.GetNoise((float)worldX, (float)worldZ) * 20.0f;
            float wz = worldZ + warp.GetNoise((float)worldZ, (float)worldX) * 20.0f;

            float base = noise.GetNoise(wx, wz);
            float ridges = 1.0f - fabs(ridgeNoise.GetNoise((float)worldX, (float)worldZ));

            float heightF = base * 15.0f + ridges * 10.0f + 20.0f; // + 40.0f = base terrain altitude
            int height = static_cast<int>(glm::clamp(heightF, 1.0f, (float)HEIGHT - 1));

            for (int y = 0; y < HEIGHT; ++y) {
                // Set blocks based on height
                if (y < height - 4) {
                    blocks[x][y][z] = BlockType::STONE; // Stone below the surface
                } else if (y < height - 1) {
                    blocks[x][y][z] = BlockType::DIRT; // Dirt layer
                } else if (y == height) {
                    blocks[x][y][z] = BlockType::GRASS; // Grass on top
                } else {
                    blocks[x][y][z] = BlockType::AIR; // Air above the surface
                }
            }
        }
    }

    
    updateVisibleBlocks();
    buildMesh();
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

    // layout(location = 0) = vec3 position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // layout(location = 1) = vec2 texCoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // layout(location = 2) = float vertexY
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

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

    static const float uvCoords[12] = {
        0, 0,
        1, 0,
        1, 1,
        1, 1,
        0, 1,
        0, 0
    };

    // Get block type for this position
    BlockType type = blocks[x][y][z];

    // Determine UV offset in atlas based on block type and face
    glm::vec2 offset;
    switch (type) {
        case BlockType::GRASS:
            if (face == 2) offset = { 0.0f, 0.0f }; // top = grass-top
            else if (face == 3) offset = { 2.0f * TILE_SIZE, 0.0f }; // bottom = dirt
            else offset = { 1.0f * TILE_SIZE, 0.0f }; // side = grass-side
            break;
        case BlockType::DIRT:
            offset = { 2.0f * TILE_SIZE, 0.0f };
            break;
        case BlockType::STONE:
            offset = { 3.0f * TILE_SIZE, 0.0f };
            break;
        default:
            offset = { 0.0f, 0.0f };
            break;
    }


    for (int i = 0; i < 6; ++i) {
        float px = faceX + faceData[face][i * 3 + 0];
        float py = faceY + faceData[face][i * 3 + 1];
        float pz = faceZ + faceData[face][i * 3 + 2];

        float u = uvCoords[i * 2 + 0] * TILE_SIZE + offset.x;
        float v = uvCoords[i * 2 + 1]; // DO NOT scale v — we only have 1 row


        meshVertices.push_back(px);
        meshVertices.push_back(py);
        meshVertices.push_back(pz);
        meshVertices.push_back(u);
        meshVertices.push_back(v);
        meshVertices.push_back(py);  // send Y again for gradient
    }


}

void Chunk::draw(GLuint shaderProgram) const {
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, meshVertices.size() / 3);
}

#endif