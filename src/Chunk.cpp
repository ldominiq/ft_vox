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


Chunk::Chunk(const int chunkX, const int chunkZ) 
    : originX(chunkX * WIDTH), originZ(chunkZ * DEPTH),
      blockIndices(WIDTH * HEIGHT * DEPTH, /*bitsPerEntry=*/4),  // or more, depending on palette size. We could even use 3 as we use less than 8 types of blocks
	m_needsUpdate(true)
{
    generate();
}

bool Chunk::needsUpdate() const {
	return m_needsUpdate;
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
					setBlock(bx, by, bz, BlockType::AIR);
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
				BlockType block = BlockType::AIR;

				if (y < height - 4) {
					block = BlockType::STONE;
				} else if (y < height - 1) {
					block = (biome == BiomeType::DESERT) ? BlockType::SAND : BlockType::DIRT;
				} else if (y < height) {
					block = [&] {
						switch (biome) {
							case BiomeType::DESERT:    return BlockType::SAND;
							case BiomeType::SNOW:      return BlockType::SNOW;
							case BiomeType::MOUNTAIN:  return BlockType::STONE;
							default:                   return BlockType::GRASS;
						}
					}();
				}

				setBlock(x, y, z, block);
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
            int numWorms = (rng() % 10 == 0) ? 1 : 0; //1 out of 5 to generate 1 worm

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

    int index = x + WIDTH * (z + DEPTH * y);
    uint32_t paletteIndex = blockIndices.get(index);
    return palette[paletteIndex];
}

//TODO create a system to compact the data once in a while. So it doesn't get too fragmented after many delete/set calls. If I understood correctly
void Chunk::setBlock(int x, int y, int z, BlockType type) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH) {
        return; // Out of bounds, do nothing
    }

    int index = x + WIDTH * (z + DEPTH * y);

    auto it = paletteMap.find(type);
    uint32_t paletteIndex;

    if (it == paletteMap.end()) {
        paletteIndex = static_cast<uint32_t>(palette.size());
        palette.push_back(type);
        paletteMap[type] = paletteIndex;
    } else {
        paletteIndex = it->second;
    }

    blockIndices.set(index, paletteIndex);

	if (!m_needsUpdate)
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

	blockTypeVector.resize(WIDTH * HEIGHT * DEPTH);

	std::vector<uint32_t> decodedIndices;
	blockIndices.decodeAll(decodedIndices);

	for (size_t i = 0; i < blockTypeVector.size(); ++i) {
		uint32_t paletteIndex = decodedIndices[i];
		blockTypeVector[i] = palette[paletteIndex];
	}

	for (int x = 0; x < WIDTH; ++x) {
		for (int y = 0; y < HEIGHT; ++y) {
			for (int z = 0; z < DEPTH; ++z) {
				BlockType block = blockTypeVector[x + WIDTH * (z + DEPTH * y)];
				if (block == BlockType::AIR) continue;

				bool exposed = false;

				// -X
				exposed |= (x == 0)
					? (adjacentChunks[WEST] == nullptr || adjacentChunks[WEST]->getBlock(WIDTH - 1, y, z) == BlockType::AIR)
					: (blockTypeVector[(x - 1) + WIDTH * (z + DEPTH * y)] == BlockType::AIR);

				// +X
				exposed |= (x == WIDTH - 1)
					? (adjacentChunks[EAST] == nullptr || adjacentChunks[EAST]->getBlock(0, y, z) == BlockType::AIR)
					: (blockTypeVector[(x + 1) + WIDTH * (z + DEPTH * y)] == BlockType::AIR);

				// -Y (bottom)
				exposed |= (y == 0)
					? true
					: (blockTypeVector[x + WIDTH * (z + DEPTH * (y - 1))] == BlockType::AIR);

				// +Y (top)
				exposed |= (y == HEIGHT - 1)
					? true
					: (blockTypeVector[x + WIDTH * (z + DEPTH * (y + 1))] == BlockType::AIR);

				// -Z
				exposed |= (z == 0)
					? (adjacentChunks[SOUTH] == nullptr || adjacentChunks[SOUTH]->getBlock(x, y, DEPTH - 1) == BlockType::AIR)
					: (blockTypeVector[x + WIDTH * ((z - 1) + DEPTH * y)] == BlockType::AIR);

				// +Z
				exposed |= (z == DEPTH - 1)
					? (adjacentChunks[NORTH] == nullptr || adjacentChunks[NORTH]->getBlock(x, y, 0) == BlockType::AIR)
					: (blockTypeVector[x + WIDTH * ((z + 1) + DEPTH * y)] == BlockType::AIR);

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

	for (const auto& block : visibleBlocks) {
		int x = block.x;
		int y = block.y;
		int z = block.z;

		int idx = x + WIDTH * (z + DEPTH * y);
		if (blockTypeVector[idx] == BlockType::AIR) continue;

		// FRONT (+Z)
		if (z == DEPTH - 1) {
			if (!adjacentChunks[NORTH] || adjacentChunks[NORTH]->getBlock(x, y, 0) == BlockType::AIR)
				addFace(x, y, z, 0);
		} else if (blockTypeVector[x + WIDTH * ((z + 1) + DEPTH * y)] == BlockType::AIR) {
			addFace(x, y, z, 0);
		}

		// BACK (-Z)
		if (z == 0) {
			if (!adjacentChunks[SOUTH] || adjacentChunks[SOUTH]->getBlock(x, y, DEPTH - 1) == BlockType::AIR)
				addFace(x, y, z, 1);
		} else if (blockTypeVector[x + WIDTH * ((z - 1) + DEPTH * y)] == BlockType::AIR) {
			addFace(x, y, z, 1);
		}

		// TOP (+Y)
		if (y == HEIGHT - 1 || blockTypeVector[x + WIDTH * (z + DEPTH * (y + 1))] == BlockType::AIR) {
			addFace(x, y, z, 2);
		}

		// BOTTOM (-Y)
		if (y == 0 || blockTypeVector[x + WIDTH * (z + DEPTH * (y - 1))] == BlockType::AIR) {
			addFace(x, y, z, 3);
		}

		// RIGHT (+X)
		if (x == WIDTH - 1) {
			if (!adjacentChunks[EAST] || adjacentChunks[EAST]->getBlock(0, y, z) == BlockType::AIR)
				addFace(x, y, z, 4);
		} else if (blockTypeVector[(x + 1) + WIDTH * (z + DEPTH * y)] == BlockType::AIR) {
			addFace(x, y, z, 4);
		}

		// LEFT (-X)
		if (x == 0) {
			if (!adjacentChunks[WEST] || adjacentChunks[WEST]->getBlock(WIDTH - 1, y, z) == BlockType::AIR)
				addFace(x, y, z, 5);
		} else if (blockTypeVector[(x - 1) + WIDTH * (z + DEPTH * y)] == BlockType::AIR) {
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

    // layout(location = 0) = vec3 position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), static_cast<void *>(nullptr));
    glEnableVertexAttribArray(0);

    // layout(location = 1) = vec2 texCoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void *>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // layout(location = 2) = float vertexY
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void *>(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

	meshVerticesSize = meshVertices.size();
	meshVertices.clear();
	meshVertices.shrink_to_fit(); // optional
	blockTypeVector.clear();
	blockTypeVector.shrink_to_fit();
	visibleBlocks.clear();
	visibleBlocks.shrink_to_fit();
	visibleBlocksSet.clear();
	visibleBlocksSet.rehash(0);
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
    const BlockType type = getBlock(x, y, z);

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

void Chunk::updateChunk()
{
	updateVisibleBlocks();
	buildMesh();
	m_needsUpdate = false;
}

void Chunk::draw(const Shader* shaderProgram) const {
    shaderProgram->use();
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, meshVerticesSize / 3);
}
