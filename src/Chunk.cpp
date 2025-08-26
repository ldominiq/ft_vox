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

        case BlockType::LOG:
            col = 8; row = 0;
            break;
        case BlockType::LEAVES:
            col = 9; row = 0;
            break;

        default:
            col = 0; row = 0;
            break;
    }

    return glm::vec2(col, row);
}

// Save a simple RGB PPM where each pixel is an sRGB color representing the biome
static void saveBiomePPM(const std::string &path, const std::vector<glm::u8vec3> &img, int w, int h) {
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << w << " " << h << "\n255\n";
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            const glm::u8vec3 &c = img[i + j * w];
            f.put(static_cast<char>(c.r));
            f.put(static_cast<char>(c.g));
            f.put(static_cast<char>(c.b));
        }
    }
    f.close();
}

Chunk::Chunk(const int chunkX, const int chunkZ, const TerrainGenerationParams& params, const bool doGenerate)
    : originX(chunkX * WIDTH), originZ(chunkZ * DEPTH),
      blockIndices(WIDTH * HEIGHT * DEPTH, /*bitsPerEntry=*/4), currentParams(params)  // or more, depending on palette size. We could even use 3 as we use less than 8 types of blocks
{
	if (doGenerate)
    	generate(params);
	else preGenerated = true;
    	
}

Chunk::Chunk() : sourceChunkX(0), sourceChunkZ(0), originX(0), originZ(0),
		  VAO(0), VBO(0), meshVerticesSize(0),
		  blockIndices(WIDTH * HEIGHT * DEPTH, 0)
{
    adjacentChunks[0].reset();
    adjacentChunks[1].reset();
    adjacentChunks[2].reset();
    adjacentChunks[3].reset();
}

Chunk::~Chunk() {
    if (glfwGetCurrentContext()) {
        if (VAO) {
            glDeleteVertexArrays(1, &VAO);
            VAO = 0;
        }
        if (VBO) {
            glDeleteBuffers(1, &VBO);
            VBO = 0;
        }
    } else {
        VAO = 0;
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


// Linear interpolation between spline points
float Chunk::interpolateSpline(float noise, const std::vector<std::pair<float, float>>& spline) {
    const auto& pts = spline;
    if (noise <= pts.front().first) return pts.front().second;
    if (noise >= pts.back().first) return pts.back().second;

    // Find the interval
    for (size_t i = 1; i < pts.size(); ++i) {
        if (noise < pts[i].first) {
            float t = (noise - pts[i-1].first) / (pts[i].first - pts[i-1].first);
            return pts[i-1].second + t * (pts[i].second - pts[i-1].second);
        }
    }
    return pts.back().second; // fallback
}

float Chunk::getContinentalness(const TerrainGenerationParams& terrainParams, float wx, float wz) {
    Noise baseNoise(terrainParams.seed);

    float fbm = baseNoise.fractalBrownianMotion2D(
        wx * terrainParams.continentalnessFrequency,
        wz * terrainParams.continentalnessFrequency,
        terrainParams.continentalnessOctaves,
        terrainParams.continentalnessLacunarity,
        terrainParams.continentalnessPersistence
        );

    fbm *= terrainParams.continentalnessScalingFactor;
    
    float continentalness = glm::clamp(fbm, -3.8f, 3.8f);

    return continentalness;
}

float Chunk::getErosion(const TerrainGenerationParams& terrainParams, float wx, float wz) {
    Noise erosionNoise(terrainParams.seed + 237);

    float erosion = erosionNoise.fractalBrownianMotion2D(
        wx * terrainParams.erosionFrequency,
        wz * terrainParams.erosionFrequency,
        terrainParams.erosionOctaves,
        terrainParams.erosionLacunarity,
        terrainParams.erosionPersistence
        );

    erosion = glm::clamp(erosion * terrainParams.erosionScalingFactor, -1.0f, 1.0f);
    return erosion;
}

float Chunk::getPV(const TerrainGenerationParams& terrainParams, float wx, float wz) {
    Noise peakValleyNoise(terrainParams.seed + 98789);

    float peakValley = peakValleyNoise.fractalBrownianMotion2D(
        wx * terrainParams.peakValleyFrequency,
        wz * terrainParams.peakValleyFrequency,
        terrainParams.peakValleyOctaves,
        terrainParams.peakValleyLacunarity,
        terrainParams.peakValleyPersistence
        );

    peakValley = glm::clamp(peakValley * terrainParams.peakValleyScalingFactor, -1.0f, 1.0f);

    return peakValley;
}

float Chunk::getTemperature(const TerrainGenerationParams& terrainParams, float wx, float wz) {
    Noise tempNoise(terrainParams.seed + 123);

    float temperature = tempNoise.fractalBrownianMotion2D(
        wx * terrainParams.temperatureFrequency,
        wz * terrainParams.temperatureFrequency,
        terrainParams.temperatureOctaves,
        terrainParams.temperatureLacunarity,
        terrainParams.temperaturePersistence
        ) * terrainParams.temperatureScalingFactor;

    return temperature;
}

float Chunk::getHumidity(const TerrainGenerationParams& terrainParams, float wx, float wz) {
    Noise humidNoise(terrainParams.seed + 456);

    float humidity = humidNoise.fractalBrownianMotion2D(
        wx * terrainParams.humidityFrequency,
        wz * terrainParams.humidityFrequency,
        terrainParams.humidityOctaves,
        terrainParams.humidityLacunarity,
        terrainParams.humidityPersistence
        ) * terrainParams.humidityScalingFactor;

    return humidity;
}

float Chunk::surfaceNoiseTransformation(float noise, int splineIndex) {
    float noiseTransform = 0.0f;
    if (splineIndex == 1)
        noiseTransform = interpolateSpline(noise, continentalnessSpline);
    else if (splineIndex == 2)
        noiseTransform = interpolateSpline(noise, erosionSpline);
    else if (splineIndex == 3)
        noiseTransform = interpolateSpline(noise, peakValleySpline);

    return noiseTransform;
}

BiomeType Chunk::computeBiome(const TerrainGenerationParams& terrainParams, float worldX, float worldZ, int height) {

    // Build very low-frequency (coarse) climate fields so biomes form large contiguous regions.
    // biomeScaleChunks controls how many chunks make up a biome patch; use an extra multiplier to ensure broad bands.
    if (height <= terrainParams.seaLevel) return BiomeType::OCEAN;

    Noise tempNoise(terrainParams.seed + 45);
    Noise humidNoise(terrainParams.seed + 964);

    const float chunks = glm::max(1, terrainParams.biomeScaleChunks);
    const float worldUnitsPerPatch = chunks * Chunk::WIDTH * 8.0f;
    const float freqCoarse = 1.0f / glm::max(256.0f, worldUnitsPerPatch);

    // Coarse climate fields in [0..1]
    float tempCoarse  = (tempNoise.fractalBrownianMotion2D(worldX * freqCoarse,            worldZ * freqCoarse,            4, 2.0f, 0.5f) + 1.0f) * 0.5f;
    float moistCoarse = (humidNoise.fractalBrownianMotion2D(worldX * freqCoarse * 0.9f,    worldZ * freqCoarse * 0.9f,    4, 2.0f, 0.5f) + 1.0f) * 0.5f;

    // Small regional bias
    Noise regionBias(terrainParams.seed + 4242);
    float bias = (regionBias.fractalBrownianMotion2D(worldX * freqCoarse * 0.6f, worldZ * freqCoarse * 0.6f, 3, 2.0f, 0.5f) + 1.0f) * 0.5f;

    float climate = glm::clamp(glm::mix(tempCoarse, 1.0f - moistCoarse, 0.35f) * 0.7f + bias * 0.3f, 0.0f, 1.0f);


    climate = glm::clamp((climate - 0.5f) * 1.2f + 0.5f, 0.0f, 1.0f);

    // High, cold overrides
    // if (height > terrainParams.seaLevel + 28) {
        if (tempCoarse < terrainParams.snowTemperatureThreshold) return BiomeType::TUNDRA;
        // return BiomeType::MOUNTAIN;
    // }

    // --- DESERT: hot + dry, inland, mid elevations ---
    float aridity = (1.0f - moistCoarse) * tempCoarse;
    if (aridity > 0.36f &&
        tempCoarse > 0.40f &&
        moistCoarse < 0.45f
        ){
        return BiomeType::DESERT;
        }

    // Cold lowlands
    if (climate < 0.16f) return BiomeType::TUNDRA;
    if (height > terrainParams.seaLevel + 30 && tempCoarse < 0.45f) return BiomeType::TUNDRA;

    // SWAMP: wet, low-lying, mild temps
    if (height <= terrainParams.seaLevel + 6 &&
        moistCoarse > 0.60f &&
        tempCoarse > 0.30f && tempCoarse < 0.80f) {
        return BiomeType::SWAMP;
    }

    // Forest: moist and not too hot
    if (moistCoarse > terrainParams.forestMoistureThreshold * 0.9f && climate < 0.65f) return BiomeType::FOREST;

    return BiomeType::PLAINS;


}

int Chunk::computeTerrainHeight(const TerrainGenerationParams& terrainParams, float worldX, float worldZ) {
    // find min/max of the erosion spline
    float eroMin = std::numeric_limits<float>::infinity();
    float eroMax = -std::numeric_limits<float>::infinity();
    for (const auto &p : erosionSpline) { eroMin = glm::min(eroMin, p.second); eroMax = glm::max(eroMax, p.second); }

    const float continentalness = getContinentalness(terrainParams, worldX, worldZ);
    const float erosion = getErosion(terrainParams, worldX, worldZ);
    const float pv = getPV(terrainParams, worldX, worldZ);

    const float baseHeight = surfaceNoiseTransformation(continentalness, 1);
    const float erosionSplineValue = surfaceNoiseTransformation(erosion, 2);
    const float pvSplineValue = surfaceNoiseTransformation(pv, 3);

    float erosionNorm = 0.0f;
    if (eroMax > eroMin) erosionNorm = glm::clamp((erosionSplineValue - eroMin) / (eroMax - eroMin), 0.0f, 1.0f);

    erosionNorm = 1.0f - erosionNorm;

    // Modulate erosion strength by location: coasts should erode less, inland/mountains more
    const float inlandMask = glm::smoothstep(-0.19f, 3.8f, continentalness); // 0 = near coast, 1 = inland
    // tune min/max erosion in world units (small compared to absolute heights from continentalness spline)
    constexpr float minErosionStrength = 2.0f;
    constexpr float maxErosionStrength = 140.0f;
    const float erosionStrength = glm::mix(minErosionStrength, maxErosionStrength, inlandMask);

    // Combine normalized spline severity with strength to get final height delta
    const float erosionDelta = erosionNorm * erosionStrength;

    const float pvFactor = pvSplineValue * (1.0f - erosionNorm);

    const float finalHeight = baseHeight - erosionDelta + pvFactor;


    int surfaceY = static_cast<int>(std::floor(finalHeight)); // round
    surfaceY = glm::clamp(surfaceY, 0, HEIGHT - 1);

    return surfaceY;
}

void Chunk::generate(const TerrainGenerationParams& terrainParams) {

    // local storage
    BlockStorage blocks;



    for (int x = 0; x < WIDTH; ++x) {
        for (int z = 0; z < DEPTH; ++z) {
            const auto worldX = static_cast<float>(originX + x);
            const auto worldZ = static_cast<float>(originZ + z);

            const int surfaceY = computeTerrainHeight(terrainParams, worldX, worldZ);

            #ifndef NDEBUG
            if (surfaceY < 0 || surfaceY >= HEIGHT) {
                std::cerr << "Invalid surfaceY: " << surfaceY << " at world (" << worldX << "," << worldZ << ")\n";
            }
            #endif

            BlockType top = BlockType::GRASS;
            BlockType fill = BlockType::DIRT;

            // SAND at sea levels
            if (surfaceY < terrainParams.seaLevel) {
                fill = BlockType::SAND;
                top = BlockType::SAND;
            }

            // Bedrock base
            for (int y = 0; y <= terrainParams.bedrockLevel; ++y)
                blocks.at(x, y, z) = BlockType::BEDROCK;

            // Terrain shaping
            for (int y = terrainParams.bedrockLevel + 1; y < surfaceY; ++y)
                blocks.at(x, y, z) = BlockType::STONE;

            // Air above
            for (int y = std::max(terrainParams.seaLevel + 1, surfaceY + 1); y < HEIGHT; ++y)
                blocks.at(x, y, z) = BlockType::AIR;

            // Compute biome
            const BiomeType biome = computeBiome(terrainParams, worldX, worldZ, surfaceY);

            // Set blocks based on biome
            for (int y = std::max(terrainParams.bedrockLevel + 1, surfaceY - 3); y < surfaceY && y < HEIGHT; y++) {
                switch (biome) {
                    case BiomeType::DESERT:
                        top = fill = BlockType::SAND;
                        break;
                    case BiomeType::TUNDRA:
                        top = BlockType::SNOW;
                        fill = BlockType::DIRT;
                        break;
                    case BiomeType::FOREST: {
                        top = BlockType::GRASS;
                        fill = BlockType::DIRT;
                        // Add trees randomly
                        if (rand() % 1000 < 10) { // 1% chance to add a tree
                            int treeHeight = 4 + rand() % 7; // Random height between 4 and 10
                            top = BlockType::DIRT;
                            for (int treeY = surfaceY; treeY < surfaceY + treeHeight; treeY++) {
                                if (treeY >= 0 && treeY < HEIGHT)
                                    blocks.at(x, treeY, z) = BlockType::LOG;
                            }
                            for (int lx = -2; lx <= 2; lx++) {
                                for (int lz = -2; lz <= 2; lz++) {
                                    for (int ly = surfaceY + treeHeight - 1; ly <= surfaceY + treeHeight + 1; ly++) {
                                        int leafX = x + lx;
                                        int leafY = ly;
                                        int leafZ = z + lz;
                                        if (abs(lx) + abs(lz) <= 3 &&
                                            leafX >= 0 && leafX < WIDTH &&
                                            leafY >= 0 && leafY < HEIGHT &&
                                            leafZ >= 0 && leafZ < DEPTH) {
                                            blocks.at(leafX, leafY, leafZ) = BlockType::LEAVES;
                                        }
                                    }
                                }
                            }
                        }
                        break;
                    }
                        
                    case BiomeType::SWAMP: top = fill = BlockType::SAND; break;
                    case BiomeType::MOUNTAIN: top = fill = BlockType::STONE; break;
                    default:
                        top = BlockType::GRASS;
                        fill = BlockType::DIRT; // PLAINS
                }

                blocks.at(x, y, z) = fill;
            }

            // Water up to sea level
            for (int y = surfaceY; y <= terrainParams.seaLevel && y < HEIGHT; ++y)
                blocks.at(x, y, z) = BlockType::WATER;

            // Set top block only if above water
            if (surfaceY > terrainParams.seaLevel) {
                blocks.at(x, surfaceY, z) = top;
            } else {
                blocks.at(x, surfaceY, z) = BlockType::SAND;
            }

        }
    }

    
    generateCaves(blocks, terrainParams);

    // encode palette and block data (same as before)
    blockIndices.encodeAll(blocks.getData(), palette, paletteMap);
}

void Chunk::generateCaves(BlockStorage &blocks, const TerrainGenerationParams &terrainParams) {
    // Cave generation (cheese + spaghetti) using 3D Perlin noise
    const int caveTopY = terrainParams.seaLevel - 20;
    const int yStart   = terrainParams.bedrockLevel + 5;
    Noise cheeseNoise(terrainParams.seed + 7890);

    for (int x = 0; x < Chunk::WIDTH; ++x) {
        const auto worldX = static_cast<float>(originX + x);
        for (int y = yStart; y <= caveTopY; ++y) {
            for (int z = 0; z < Chunk::DEPTH; ++z) {
                const auto worldZ = static_cast<float>(originZ + z);

                float noiseValue = cheeseNoise.getNoise(worldX * 0.1f, y * 0.25f, worldZ * 0.1f);
                if (noiseValue < -0.25f) {
                    blocks.at(x, y, z) = BlockType::AIR;
                }
            }
        }
    }
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
