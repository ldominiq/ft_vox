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

bool Chunk::hasAllAdjacentChunkLoaded() const
{
    return (adjacentChunks[NORTH] && adjacentChunks[EAST] && adjacentChunks[WEST] && adjacentChunks[SOUTH]);
}

void Chunk::setAdjacentChunks(int direction, Chunk *chunk){
    adjacentChunks[direction] = chunk;
}

void Chunk::carveWorm(glm::vec3 startPos, float radius, int steps, FastNoiseLite& noise) {
    glm::vec3 pos = startPos;
    glm::vec3 dir = glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f)); // initial forward

    for (int i = 0; i < steps; ++i) {
        // Apply Perlin noise to rotate direction
        float angleX = noise.GetNoise(pos.x, pos.y, pos.z) * 0.3f;
        float angleY = noise.GetNoise(pos.y, pos.z, pos.x) * 0.3f;

        glm::mat4 rot =
            glm::rotate(glm::mat4(1.0f), angleX, glm::vec3(1, 0, 0)) *
            glm::rotate(glm::mat4(1.0f), angleY, glm::vec3(0, 1, 0));
        dir = glm::normalize(glm::vec3(rot * glm::vec4(dir, 0.0f)));

        // Move forward
        pos += dir * 1.0f;

        // Carve a spherical area at pos
        for (int x = -radius; x <= radius; ++x)
        for (int y = -radius; y <= radius; ++y)
        for (int z = -radius; z <= radius; ++z) {
            glm::vec3 offset(x, y, z);
            glm::vec3 p = pos + offset;

            if (glm::length(offset) <= radius) {
                int bx = (int)(p.x - originX);
                int by = (int)(p.y);
                int bz = (int)(p.z - originZ);

                if (bx >= 0 && bx < WIDTH && by >= 0 && by < HEIGHT && bz >= 0 && bz < DEPTH) {
                    blocks[bx][by][bz] = BlockType::AIR;
                }
            }
        }
    }
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

    FastNoiseLite wormNoise;
    wormNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    wormNoise.SetFrequency(0.1f);

    for (int x = 0; x < WIDTH; ++x) {
        for (int z = 0; z < DEPTH; ++z) {
            // World-space position
            int worldX = originX + x;
            int worldZ = originZ + z;

            // Warping: displace coordinates for baseNoise
            float wx = worldX + warp.GetNoise((float)worldX, (float)worldZ) * 2.0f;
            float wz = worldZ + warp.GetNoise((float)worldZ, (float)worldX) * 2.0f;

            float base = noise.GetNoise(wx, wz);
            float ridges = 1.0f - fabs(ridgeNoise.GetNoise((float)worldX, (float)worldZ));

            float heightF = base * 15.0f + ridges * 10.0f + 60.0f; // + 60.0f = base terrain altitude
            int height = static_cast<int>(glm::clamp(heightF, 1.0f, (float)HEIGHT - 1));

            for (int y = 0; y < HEIGHT; ++y) {
                // Set blocks based on height
                if (y < height - 4) {
                    blocks[x][y][z] = BlockType::STONE; // Stone below the surface
                } else if (y <= height - 1) {
                    blocks[x][y][z] = BlockType::DIRT; // Dirt layer
                } else if (y == height) {
                    blocks[x][y][z] = BlockType::GRASS; // Grass on top
                } else {
                    blocks[x][y][z] = BlockType::AIR; // Air above the surface
                }
            }
        }
    }


    for (int i = 0; i < 4; ++i) {
        float x = originX + rand() % WIDTH;
        float y = 20 + rand() % 30; // underground
        float z = originZ + rand() % DEPTH;
        carveWorm(glm::vec3(x, y, z), 2.0f, 200, wormNoise);
    }

    // updateVisibleBlocks();
    // buildMesh();
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

                bool exposed = false;

                // -X direction
                if (x == 0) {
                    exposed |= adjacentChunks[WEST] == nullptr || 
                               adjacentChunks[WEST]->getBlock(WIDTH - 1, y, z) == BlockType::AIR;
                } else {
                    exposed |= blocks[x - 1][y][z] == BlockType::AIR;
                }

                // +X direction
                if (x == WIDTH - 1) {
                    exposed |= adjacentChunks[EAST] == nullptr || 
                               adjacentChunks[EAST]->getBlock(0, y, z) == BlockType::AIR;
                } else {
                    exposed |= blocks[x + 1][y][z] == BlockType::AIR;
                }

                // -Y direction (bottom)
                if (y == 0) {
                    exposed = false;
                } else {
                    exposed |= blocks[x][y - 1][z] == BlockType::AIR;
                }

                // +Y direction (top)
                if (y == HEIGHT - 1) {
                    exposed = false;
                } else {
                    exposed |= blocks[x][y + 1][z] == BlockType::AIR;
                }

                // -Z direction
                if (z == 0) {
                    exposed |= adjacentChunks[SOUTH] == nullptr || 
                               adjacentChunks[SOUTH]->getBlock(x, y, DEPTH - 1) == BlockType::AIR;
                } else {
                    exposed |= blocks[x][y][z - 1] == BlockType::AIR;
                }

                // +Z direction
                if (z == DEPTH - 1) {
                    exposed |= adjacentChunks[NORTH] == nullptr || 
                               adjacentChunks[NORTH]->getBlock(x, y, 0) == BlockType::AIR;
                } else {
                    exposed |= blocks[x][y][z + 1] == BlockType::AIR;
                }

                if (exposed) {
                    visibleBlocks.emplace_back(x, y, z);
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

    for (auto block : visibleBlocks)
    {
        int x = block.x;
        int y = block.y;
        int z = block.z;

        if (blocks[x][y][z] == BlockType::AIR) continue;
        // Check each face of the block

        if (z == DEPTH - 1 || blocks[x][y][z + 1] == BlockType::AIR) addFace(x, y, z, 0); // Front face (Z+)
        if (z == 0 || blocks[x][y][z - 1] == BlockType::AIR) addFace(x, y, z, 1); // Back face (Z-)
        if (y == HEIGHT - 1 || blocks[x][y + 1][z] == BlockType::AIR) addFace(x, y, z, 2); // Top face (Y+)
        if (y == 0 || blocks[x][y - 1][z] == BlockType::AIR) addFace(x, y, z, 3); // Bottom face (Y-)
        if (x == WIDTH - 1 || blocks[x + 1][y][z] == BlockType::AIR) addFace(x, y, z, 4); // Right face (X+)
        if (x == 0 || blocks[x - 1][y][z] == BlockType::AIR) addFace(x, y, z, 5); // Left face (X-)
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

void Chunk::draw(Shader* shaderProgram) const {
    shaderProgram->use();
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, meshVertices.size() / 3);
}

#endif