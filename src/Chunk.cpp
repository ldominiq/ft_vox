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

static float fbm(FastNoiseLite& n, float x, float z, int oct, float lac, float gain) {
    float a = 1.0f, f = 1.0f, sum = 0.0f, norm = 0.0f;
    for (int i = 0; i < oct; ++i) {
        sum  += n.GetNoise(x * f, z * f) * a;
        norm += a;
        a    *= gain;
        f    *= lac;
    }
    return (norm > 0.0f) ? (sum / norm) : 0.0f; // ~[-1,1]
}

static float ridged(FastNoiseLite& n, float x, float z) {
    float h = 1.0f - std::abs(n.GetNoise(x, z)); // [0,1]
    return h * h * h; // sharper ridges
}


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
    // Noise setup
    FastNoiseLite nCont, nWarpA, nWarpB, nHills, nRidged, nTemp, nMoist;

    FastNoiseLite nMtnMask, nColdCell;

    nMtnMask.SetSeed(terrainParams.seed + 40);
    nMtnMask.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    nMtnMask.SetFrequency(terrainParams.mtnMaskFreq);

    nColdCell.SetSeed(terrainParams.seed + 41);
    nColdCell.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    nColdCell.SetFrequency(terrainParams.coldCellFreq);


    nCont.SetSeed(terrainParams.seed + 11);
    nCont.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    nCont.SetFrequency(terrainParams.continentFreq);

    nWarpA.SetSeed(terrainParams.seed + 12);
    nWarpA.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    nWarpA.SetFrequency(terrainParams.warpFreq);

    nWarpB.SetSeed(terrainParams.seed + 13);
    nWarpB.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    nWarpB.SetFrequency(terrainParams.warpFreq);

    nHills.SetSeed(terrainParams.seed + 21);
    nHills.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    nHills.SetFrequency(terrainParams.hillsFreq);

    nRidged.SetSeed(terrainParams.seed + 22);
    nRidged.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    nRidged.SetFrequency(terrainParams.ridgedFreq);

    nTemp.SetSeed(terrainParams.seed + 31);
    nTemp.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    nTemp.SetFrequency(terrainParams.tempFreq);

    nMoist.SetSeed(terrainParams.seed + 32);
    nMoist.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    nMoist.SetFrequency(terrainParams.moistFreq);

	BlockStorage blocks;

    for (int x = 0; x < WIDTH; ++x) {
        for (int z = 0; z < DEPTH; ++z) {
            const float wx = float(originX + x);
            const float wz = float(originZ + z);

            // --- Domain warp to make interesting coasts ---
            const float qx = wx + terrainParams.warpAmp * nWarpA.GetNoise(wx, wz);
            const float qz = wz + terrainParams.warpAmp * nWarpB.GetNoise(wx + 1000.0f, wz - 1000.0f);

            // --- Continentalness and base uplift ---
            float c = 0.5f * (nCont.GetNoise(qx, qz) + 1.0f);
            c = glm::smoothstep(0.33f, 0.58f, c);             // broader land band
            float baseShift = glm::mix(-22.0f, 44.0f, c);     // lift inland a bit

            // --- Determine mountain regions (mask) ---
            float mMaskRaw  = 0.5f * (nMtnMask.GetNoise(qx + 777.0f, qz - 333.0f) + 1.0f);
            float mMask     = glm::smoothstep(terrainParams.mtnMaskThreshold - terrainParams.mtnMaskSharpness,
                                              terrainParams.mtnMaskThreshold + terrainParams.mtnMaskSharpness,
                                              mMaskRaw);
            // mMask ~0 => plains region; ~1 => mountain region

            // --- Terrain details ---
            float hills = fbm(nHills, qx, qz, 4, 2.0f, 0.5f); // ~[-1,1]
            float mtn   = ridged(nRidged, qx, qz);            // [0,1]

            // Flatter outside mountain regions; real relief only where mMask≈1
            float elevation =
                baseShift
              + (1.0f - mMask) * (terrainParams.plainsHillsAmp * hills)   // gentle undulation in plains
              + mMask * (terrainParams.mtnBase + terrainParams.mtnAmp * mtn);         // mountains only in mountain regions

            int surfaceY = int(std::round(float(terrainParams.seaLevel) + elevation));
            surfaceY = glm::clamp(surfaceY, 1, HEIGHT - 2);



            // --- Climate (temperature & moisture) ---
            float baseTemp = 0.5f * (nTemp.GetNoise(wx, wz) + 1.0f);   // 0..1
            float moist    = 0.5f * (nMoist.GetNoise(wx, wz) + 1.0f);  // 0..1

            // Large cold regions to allow snowy plains at LOW altitude
            float coldCell = 0.5f * (nColdCell.GetNoise(wx - 2000.0f, wz + 500.0f) + 1.0f); // 0..1
            baseTemp = glm::clamp(baseTemp - terrainParams.coldCellStrength * coldCell, 0.0f, 1.0f);

            // Modest latitude (optional)
            if (terrainParams.latitudeAmp > 0.0f) {
                float lat = glm::clamp(0.5f - (wz / 80000.0f), 0.0f, 1.0f);
                baseTemp = glm::clamp((1.0f - terrainParams.latitudeAmp) * baseTemp + terrainParams.latitudeAmp * lat, 0.0f, 1.0f);
            }

            // Small lapse rate with height, but not too strong (snow mainly via snowLine)
            float aboveSea = float(surfaceY - terrainParams.seaLevel);
            float lapse    = glm::clamp(aboveSea / 120.0f, 0.0f, 1.0f);
            float temp     = glm::clamp(baseTemp - 0.20f * lapse, 0.0f, 1.0f);

            // Nudge moisture drier inland
            float inlandness = c;
            moist = glm::clamp(moist * glm::mix(0.90f, 1.05f, inlandness), 0.0f, 1.0f);

            // --- Biome selection (Minecraft-ish) ---
            BlockType top  = BlockType::GRASS;
            BlockType fill = BlockType::DIRT;

            // Beaches
            if (std::abs(surfaceY - terrainParams.seaLevel) <= int(terrainParams.beachWidth)) {
                top = fill = BlockType::SAND;
            }
            // Snow by elevation (mountaintops)
            else if (surfaceY >= terrainParams.snowLine) {
                top  = BlockType::SNOW;
                fill = BlockType::STONE;
            }
            // **Snowy plains**: cold cell + low elevation (no mountains)
            else if (temp < 0.22f && surfaceY < terrainParams.snowLine - 8 && mMask < 0.35f) {
                top  = BlockType::SNOW;   // snowy ground
                fill = BlockType::DIRT;   // dirt under snow (not stone)
            }
            // Desert: warm+dry, low-ish, somewhat inland, and NOT in mountain zones
            else if (temp > 0.60f && moist < 0.38f && surfaceY < 120 && inlandness > 0.45f && mMask < 0.5f) {
                top = fill = BlockType::SAND;
            }
            // Forest (wetter)
            else if (moist >= 0.60f && temp > 0.28f) {
                top  = BlockType::GRASS;
                fill = BlockType::DIRT;
            }
            // Plains (moderate)
            else if (temp > 0.42f && moist >= 0.35f && moist < 0.60f) {
                top  = BlockType::GRASS;
                fill = BlockType::DIRT;
            }
            // Cool plains / taiga ground (no full snow cover)
            else if (temp <= 0.42f) {
                top  = BlockType::GRASS;
                fill = BlockType::DIRT;
            }
            // Fallback
            else {
                top  = BlockType::GRASS;
                fill = BlockType::DIRT;
            }

            // Bedrock-ish base
            for (int y = 0; y <= terrainParams.bedrockLevel; ++y)
                blocks.at(x, y, z) = BlockType::BEDROCK;

            // Deep stone
            for (int y = terrainParams.bedrockLevel + 1; y < surfaceY - 4; ++y)
                blocks.at(x, y, z) = BlockType::STONE;

            // Subsurface filler
            for (int y = std::max(terrainParams.bedrockLevel + 1, surfaceY - 4); y < surfaceY; ++y)
                blocks.at(x, y, z) = fill;

            // Surface/top
            blocks.at(x, surfaceY, z) =
                (surfaceY <= terrainParams.seaLevel ? (top == BlockType::SAND ? BlockType::SAND : BlockType::DIRT) : top);

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
