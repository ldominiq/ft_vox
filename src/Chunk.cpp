#include "Chunk.hpp"
#include "World.hpp"
#include <fstream>

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

float Chunk::computeColumnHeight(const TerrainGenerationParams& params,
                                 Noise& baseNoise, Noise& detailNoise, Noise& warpNoise,
                                 Noise& erosionNoise, Noise& weirdnessNoise, Noise& riverNoise,
                                 float worldX, float worldZ)
{
    const float freqContinent = 0.0005f;
    const int octaves = 8;
    const float lacunarity = 2.0f;
    const float persistence = 0.5f;

    float continent = baseNoise.fractalBrownianMotion2D(worldX * freqContinent, worldZ * freqContinent, octaves, lacunarity, persistence);
    continent = glm::clamp(continent, -3.8f, 3.8f);

    // small rolling hills noise (used later for cliffs/mountain mask)
    float hills = baseNoise.fractalBrownianMotion2D(worldX * 0.005f, worldZ * 0.005f, 5, 2.0f, 0.5f);
    hills = glm::clamp(hills, 0.0f, 1.0f);

    float erosion = erosionNoise.fractalBrownianMotion2D(worldX * 0.01f, worldZ * 0.01f, 5, 2.0f, 0.5f);
    erosion = glm::clamp(erosion, -1.0f, 1.0f);

    float weirdness = weirdnessNoise.fractalBrownianMotion2D(worldX * 0.002f, worldZ * 0.002f, 5, 2.0f, 0.5f);
    weirdness = glm::clamp(weirdness, -1.0f, 1.0f);
    float PV = 1.0f - fabs(3.0f * fabs(weirdness) - 2.0f);

    float warpX = worldX + warpNoise.getNoise(worldX, worldZ) * 20.0f;
    float warpZ = worldZ + warpNoise.getNoise(worldZ, worldX) * 20.0f;

    // --- make plains default: small base land elevation, mountains only build inland ---
    float baseH = static_cast<float>(params.seaLevel);

    // map continent to inlandFactor in [0,1]
    float inlandFactor = glm::smoothstep(-0.455f, 0.5f, continent);

    // plains baseline: gentle land rise inland (controls how flat the bulk of land is)
    const float plainsBaseline = 10.0f; // tweakable: low -> more flat plains
    baseH = params.seaLevel + inlandFactor * plainsBaseline;

    // use erosion/weirdness to vary mountain probability/amplitude
    float erosionFactor = glm::smoothstep(-1.0f, 1.0f, -erosion);

    // mountainMask: only significant when inlandFactor is reasonably high
    float mountainMask = glm::smoothstep(0.45f, 0.85f, inlandFactor); // grows from 0 -> 1 inland
    mountainMask *= params.mountainBoost;

    // PV (peaks/valleys) only matters if mountainMask is present
    if (mountainMask > 0.15f) {
        baseH += PV * 40.0f * params.PVBoost * mountainMask; // pockets of peaks
    }

    // hills contribute but scaled by mountainMask to avoid producing tall hills on near-coast plains
    float hillsTerm = hills * 20.0f * mountainMask;

    // detail layers (small scale) - always present but smaller amplitude
    float detailTerm = detailNoise.fractalBrownianMotion2D(warpX * 0.03f, warpZ * 0.03f, 3, 2.0f, 0.5f) * 6.0f * glm::mix(0.6f, 1.0f, mountainMask);
    float fineDetail = detailNoise.fractalBrownianMotion2D(warpX * 0.1f, warpZ * 0.1f, 2, 2.0f, 0.5f) * 2.0f;

    float finalH = baseH + hillsTerm + detailTerm + fineDetail;

    // rivers: make rarer and less flattening by default (tweak via params)
    float r = fabs(riverNoise.getNoise(worldX * 0.002f, worldZ * 0.002f));
    if (r < params.riverThreshold) {
        // reduce mixing amount so rivers carve less aggressively
        float mixAmount = glm::smoothstep(params.riverThreshold, 0.0f, r) * params.riverStrength * 0.6f;
        finalH = glm::mix(finalH, (float)params.seaLevel - 2.0f, glm::clamp(mixAmount, 0.0f, 1.0f));
    }

    // clamp to sensible world bounds
    finalH = glm::clamp(finalH, 1.0f, float(Chunk::HEIGHT - 20));
    return finalH;
}

void Chunk::generate(const TerrainGenerationParams& terrainParams) {
    // Noise instances seeded from world seed (different offsets for independent layers)
    Noise baseNoise(terrainParams.seed + 1);
    Noise detailNoise(terrainParams.seed + 2);
    Noise warpNoise(terrainParams.seed + 3);
    Noise erosionNoise(terrainParams.seed + 237);
    Noise weirdnessNoise(terrainParams.seed + 98789);
    Noise moistureNoise(terrainParams.seed + 54321);
    Noise riverNoise(terrainParams.seed + 99999);

    // local storage
    BlockStorage blocks;
    std::vector<float> heightmap(WIDTH * DEPTH, static_cast<float>(terrainParams.seaLevel));
    std::vector<float> moistureMap(WIDTH * DEPTH, 0.0f);
    std::vector<float> steepness(WIDTH * DEPTH, 0.0f);

    // store helper maps so second pass can make smarter decisions
    std::vector<float> hillsMap(WIDTH * DEPTH, 0.0f);
    std::vector<float> continentMap(WIDTH * DEPTH, 0.0f);

    for (int x = 0; x < WIDTH; ++x) {
        for (int z = 0; z < DEPTH; ++z) {
            const int ix = x + WIDTH * z;
            const float wx = float(originX + x);
            const float wz = float(originZ + z);

            

            // moisture (for biome choice)
            float moisture = (moistureNoise.fractalBrownianMotion2D(wx * 0.005f, wz * 0.005f,
                                                                    8, 2.0f, 0.5f) + 1.0f) * 0.5f;
            moistureMap[ix] = moisture;

            float finalH = computeColumnHeight(terrainParams, baseNoise, detailNoise, warpNoise, erosionNoise, weirdnessNoise, riverNoise, wx, wz);

            heightmap[ix] = finalH;
        }
    }

    // NEW: shore smoothing -- compute distance from water and gently ramp nearby heights toward sea
    {
        const int W = WIDTH, D = DEPTH;
        const int R = terrainParams.shoreSmoothRadius;
        if (R > 0) {
            const int N = W * D;
            std::vector<int> dist(N, INT_MAX);
            std::deque<int> q;
            // initialize queue with water cells (<= seaLevel)
            for (int iz = 0; iz < D; ++iz) {
                for (int ix2 = 0; ix2 < W; ++ix2) {
                    int index = ix2 + W * iz;
                    if (heightmap[index] <= terrainParams.seaLevel) {
                        dist[index] = 0;
                        q.push_back(index);
                    }
                }
            }
            // BFS 4-neighbor distance
            while (!q.empty()) {
                int cur = q.front(); q.pop_front();
                int cx = cur % W;
                int cz = cur / W;
                const int d4x[4] = {1,-1,0,0};
                const int d4z[4] = {0,0,1,-1};
                for (int k = 0; k < 4; ++k) {
                    int nx = cx + d4x[k];
                    int nz = cz + d4z[k];
                    if (nx < 0 || nx >= W || nz < 0 || nz >= D) continue;
                    int nidx = nx + W * nz;
                    if (dist[nidx] > dist[cur] + 1) {
                        dist[nidx] = dist[cur] + 1;
                        if (dist[nidx] < R) q.push_back(nidx); // only propagate until radius
                    }
                }
            }
            // apply smoothing toward a ramp target based on distance
            for (int z = 0; z < D; ++z) {
                for (int x = 0; x < W; ++x) {
                    int i = x + W * z;
                    int d = dist[i];
                    if (d > 0 && d <= R) {
                        float t = 1.0f - (float(d) / float(R)); // 1 at coast, 0 at radius edge
                        // target: seaLevel + distance * shoreSlopeFactor (gentle ramp)
                        float target = float(terrainParams.seaLevel) + float(d) * terrainParams.shoreSlopeFactor;
                        // mix current height toward target; shoreSmoothStrength controls strength
                        float mixAmt = t * terrainParams.shoreSmoothStrength;
                        heightmap[i] = glm::mix(heightmap[i], target, glm::clamp(mixAmt, 0.0f, 1.0f));
                    }
                }
            }
        }
    }

    // optional single-pass smoothing to remove tiny spikes (keep light)
    std::vector<float> tmp(heightmap);
    for (int x = 1; x < WIDTH - 1; ++x) {
        for (int z = 1; z < DEPTH - 1; ++z) {
            int ix = x + WIDTH * z;
            float sum = 0.0f;
            for (int ox = -1; ox <= 1; ++ox)
                for (int oz = -1; oz <= 1; ++oz)
                    sum += heightmap[(x + ox) + WIDTH * (z + oz)];
            // blend original + small smoothing to preserve peaks
            tmp[ix] = glm::mix(heightmap[ix], sum / 9.0f, 0.35f); // weaker smoothing
        }
    }
    heightmap.swap(tmp);

    // second pass: fill blocks from heightmap (deterministic, fast)
    float cliffSlopeThreshold = currentParams.cliffSlopeThreshold;
    const int minCliffElevation = currentParams.minCliffElevation;
    const float hillsThresholdForCliff = 0.55f; // require reasonably hilly area to allow cliffs

    for (int x = 0; x < WIDTH; ++x) {
        for (int z = 0; z < DEPTH; ++z) {
            int ix = x + WIDTH * z;
            int surfaceY = static_cast<int>(glm::round(heightmap[ix]));
            surfaceY = glm::clamp(surfaceY, 1, HEIGHT - 20);

            // compute local slope by central differences (handle edges)
            float hL = (x > 0) ? heightmap[(x-1) + WIDTH * z] : heightmap[ix];
            float hR = (x < WIDTH-1) ? heightmap[(x+1) + WIDTH * z] : heightmap[ix];
            float hD = (z > 0) ? heightmap[x + WIDTH * (z-1)] : heightmap[ix];
            float hU = (z < DEPTH-1) ? heightmap[x + WIDTH * (z+1)] : heightmap[ix];

            float dhdx = (hR - hL) * 0.5f;
            float dhdz = (hU - hD) * 0.5f;
            float slope = glm::length(glm::vec2(dhdx, dhdz)); // higher = steeper

            float moisture = moistureMap[ix];

            // decide top/fill, but override if cliff
            // Mark cliff only when:
            // - slope is high
            // - column is well above sea level (minCliffElevation)
            // - area has hills and is inland (avoid making shoreline cliffs)
            bool isCliff = (slope > currentParams.cliffSlopeThreshold)
               && (surfaceY > currentParams.seaLevel + currentParams.minCliffElevation)
               && (hillsMap[ix] > 0.55f)           // sufficiently hilly
               && (continentMap[ix] > -0.1f);     // reasonably inland (tweakable)
            BlockType top = BlockType::GRASS;
            BlockType fill = BlockType::DIRT;

            if (surfaceY <= terrainParams.seaLevel) {
                top = BlockType::SAND;
                fill = BlockType::SAND;
            } else if (isCliff) {
                // steep slopes -> rock
                top = BlockType::STONE;
                fill = BlockType::STONE;
            } else {
                if (moisture < 0.25f) { top = BlockType::SAND; fill = BlockType::SAND; } // dry
                else if (surfaceY > terrainParams.seaLevel + 60) { top = BlockType::SNOW; fill = BlockType::STONE; } // high
                else { top = BlockType::GRASS; fill = BlockType::DIRT; }
            }

            // Bedrock base
            for (int y = 0; y <= terrainParams.bedrockLevel; ++y)
                blocks.at(x, y, z) = BlockType::BEDROCK;

            // Fill below surface (stone deeper)
            for (int y = terrainParams.bedrockLevel + 1; y < surfaceY - 4; ++y)
                blocks.at(x, y, z) = BlockType::STONE;

            // near-surface fill
            for (int y = std::max(terrainParams.bedrockLevel + 1, surfaceY - 4); y < surfaceY; ++y)
                blocks.at(x, y, z) = fill;

            // Surface/top
            blocks.at(x, surfaceY, z) = top;

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

    // encode palette and block data (same as before)
    blockIndices.encodeAll(blocks.getData(), palette, paletteMap);
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
