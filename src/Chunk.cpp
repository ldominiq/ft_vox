#include "Chunk.hpp"
#include "World.hpp"

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

        case BlockType::WATER:
            col = 6; row = 0;
            break;
            
        case BlockType::BEDROCK:
            col = 7; row = 0;
            break;

        default:
            col = 0; row = 0;
            break;
    }

    return glm::vec2(col, row);
}

static inline float remap01(float v) { return 0.5f * (v + 1.0f); }  // [-1,1] -> [0,1]
static inline float clamp01(float v) { return glm::clamp(v, 0.0f, 1.0f); }
static inline float smooth01(float v){ return glm::smoothstep(0.0f, 1.0f, v); }



Chunk::Chunk(const int chunkX, const int chunkZ, const TerrainGenerationParams& params, const bool doGenerate)
    : originX(chunkX * WIDTH), originZ(chunkZ * DEPTH),
      blockIndices(WIDTH * HEIGHT * DEPTH, /*bitsPerEntry=*/4), currentParams(params)  // or more, depending on palette size. We could even use 3 as we use less than 8 types of blocks
{
	if (doGenerate)
    	generate(params);
	else preGenerated = true;
    	
}

Chunk::~Chunk() {
    // Free resources for the GPU memory
    if (VAO) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
}


bool Chunk::hasAllAdjacentChunkLoaded() const {
    for (const auto& adj : adjacentChunks) {
        if (adj.expired()) {
            return false;
        }
    }
    return true;
}

void Chunk::setAdjacentChunks(const int direction, std::shared_ptr<Chunk> &chunk){
    adjacentChunks[direction] = chunk;
}

void Chunk::carveWorm(Worm &worm, BlockStorage &blocks) {

    Noise noise;
    noise.setFrequency(0.1f); // Adjust frequency for worm carving
    
    glm::vec3 pos = worm.pos;
    float radius = worm.radius;
    int steps = worm.steps;

    glm::vec3 dir = glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f));

    for (int i = 0; i < steps; ++i) {
        float angleX = noise.getNoise(pos.x, pos.y, pos.z) * 0.5f;
        float angleY = noise.getNoise(pos.y, pos.z, pos.x) * 0.5f;

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
            const glm::vec3 params = pos + offset;

            if (glm::length(offset) <= radius) {
                const int bx = static_cast<int>(params.x - originX);
                const int by = static_cast<int>(params.y);
                const int bz = static_cast<int>(params.z - originZ);

                if (bx >= 0 && bx < WIDTH && by >= 0 && by < HEIGHT && bz >= 0 && bz < DEPTH) {
					// setBlock(bx, by, bz, BlockType::AIR);
					blocks.at(bx,by,bz) = BlockType::AIR;
                }
            }
        }
    }
}

void Chunk::generate(const TerrainGenerationParams& terrainParams) {
    
    Noise elevationNoise(terrainParams.seed);
    Noise erosionNoise(terrainParams.seed + 237);
    Noise weirdnessNoise(terrainParams.seed + 98789);

    Noise moistureNoise(9999);

	BlockStorage blocks;

    for (int x = 0; x < WIDTH; ++x) {
        for (int z = 0; z < DEPTH; ++z) {
            const float wx = float(originX + x);
            const float wz = float(originZ + z);

            const float frequency = 0.0005f;
            const int octaves = 8;
            const float lacunarity = 2.0f;
            const float persistence = 0.5f;

            // BIOME GEN
            float moisture = (moistureNoise.fractalBrownianMotion2D(wx * frequency,
                                                            wz * frequency,
                                                            octaves,
                                                            lacunarity,
                                                            persistence
            ) + 1.0f) * 0.5f;

            // TERRAIN GENERATION
            float continent = elevationNoise.fractalBrownianMotion2D(wx * frequency,
                                                            wz * frequency,
                                                            octaves,
                                                            lacunarity,
                                                            persistence
            );

            continent = glm::clamp(continent, -3.8f, 3.8f); // [-3.8, 3.8]


            float hills = elevationNoise.fractalBrownianMotion2D(wx * 0.005f,
                                                                wz * 0.005f,
                                                                5,
                                                                2.0f,
                                                                0.5f);

            hills = glm::clamp(hills, 0.0f, 1.0f); // [0,1]

            float erosion = erosionNoise.fractalBrownianMotion2D(wx * 0.01f,
                                                                wz * 0.01f,
                                                                5,
                                                                2.0f,
                                                                0.5f);

            erosion = glm::clamp(erosion, -1.0f, 1.0f);

            float weirdness = weirdnessNoise.fractalBrownianMotion2D(wx * 0.002f,
                                                                    wz * 0.002f,
                                                                    5,
                                                                    2.0f,
                                                                    0.5f);

            weirdness = glm::clamp(weirdness, -1.0f, 1.0f);

            float  PV = 1.0f - fabs(3.0f * fabs(weirdness) - 2.0f);

            float baseHeight = terrainParams.seaLevel;

            // --- Continentalness decides macro elevation ---
            if (continent > -1.2f && continent < -1.05f) { // Mushroom fields
                baseHeight = terrainParams.seaLevel + 80.0f;
            }
            else if (continent > -1.05f && continent < -0.455f) { // Deep ocean
                baseHeight = terrainParams.seaLevel - 30.0f;
            }
            else if (continent > -0.455f && continent < -0.19f) { // Ocean
                baseHeight = terrainParams.seaLevel - 10.0f;
            }
            else if (continent > -0.19f) { // Inland
                // coast / near / mid / far inland
                if (continent < -0.11f)      baseHeight = terrainParams.seaLevel + (continent * 5.0f);
                else if (continent < 0.03f)  baseHeight = terrainParams.seaLevel + (continent * 40.0f);
                else if (continent < 0.3f)   baseHeight = terrainParams.seaLevel + (continent * 60.0f);
                else                         baseHeight = terrainParams.seaLevel + (continent * 100.0f);

                // --- Apply erosion (flattens or exaggerates) ---
                // low erosion = mountainous, high erosion = flat
                float erosionFactor = glm::smoothstep(-1.0f, 1.0f, -erosion); 
                baseHeight += erosionFactor * 30.0f; // exaggerates mountains
                baseHeight -= (1.0f - erosionFactor) * 10.0f; // flattens lowland

                // --- Apply PV (rivers/peaks) ---
                if (PV > 0.7f) {
                    baseHeight += PV * 60.0f; // peaks
                } else if (PV < -0.85f) {
                    baseHeight -= 20.0f; // valleys/rivers
                }
            }



            int surfaceY = int(baseHeight);
            surfaceY = glm::clamp(surfaceY, 1, HEIGHT - 50);

            //============================

            // int surfaceY = int(continent * 30 + terrainParams.seaLevel);
            // surfaceY = glm::clamp(surfaceY, 1, HEIGHT - 20);
            // int surfaceY = continent * HEIGHT;



            // --- Biome selection (Minecraft-ish) ---
            BlockType top  = BlockType::GRASS;
            BlockType fill = BlockType::DIRT;


            // Bedrock-ish base
            for (int y = 0; y <= terrainParams.bedrockLevel; ++y)
                blocks.at(x, y, z) = BlockType::BEDROCK;

            // Deep stone
            for (int y = terrainParams.bedrockLevel + 1; y < surfaceY - 4; ++y)
                blocks.at(x, y, z) = BlockType::STONE;

            // Subsurface filler
            for (int y = std::max(terrainParams.bedrockLevel + 1, surfaceY - 4); y < surfaceY; ++y)
                blocks.at(x, y, z) = fill;

            

            if (surfaceY > terrainParams.seaLevel) {
                if (continent < 0.3f) {
                    if (moisture < 0.3f) {
                        std::cout << "Desert at " << x << ", " << surfaceY << ", " << z  << std::endl;
                        top = BlockType::SAND; // Desert
                        fill = BlockType::SAND;
                    } else if (moisture < 0.6f) {
                        top = BlockType::GRASS; // Plains
                        fill = BlockType::DIRT;
                    } else {
                        top = BlockType::GRASS; // swamp
                        fill = BlockType::DIRT;
                    }
                } else if (continent < 0.7f) { // midland
                    if (moisture < 0.3f) {
                        std::cout << "Savanna at " << x << ", " << surfaceY << ", " << z  << std::endl;
                        top = BlockType::SAND;  // savanna
                        fill = BlockType::DIRT;
                    } else if (moisture < 0.6f) {
                        // std::cout << "Forest at " << x << ", " << surfaceY << ", " << z  << std::endl;
                        top = BlockType::GRASS; // forest
                        fill = BlockType::DIRT;
                    } else {
                        // std::cout << "Jungle at " << x << ", " << surfaceY << ", " << z  << std::endl;
                        top = BlockType::GRASS; // jungle
                        fill = BlockType::DIRT;
                    }
                } else { // highland
                    if (surfaceY > terrainParams.seaLevel + 40) {
                        std::cout << "Snowy Peaks at " << x << ", " << surfaceY << ", " << z  << std::endl;
                        top = BlockType::SNOW;  // snowy peaks
                        fill = BlockType::STONE;
                    } else {
                        std::cout << "Mountain Grassland at " << x << ", " << surfaceY << ", " << z  << std::endl;
                        top = BlockType::GRASS; // mountain grassland
                        fill = BlockType::STONE;
                    }
                }
            }

            // Surface/top
            blocks.at(x, surfaceY, z) =
                (surfaceY <= terrainParams.seaLevel ? BlockType::SAND : top);
            // Water up to sea level
            for (int y = surfaceY + 1; y <= terrainParams.seaLevel && y < HEIGHT; ++y)
                blocks.at(x, y, z) = BlockType::WATER;

            // Air above
            for (int y = std::max(terrainParams.seaLevel + 1, surfaceY + 1); y < HEIGHT; ++y)
                blocks.at(x, y, z) = BlockType::AIR;
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

                carveWorm(worm, blocks);
            }
        }
    }

	blockIndices.encodeAll(blocks.getData(), palette, paletteMap);
}

BlockType Chunk::selectBlockType(int y, int surfaceHeight, float blend,
                                 const std::vector<BiomeParams>& biomes,
                                 const std::vector<float>& weights,
                                 const std::vector<float>& heights) {
    if (y < surfaceHeight - 5)
        return BlockType::STONE;
    if (y < surfaceHeight)
        return BlockType::DIRT;
    if (y > surfaceHeight)
        return BlockType::AIR;

    // Compute contribution = weight * actual height
    float maxContribution = -1.0f;
    BlockType topBlock = BlockType::DIRT;

    for (size_t i = 0; i < biomes.size(); ++i) {
        float contribution = weights[i] * heights[i];
        if (contribution > maxContribution) {
            maxContribution = contribution;
            topBlock = biomes[i].topBlock;
        }
    }

    return topBlock;
}


BlockType Chunk::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH) {
        return BlockType::AIR; // Out of bounds returns air
    }

    int index = x + WIDTH * (y + HEIGHT * z);
    uint32_t paletteIndex = blockIndices.get(index);
    return palette[paletteIndex];
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH) {
        return; // Out of bounds, do nothing
    }

    int index = x + WIDTH * (y + HEIGHT * z);
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

	buildMesh();

	//update possible neighbour
	if (x == 0) {
		if (auto westChunk = adjacentChunks[WEST].lock()) {
			westChunk->buildMesh();
		}
	}
	if (x == WIDTH - 1) {
		if (auto eastChunk = adjacentChunks[EAST].lock()) {
			eastChunk->buildMesh();
		}
	}
	if (z == 0) {
		if (auto southChunk = adjacentChunks[SOUTH].lock()) {
			southChunk->buildMesh();
		}
	}
	if (z == DEPTH - 1) {
		if (auto northChunk = adjacentChunks[NORTH].lock()) {
			northChunk->buildMesh();
		}
	}
}

bool Chunk::isBlockVisible(glm::ivec3 pos) {
    int x = pos.x, y = pos.y, z = pos.z;
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH)
        return false;

    int idx = x + WIDTH * (y + HEIGHT * z);
    if (getBlock(x,y,z) == BlockType::AIR)
        return false;

    auto getBlockOrNeighbor = [&](int dx, int dy, int dz, Direction dir) -> BlockType {
        if (x + dx < 0 || x + dx >= WIDTH ||
            z + dz < 0 || z + dz >= DEPTH) 
        {
            auto neighbor = adjacentChunks[dir].lock();
            if (!neighbor) return BlockType::AIR;
            int nx = (dx == -1 ? WIDTH - 1 : (dx == 1 ? 0 : x));
            int nz = (dz == -1 ? DEPTH - 1 : (dz == 1 ? 0 : z));
            return neighbor->getBlock(nx, y + dy, nz);
        }
        if (y + dy < 0 || y + dy >= HEIGHT)
            return BlockType::AIR;
		return getBlock(x + dx, y + dy, z + dz);
    };

    return getBlockOrNeighbor(0, 0, +1, NORTH) == BlockType::AIR ||
           getBlockOrNeighbor(0, 0, -1, SOUTH) == BlockType::AIR ||
           y == HEIGHT - 1 || getBlockOrNeighbor(0, +1, 0, NONE) == BlockType::AIR ||
           y == 0 || getBlockOrNeighbor(0, -1, 0, NONE) == BlockType::AIR ||
           getBlockOrNeighbor(+1, 0, 0, EAST) == BlockType::AIR ||
           getBlockOrNeighbor(-1, 0, 0, WEST) == BlockType::AIR;
}


void Chunk::buildMesh() {
	buildMeshData();
	uploadMesh();
}

void Chunk::buildMeshData() {
	meshVertices.clear();
	std::vector<BlockType> blockTypeVector;	// unpacked block indices

    // Decode palette indices to block types
    blockTypeVector.resize(WIDTH * HEIGHT * DEPTH);
    std::vector<uint32_t> decodedIndices;
    blockIndices.decodeAll(decodedIndices);

    for (size_t i = 0; i < blockTypeVector.size(); ++i) {
        uint32_t paletteIndex = decodedIndices[i];
        blockTypeVector[i] = palette[paletteIndex];
    }

    auto getBlockOrNeighbor = [&](int x, int y, int z, int dx, int dy, int dz, Direction dir) -> BlockType {
        if (x + dx < 0 || x + dx >= WIDTH ||
            z + dz < 0 || z + dz >= DEPTH) 
        {
            auto neighbor = adjacentChunks[dir].lock();
            if (!neighbor) return BlockType::AIR;
            int nx = (dx == -1 ? WIDTH - 1 : (dx == 1 ? 0 : x));
            int nz = (dz == -1 ? DEPTH - 1 : (dz == 1 ? 0 : z));
            return neighbor->getBlock(nx, y + dy, nz);
        }
        if (y + dy < 0 || y + dy >= HEIGHT) 
            return BlockType::AIR; // top or bottom world edge inside chunk
        return blockTypeVector[(x + dx) + WIDTH * ((y + dy) + HEIGHT * (z + dz))];
    };

    for (int x = 0; x < WIDTH; ++x) {
        for (int y = 0; y < HEIGHT; ++y) {
            for (int z = 0; z < DEPTH; ++z) {
                int idx = x + WIDTH * (y + HEIGHT * z);
                if (blockTypeVector[idx] == BlockType::AIR) continue;

                // FRONT (+Z)
                if (getBlockOrNeighbor(x, y, z, 0, 0, +1, NORTH) == BlockType::AIR)
                    addFace(x, y, z, 0);

                // BACK (-Z)
                if (getBlockOrNeighbor(x, y, z, 0, 0, -1, SOUTH) == BlockType::AIR)
                    addFace(x, y, z, 1);

                // TOP (+Y) – no vertical neighbor chunks
                if (y == HEIGHT - 1 || getBlockOrNeighbor(x, y, z, 0, +1, 0, NONE) == BlockType::AIR)
                    addFace(x, y, z, 2);

                // BOTTOM (-Y)
                if (y == 0 || getBlockOrNeighbor(x, y, z, 0, -1, 0, NONE) == BlockType::AIR)
                    addFace(x, y, z, 3);

                // RIGHT (+X)
                if (getBlockOrNeighbor(x, y, z, +1, 0, 0, EAST) == BlockType::AIR)
                    addFace(x, y, z, 4);

                // LEFT (-X)
                if (getBlockOrNeighbor(x, y, z, -1, 0, 0, WEST) == BlockType::AIR)
                    addFace(x, y, z, 5);
            }
        }
    }
}

void Chunk::uploadMesh() {
    // Upload mesh to OpenGL (unchanged)
    if (VAO == 0)
        glGenVertexArrays(1, &VAO);
    if (VBO == 0)
        glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(float), meshVertices.data(), GL_STATIC_DRAW);

    GLsizei stride = 9 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, static_cast<void *>(nullptr));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    
    meshVerticesSize = meshVertices.size();
    meshVertices.clear();
    meshVertices.shrink_to_fit();
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
    const BlockType type = getBlock(x, y, z);

    // Determine UV offset in atlas based on block type and face
    glm::vec2 tileCoord = getTextureOffset(type, face);
    glm::vec2 offset = { tileCoord.x * TILE_W, tileCoord.y * TILE_H };

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
        meshVertices.push_back(normal.x);
        meshVertices.push_back(normal.y);
        meshVertices.push_back(normal.z);
    }
}

void Chunk::draw(const std::shared_ptr<Shader>& shader) const {
    shader->use();
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, meshVerticesSize / 9);
}

void Chunk::saveToStream(std::ostream& out) const {
    // Write chunk key for O(1) lookup later
    out.write(reinterpret_cast<const char*>(&originX), sizeof(originX));
    out.write(reinterpret_cast<const char*>(&originZ), sizeof(originZ));

    // --- Save palette ---
    uint32_t paletteSize = static_cast<uint32_t>(palette.size());
    out.write(reinterpret_cast<const char*>(&paletteSize), sizeof(paletteSize));

    for (const auto& block : palette) {
        out.write(reinterpret_cast<const char*>(&block), sizeof(block));
    }

	// Save block data
    blockIndices.saveToStream(out);
}

void Chunk::loadFromStream(std::istream& in) {
    // Read chunk key
    in.read(reinterpret_cast<char*>(&originX), sizeof(originX));
    in.read(reinterpret_cast<char*>(&originZ), sizeof(originZ));

    // --- Load palette ---
    uint32_t paletteSize;
    in.read(reinterpret_cast<char*>(&paletteSize), sizeof(paletteSize));

    palette.resize(paletteSize);
    paletteMap.clear();

    for (uint32_t i = 0; i < paletteSize; ++i) {
        in.read(reinterpret_cast<char*>(&palette[i]), sizeof(BlockType));
        paletteMap[palette[i]] = i; // rebuild map
    }

	// Load block data
    blockIndices.loadFromStream(in);
}
