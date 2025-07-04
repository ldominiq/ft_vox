#include "Chunk.hpp"


const int ATLAS_COLS = 6;
const int ATLAS_ROWS = 1;



// This function maps block type + face to UV offset
glm::vec2 getTextureOffset(const BlockType type, const int face) {
    int col = 0;
    int row = 0;

    switch (type) {
        case BlockType::GRASS:
            if (face == 2)      { col = 0; row = 0; } // top
            else if (face == 3) { col = 2; row = 0; } // bottom = dirt
            else                { col = 1; row = 0; } // side = grass-side
            break;

        case BlockType::DIRT:
            col = 2; row = 0;
            break;

        case BlockType::STONE:
            col = 3; row = 0;
            break;

        case BlockType::SAND:
            col = 4; row = 0;
            break;

        case BlockType::SNOW:
            col = 5; row = 0;
            break;

        default:
            col = 0; row = 0;
            break;
    }

    return glm::vec2(col, row);
}


Chunk::Chunk(const int chunkX, const int chunkZ) : originX(chunkX * WIDTH), originZ(chunkZ * DEPTH){
    generate();
}

bool Chunk::hasAllAdjacentChunkLoaded() const {
    return adjacentChunks[NORTH] && adjacentChunks[EAST] && adjacentChunks[WEST] && adjacentChunks[SOUTH];
}

void Chunk::setAdjacentChunks(const int direction, Chunk *chunk){
    adjacentChunks[direction] = chunk;
}

void Chunk::carveWorm(Worm &worm) {

    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise.SetFrequency(0.1f);

    glm::vec3 pos = worm.pos;
    float radius = worm.radius;
    int steps = worm.steps;

    glm::vec3 dir = glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f));

    for (int i = 0; i < steps; ++i) {
        float angleX = noise.GetNoise(pos.x, pos.y, pos.z) * 0.5f;
        float angleY = noise.GetNoise(pos.y, pos.z, pos.x) * 0.5f;

        glm::mat4 rot =
            glm::rotate(glm::mat4(1.0f), angleX, glm::vec3(1, 0, 0)) *
            glm::rotate(glm::mat4(1.0f), angleY, glm::vec3(0, 1, 0));
        dir = glm::normalize(glm::vec3(rot * glm::vec4(dir, 0.0f)));

        pos += dir * 1.0f;

        // Carve sphere — only if inside this chunk
        for (int x = -radius; x <= radius; ++x)
        for (int y = -radius; y <= radius; ++y)
        for (int z = -radius; z <= radius; ++z) {
            glm::vec3 offset(x, y, z);
            const glm::vec3 p = pos + offset;

            if (glm::length(offset) <= radius) {
                const int bx = static_cast<int>(p.x - originX);
                const int by = static_cast<int>(p.y);
                const int bz = static_cast<int>(p.z - originZ);

                if (bx >= 0 && bx < WIDTH && by >= 0 && by < HEIGHT && bz >= 0 && bz < DEPTH) {
                    blocks[bx][by][bz] = BlockType::AIR;
                }
            }
        }
    }
}

void Chunk::generate() {
    FastNoiseLite biomeNoise;
    biomeNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    biomeNoise.SetFrequency(0.001f); // low = large biomes

    FastNoiseLite baseNoise;
    baseNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    baseNoise.SetFrequency(0.01f); // standard terrain noise

    FastNoiseLite warp;
    warp.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    warp.SetFrequency(0.02f);

    FastNoiseLite wormNoise;
    wormNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    wormNoise.SetFrequency(0.1f);

    // ========== Base terrain ==========
    for (int x = 0; x < WIDTH; ++x) {
        for (int z = 0; z < DEPTH; ++z) {
            // World-space position
            const int worldX = originX + x;
            const int worldZ = originZ + z;

            float xf = static_cast<float>(worldX);
            float zf = static_cast<float>(worldZ);

            // Domain warp to add terrain folds
            float wx = xf + warp.GetNoise(xf, zf) * 20.0f;
            float wz = zf + warp.GetNoise(zf, xf) * 20.0f;

            
            // Get biome value and normalize to [0,1]
            float biomeBlendFactor = biomeNoise.GetNoise(xf, zf) * 0.5f + 0.5f;

            // Determine biome type
            BiomeType biome;
            if (biomeBlendFactor < 0.2f)       biome = BiomeType::DESERT;
            else if (biomeBlendFactor < 0.4f)  biome = BiomeType::PLAINS;
            else if (biomeBlendFactor < 0.6f)  biome = BiomeType::FOREST;
            else if (biomeBlendFactor < 0.8f)  biome = BiomeType::MOUNTAIN;
            else                biome = BiomeType::SNOW;

            float plainsH   = baseNoise.GetNoise(wx, wz) * 6.0f + 10.0f;
            float forestH   = baseNoise.GetNoise(wx, wz) * 10.0f + 44.0f;
            float desertH   = baseNoise.GetNoise(wx, wz) * 5.0f + 36.0f;
            float mountainH = baseNoise.GetNoise(wx * 0.6f, wz * 0.6f) * 28.0f + 100.0f;
            float snowH     = baseNoise.GetNoise(wx * 0.5f, wz * 0.5f) * 18.0f + 110.0f;


            // Terrain height by biome
            float heightF = 0.0f;

            // b in [0, 1], we split into 4 blend zones
            if (biomeBlendFactor < 0.25f) {
                // desert → plains
                float t = biomeBlendFactor / 0.25f;
                heightF = glm::mix(desertH, plainsH, t);
                biome = (t < 0.5f) ? BiomeType::DESERT : BiomeType::PLAINS;

            } else if (biomeBlendFactor < 0.5f) {
                // plains → forest
                float t = (biomeBlendFactor - 0.25f) / 0.25f;
                heightF = glm::mix(plainsH, forestH, t);
                biome = (t < 0.5f) ? BiomeType::PLAINS : BiomeType::FOREST;

            } else if (biomeBlendFactor < 0.75f) {
                // forest → mountain
                float t = (biomeBlendFactor - 0.5f) / 0.25f;
                heightF = glm::mix(forestH, mountainH, t);
                biome = (t < 0.5f) ? BiomeType::FOREST : BiomeType::MOUNTAIN;

            } else {
                // mountain → snow
                float t = (biomeBlendFactor - 0.75f) / 0.25f;
                heightF = glm::mix(mountainH, snowH, t);
                biome = (t < 0.5f) ? BiomeType::MOUNTAIN : BiomeType::SNOW;
            }

            int height = static_cast<int>(glm::clamp(heightF, 1.0f, (float)HEIGHT - 1));

            // Block layers based on biome
            for (int y = 0; y < HEIGHT; ++y) {
                if (y < height - 4) {
                    blocks[x][y][z] = BlockType::STONE;
                } else if (y < height - 1) {
                    blocks[x][y][z] = (biome == BiomeType::DESERT) ? BlockType::SAND : BlockType::DIRT;
                } else if (y < height) {
                    blocks[x][y][z] = [&] {
                        switch (biome) {
                            case BiomeType::DESERT: return BlockType::SAND;
                            case BiomeType::SNOW:   return BlockType::SNOW;
                            case BiomeType::MOUNTAIN: return BlockType::STONE;
                            default: return BlockType::GRASS;
                        }
                    }();
                } else {
                    blocks[x][y][z] = BlockType::AIR;
                }
            }
        }
    }

    // ========== CAVE GENERATION ==========
    int range = 2; // simulate worms from this many neighboring chunks
    int worldSeed = 1337;

    for (int dx = -range; dx <= range; ++dx) {
        for (int dz = -range; dz <= range; ++dz) {
            int sourceChunkX = originX / WIDTH + dx;
            int sourceChunkZ = originZ / DEPTH + dz;

            // Seed RNG deterministically for each chunk
            unsigned int seed = worldSeed ^ (sourceChunkX * 341873128712 + sourceChunkZ * 132897987541);
            std::mt19937 rng(seed);

            // int numWorms = (rng() % 5); // 0 to 1 worms per chunk
            int numWorms = (rng() % 100 == 0) ? 1 : 0; //1 out of 5 to generate 1 worm

            for (int i = 0; i < numWorms; ++i) {
                float localX = (float)(rng() % WIDTH);
                float localZ = (float)(rng() % DEPTH);
                float worldX = sourceChunkX * WIDTH + localX;
                float worldZ = sourceChunkZ * DEPTH + localZ;
                float worldY = 10 + (rng() % 40); // underground

                Worm worm = Worm(glm::vec3(worldX, worldY, worldZ), 2.0f, 240);
                // worms.emplace_back(glm::vec3(worldX, worldY, worldZ), 2.0f, 300, &wormNoise);

                carveWorm(worm);
            }
        }
    }
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), static_cast<void *>(nullptr));
    glEnableVertexAttribArray(0);

    // layout(location = 1) = vec2 texCoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void *>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // layout(location = 2) = float vertexY
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void *>(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

}

void Chunk::addFace(int x, int y, int z, int face) {
    const float faceX = static_cast<float>(originX + x);
    const float faceY = static_cast<float>(y);
    const float faceZ = static_cast<float>(originZ + z);

    const float TILE_W = 1.0f / ATLAS_COLS;
    const float TILE_H = 1.0f / ATLAS_ROWS;

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
    const BlockType type = blocks[x][y][z];

    // Determine UV offset in atlas based on block type and face
    glm::vec2 tileCoord = getTextureOffset(type, face);
    glm::vec2 offset = { tileCoord.x * TILE_W, tileCoord.y * TILE_H };


    for (int i = 0; i < 6; ++i) {
        float px = faceX + faceData[face][i * 3 + 0];
        float py = faceY + faceData[face][i * 3 + 1];
        float pz = faceZ + faceData[face][i * 3 + 2];

        float baseU = uvCoords[i * 2 + 0]; // 0 → 1
        float baseV = uvCoords[i * 2 + 1]; // 0 → 1

        float u = baseU * TILE_W + offset.x;
        float v = baseV * TILE_H + offset.y;


        meshVertices.push_back(px);
        meshVertices.push_back(py);
        meshVertices.push_back(pz);
        meshVertices.push_back(u);
        meshVertices.push_back(v);
        meshVertices.push_back(py);  // send Y again for gradient
    }


}

void Chunk::draw(const Shader* shaderProgram) const {
    shaderProgram->use();
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, meshVertices.size() / 3);
}