#include "Chunk.hpp"
#include "World.hpp"

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
	visibleBlocksSet.reserve(WIDTH * DEPTH * HEIGHT);
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
                    voxels[bx][by][bz].type = BlockType::AIR;
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
    baseNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
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
                    voxels[x][y][z].type = BlockType::STONE;
                } else if (y < height - 1) {
                    voxels[x][y][z].type = (biome == BiomeType::DESERT) ? BlockType::SAND : BlockType::DIRT;
                } else if (y < height) {
                    voxels[x][y][z].type = [&] {
                        switch (biome) {
                            case BiomeType::DESERT: return BlockType::SAND;
                            case BiomeType::SNOW:   return BlockType::SNOW;
                            case BiomeType::MOUNTAIN: return BlockType::STONE;
                            default: return BlockType::GRASS;
                        }
                    }();
                } else {
                    voxels[x][y][z].type = BlockType::AIR;
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
            int numWorms = (rng() % 50 == 0) ? 1 : 0; //1 out of 5 to generate 1 worm

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
    initializeSkyLight();
    propagateSkyLight();
}


BlockType Chunk::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH) {
        return BlockType::AIR; // Out of bounds returns air
    }
    return voxels[x][y][z].type;
}

void Chunk::setBlock(World *world, int x, int y, int z, BlockType type) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH) {
        return; // Out of bounds, do nothing
    }
    voxels[x][y][z].type = type;

	updateChunk();

	//update possible neighbour
	if (x == 0 && adjacentChunks[WEST])	adjacentChunks[WEST]->updateChunk();
	if (x == WIDTH - 1 && adjacentChunks[EAST]) adjacentChunks[EAST]->updateChunk();
	if (z == 0 && adjacentChunks[SOUTH]) adjacentChunks[SOUTH]->updateChunk();
	if (z == DEPTH - 1 && adjacentChunks[NORTH]) adjacentChunks[NORTH]->updateChunk();
}

void Chunk::updateVisibleBlocks() {
    visibleBlocks.clear();
	visibleBlocksSet.clear();

    for (int x = 0; x < WIDTH; ++x) {
        for (int y = 0; y < HEIGHT; ++y) {
            for (int z = 0; z < DEPTH; ++z) {
                if (voxels[x][y][z].type == BlockType::AIR) continue;

                bool exposed = false;

                // -X direction
                if (x == 0) {
                    exposed |= adjacentChunks[WEST] == nullptr || 
                               adjacentChunks[WEST]->getBlock(WIDTH - 1, y, z) == BlockType::AIR;
                } else {
                    exposed |= voxels[x - 1][y][z].type == BlockType::AIR;
                }

                // +X direction
                if (x == WIDTH - 1) {
                    exposed |= adjacentChunks[EAST] == nullptr || 
                               adjacentChunks[EAST]->getBlock(0, y, z) == BlockType::AIR;
                } else {
                    exposed |= voxels[x + 1][y][z].type == BlockType::AIR;
                }

                // -Y direction (bottom)
                if (y == 0) {
                    exposed = true;
                } else {
                    exposed |= voxels[x][y - 1][z].type == BlockType::AIR;
                }

                // +Y direction (top)
                if (y == HEIGHT - 1) {
                    exposed = true;
                } else {
                    exposed |= voxels[x][y + 1][z].type == BlockType::AIR;
                }

                // -Z direction
                if (z == 0) {
                    exposed |= adjacentChunks[SOUTH] == nullptr || 
                               adjacentChunks[SOUTH]->getBlock(x, y, DEPTH - 1) == BlockType::AIR;
                } else {
                    exposed |= voxels[x][y][z - 1].type == BlockType::AIR;
                }

                // +Z direction
                if (z == DEPTH - 1) {
                    exposed |= adjacentChunks[NORTH] == nullptr || 
                               adjacentChunks[NORTH]->getBlock(x, y, 0) == BlockType::AIR;
                } else {
                    exposed |= voxels[x][y][z + 1].type == BlockType::AIR;
                }

                if (exposed) {
                    visibleBlocks.emplace_back(x, y, z);
					visibleBlocksSet.insert(glm::ivec3(x, y, z));
                }
            }
        }
    }
}

bool Chunk::isBlockVisible(glm::ivec3 blockPos)
{
	return visibleBlocksSet.count(blockPos) > 0;
}

const std::vector<glm::ivec3>& Chunk::getVisibleBlocks() const {
    return visibleBlocks;
}

void Chunk::buildMesh() {
    meshVertices.clear();

	for (auto block : visibleBlocks) {
		int x = block.x;
		int y = block.y;
		int z = block.z;

		if (voxels[x][y][z].type == BlockType::AIR) continue;

		// FRONT (+Z)
		if (z == DEPTH - 1) {
			if (!adjacentChunks[NORTH] || adjacentChunks[NORTH]->getBlock(x, y, 0) == BlockType::AIR)
				addFace(x, y, z, 0);
		} else if (voxels[x][y][z + 1].type == BlockType::AIR) {
			addFace(x, y, z, 0);
		}

		// BACK (-Z)
		if (z == 0) {
			if (!adjacentChunks[SOUTH] || adjacentChunks[SOUTH]->getBlock(x, y, DEPTH - 1) == BlockType::AIR)
				addFace(x, y, z, 1);
		} else if (voxels[x][y][z - 1].type == BlockType::AIR) {
			addFace(x, y, z, 1);
		}

		// TOP (+Y) – no chunk above, so no neighbor
		if (y == HEIGHT - 1 || voxels[x][y + 1][z].type == BlockType::AIR)
			addFace(x, y, z, 2);

		// BOTTOM (-Y) – no chunk below, so no neighbor
		if (y == 0 || voxels[x][y - 1][z].type == BlockType::AIR)
			addFace(x, y, z, 3);

		// RIGHT (+X)
		if (x == WIDTH - 1) {
			if (!adjacentChunks[EAST] || adjacentChunks[EAST]->getBlock(0, y, z) == BlockType::AIR)
				addFace(x, y, z, 4);
		} else if (voxels[x + 1][y][z].type == BlockType::AIR) {
			addFace(x, y, z, 4);
		}

		// LEFT (-X)
		if (x == 0) {
			if (!adjacentChunks[WEST] || adjacentChunks[WEST]->getBlock(WIDTH - 1, y, z) == BlockType::AIR)
				addFace(x, y, z, 5);
		} else if (voxels[x - 1][y][z].type == BlockType::AIR) {
			addFace(x, y, z, 5);
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

    // Currantly, we use 9 floats per vertex:
    // 3 for position, 2 for texture coordinates, 1 for vertex Y (for gradient), and 3 for normal.
    GLsizei stride = 10 * sizeof(float);

    // layout(location = 0) = vec3 position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, static_cast<void *>(nullptr));
    glEnableVertexAttribArray(0);

    // layout(location = 1) = vec2 texCoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // layout(location = 2) = float vertexY (gradient)
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // layout(location = 3) = float light
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(6 * sizeof(float)));
    glEnableVertexAttribArray(3);

    // layout(location = 4) = vec3 normal
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(7 * sizeof(float)));
    glEnableVertexAttribArray(4);
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

    static const glm::vec3 faceNormals[6] = {
        {  0,  0,  1 }, // front
        {  0,  0, -1 }, // back
        {  0,  1,  0 }, // top
        {  0, -1,  0 }, // bottom
        {  1,  0,  0 }, // right
        { -1,  0,  0 }  // left
    };

    glm::vec3 normal = faceNormals[face];

    // Get block type for this position
    const BlockType type = voxels[x][y][z].type;

    // Determine UV offset in atlas based on block type and face
    glm::vec2 tileCoord = getTextureOffset(type, face);
    glm::vec2 offset = { tileCoord.x * TILE_W, tileCoord.y * TILE_H };

    // Determine the light for this face
    uint8_t neighbourSky = 0;
    uint8_t neighbourBlock = 0;
    switch (face) {
        case 0: { // front (+Z)
            int nz = z + 1;
            if (nz < DEPTH) {
                const Voxel& v = voxels[x][y][nz];
                neighbourSky = v.skyLight;
                neighbourBlock = v.blockLight;
            } else if (adjacentChunks[NORTH]) {
                const Voxel& v = adjacentChunks[NORTH]->voxels[x][y][0];
                neighbourSky = v.skyLight;
                neighbourBlock = v.blockLight;
            } else {
                // If no neighbouring chunk is loaded, fall back to this block's own light
                neighbourSky = voxels[x][y][z].skyLight;
                neighbourBlock = voxels[x][y][z].blockLight;
            }
            break;
        }
        case 1: { // back (–Z)
            int nz = z - 1;
            if (nz >= 0) {
                const Voxel& v = voxels[x][y][nz];
                neighbourSky = v.skyLight;
                neighbourBlock = v.blockLight;
            } else if (adjacentChunks[SOUTH]) {
                const Voxel& v = adjacentChunks[SOUTH]->voxels[x][y][DEPTH - 1];
                neighbourSky = v.skyLight;
                neighbourBlock = v.blockLight;
            } else {
                neighbourSky = voxels[x][y][z].skyLight;
                neighbourBlock = voxels[x][y][z].blockLight;
            }
            break;
        }
        case 2: { // top (+Y)
            int ny = y + 1;
            if (ny < HEIGHT) {
                const Voxel& v = voxels[x][ny][z];
                neighbourSky = v.skyLight;
                neighbourBlock = v.blockLight;
            } else {
                neighbourSky = 15; // nothing above so full sunlight
            }
            break;
        }
        case 3: { // bottom (–Y)
            int ny = y - 1;
            if (ny >= 0) {
                const Voxel& v = voxels[x][ny][z];
                neighbourSky = v.skyLight;
                neighbourBlock = v.blockLight;
            } else {
                neighbourSky = 0; // underground
            }
            break;
        }
        case 4: { // right (+X)
            int nx = x + 1;
            if (nx < WIDTH) {
                const Voxel& v = voxels[nx][y][z];
                neighbourSky = v.skyLight;
                neighbourBlock = v.blockLight;
            } else if (adjacentChunks[EAST]) {
                const Voxel& v = adjacentChunks[EAST]->voxels[0][y][z];
                neighbourSky = v.skyLight;
                neighbourBlock = v.blockLight;
            } else {
                neighbourSky = voxels[x][y][z].skyLight;
                neighbourBlock = voxels[x][y][z].blockLight;
            }
            break;
        }
        case 5: { // left (–X)
            int nx = x - 1;
            if (nx >= 0) {
                const Voxel& v = voxels[nx][y][z];
                neighbourSky = v.skyLight;
                neighbourBlock = v.blockLight;
            } else if (adjacentChunks[WEST]) {
                const Voxel& v = adjacentChunks[WEST]->voxels[WIDTH - 1][y][z];
                neighbourSky = v.skyLight;
                neighbourBlock = v.blockLight;
            } else {
                neighbourSky = voxels[x][y][z].skyLight;
                neighbourBlock = voxels[x][y][z].blockLight;
            }
            break;
        }
    }
    // Take into account both the block's own light and the neighbouring light.
    // A solid block may have skyLight if it is the topmost block in a column.
    uint8_t blockSky   = voxels[x][y][z].skyLight;
    uint8_t blockBlock = voxels[x][y][z].blockLight;
    uint8_t combinedSky;
    uint8_t combinedBlock;
    // Decide how to combine block and neighbour light depending on face orientation.
    // Top face: use the brighter of the block and neighbour.
    // Bottom face: use the neighbour.
    // Vertical faces: use the neighbour but dim it slightly to avoid bright stripes.
    switch (face) {
        case 2: { // top (+Y)
            combinedSky   = std::max(blockSky, neighbourSky);
            combinedBlock = std::max(blockBlock, neighbourBlock);
            // Dim the top face slightly to avoid pure white surfaces by reducing the light by two levels
            if (combinedSky > 2) {
                combinedSky -= 2;
            } else {
                combinedSky = 0;
            }
            if (combinedBlock > 2) {
                combinedBlock -= 2;
            } else {
                combinedBlock = 0;
            }
            break;
        }
        case 3: { // bottom (-Y)
            combinedSky   = neighbourSky;
            combinedBlock = neighbourBlock;
            break;
        }
        default: {
            // vertical faces: rely on neighbour's light but attenuate it by one level
            // Dim the neighbour light for vertical faces by two levels to prevent very bright strips.
            uint8_t attenuatedSky   = (neighbourSky   > 1) ? static_cast<uint8_t>(neighbourSky   - 2) : 0;
            uint8_t attenuatedBlock = (neighbourBlock > 1) ? static_cast<uint8_t>(neighbourBlock - 2) : 0;
            combinedSky   = attenuatedSky;
            combinedBlock = attenuatedBlock;
            break;
        }
    }
    // Normalize the resulting light to the range [0,1]
    float lightVal = static_cast<float>(std::max(combinedSky, combinedBlock)) / 15.0f;
    // Clamp to a minimum brightness so faces are never completely black and never pure white.
    // This mixes the computed light with a base ambient term. Adjust the constants to taste.
    lightVal = 0.2f + 0.8f * lightVal;

    // Build six vertices for this face using the computed light
    for (int i = 0; i < 6; ++i) {
        float px = faceX + faceData[face][i * 3 + 0];
        float py = faceY + faceData[face][i * 3 + 1];
        float pz = faceZ + faceData[face][i * 3 + 2];

        float baseU = uvCoords[i * 2 + 0]; // 0 → 1
        float baseV = uvCoords[i * 2 + 1]; // 0 → 1

        float u = baseU * TILE_W + offset.x;
        float v = baseV * TILE_H + offset.y;

        meshVertices.push_back(px);    // position.x
        meshVertices.push_back(py);    // position.y
        meshVertices.push_back(pz);    // position.z
        meshVertices.push_back(u);     // texture u
        meshVertices.push_back(v);     // texture v
        meshVertices.push_back(py);    // send Y again for gradient
        meshVertices.push_back(lightVal); // computed light
        meshVertices.push_back(normal.x);
        meshVertices.push_back(normal.y);
        meshVertices.push_back(normal.z);
    }

}

void Chunk::updateChunk()
{
	updateVisibleBlocks();
	buildMesh();
}

void Chunk::draw(const Shader* shaderProgram) const {
    shaderProgram->use();
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, meshVertices.size() / 10); // 6 floats per vertex (3 pos + 2 tex + 1 Y)
}

void Chunk::initializeSkyLight() {
    // Assign full sunlight to all air blocks above the first solid block in each column
    // and to the first solid block itself. All blocks below the topmost solid block
    // start with 0 skyLight (in shadow).
    for (int x = 0; x < WIDTH; ++x) {
        for (int z = 0; z < DEPTH; ++z) {
            bool hitSolid = false;
            for (int y = HEIGHT - 1; y >= 0; --y) {
                Voxel& voxel = voxels[x][y][z];
                // Reset block light to zero to avoid uninitialised values causing bright spots
                voxel.blockLight = 0;
                if (!hitSolid) {
                    // Before hitting a solid block (including the solid itself), assign full light
                    voxel.skyLight = 15;
                    if (voxel.type != BlockType::AIR) {
                        // Mark that we've encountered the topmost solid block
                        hitSolid = true;
                    }
                } else {
                    // Once we've hit a solid block, remaining blocks are initially dark
                    voxel.skyLight = 0;
                }
            }
        }
    }
}

void Chunk::propagateSkyLight() {
    std::queue<Node> queue;
    //enqueue all sky-lit voxels on the chunk surface
    for (int x=0; x<WIDTH; ++x) {
        for (int z=0; z<DEPTH; ++z) {
            for (int y=HEIGHT-1; y>=0 && voxels[x][y][z].skyLight == 15; --y) {
                queue.push({x, y, z, 15});
            }
        }
    }

    // BFS (Flood-fill algorithm)
    static const int dx[] = {1,-1,0,0,0,0};
    static const int dy[] = {0,0,1,-1,0,0};
    static const int dz[] = {0,0,0,0,1,-1};
    while (!queue.empty()) {
        Node node = queue.front();
        queue.pop();
        for (int i=0; i<6; ++i) {
            int nx = node.x + dx[i];
            int ny = node.y + dy[i];
            int nz = node.z + dz[i];
            if (nx < 0 || nx >= WIDTH ||
                ny < 0 || ny >= HEIGHT ||
                nz < 0 || nz >= DEPTH) continue;
            Voxel& voxel = voxels[nx][ny][nz];
            // Skip if block is opaque
            if (voxel.type != BlockType::AIR) continue;
            // Next light level
            int nextLight = node.light - 1;
            if (nextLight > (int)voxel.skyLight) {
                voxel.skyLight = nextLight;
                if (nextLight > 1) {
                    queue.push({nx, ny, nz, nextLight});
                }
            }
        }
    }
}

float Chunk::getLight(int x, int y, int z) const {
    const Voxel &voxel = voxels[x][y][z];
    int maxLight = std::max(voxel.skyLight, voxel.blockLight);
    return static_cast<float>(maxLight) / 15.0f;
}
