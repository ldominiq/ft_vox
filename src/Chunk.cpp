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

BiomeType Chunk::computeColumnBiome(const TerrainGenerationParams& params,
                                    Noise& baseNoise, Noise& detailNoise, Noise& warpNoise,
                                    Noise& erosionNoise, Noise& weirdnessNoise, Noise& riverNoise,
                                    Noise& temperatureNoise, Noise& moistureNoise,
                                    float worldX, float worldZ, float baseHeight)
{
    // If underwater -> ocean biome
    if (baseHeight <= params.seaLevel) return BiomeType::OCEAN;

    // Build very low-frequency (coarse) climate fields so biomes form large contiguous regions.
    // biomeScaleChunks controls how many chunks make up a biome patch; use an extra multiplier to ensure broad bands.
    const float chunks = glm::max(1, params.biomeScaleChunks);
    const float worldUnitsPerPatch = chunks * Chunk::WIDTH * 8.0f; // make patches even larger
    const float freqCoarse = 1.0f / glm::max(256.0f, worldUnitsPerPatch);

    // Sample coarse temperature and moisture (0..1)
    float tempCoarse = (temperatureNoise.fractalBrownianMotion2D(worldX * freqCoarse, worldZ * freqCoarse, 4, 2.0f, 0.5f) + 1.0f) * 0.5f;
    float moistCoarse = (moistureNoise.fractalBrownianMotion2D(worldX * freqCoarse * 0.9f, worldZ * freqCoarse * 0.9f, 4, 2.0f, 0.5f) + 1.0f) * 0.5f;

    // Small random regional bias so neighboring patches aren't perfectly uniform
    Noise regionBias(params.seed + 4242);
    float bias = (regionBias.fractalBrownianMotion2D(worldX * freqCoarse * 0.6f, worldZ * freqCoarse * 0.6f, 3, 2.0f, 0.5f) + 1.0f) * 0.5f;

    // Combine into a simple climate score [0..1] where lower -> cold, higher -> hot/dry
    float climate = glm::clamp(glm::mix(tempCoarse, 1.0f - moistCoarse, 0.35f) * 0.7f + bias * 0.3f, 0.0f, 1.0f);

    // Compute inland/continent factor so mountains prefer inland
    float continent = baseNoise.fractalBrownianMotion2D(worldX * 0.0005f, worldZ * 0.0005f, 6, 2.0f, 0.5f);
    float inlandFactor = glm::smoothstep(-0.455f, 0.5f, continent);

    // Mountain-range mask (medium frequency) - creates continuous ranges
    float mountainRange = baseNoise.fractalBrownianMotion2D(worldX * 0.0022f, worldZ * 0.0022f, 5, 2.0f, 0.5f);
    mountainRange = glm::clamp((mountainRange + 1.0f) * 0.5f, 0.0f, 1.0f);

    // Height-driven mountain override: if tall enough, or located in a mountain range and sufficiently inland
    if (baseHeight > params.seaLevel + 28) {
        // if high altitude and cold, prefer snow biome (shows as white on biome map)
        if (tempCoarse < params.snowTemperatureThreshold || baseHeight > params.seaLevel + 60) return BiomeType::SNOW;
        return BiomeType::MOUNTAIN;
    }
    if (mountainRange > 0.58f && inlandFactor > 0.45f) {
        if (tempCoarse < params.snowTemperatureThreshold) return BiomeType::SNOW;
        return BiomeType::MOUNTAIN;
    }

    // Sharpen climate distribution toward extremes so cold/hot regions are more common
    // Remap climate away from mid-range and amplify extremes
    climate = glm::clamp((climate - 0.5f) * 1.6f + 0.5f, 0.0f, 1.0f);

    // Snow if very cold OR high altitude + cool
    if (climate < 0.18f) return BiomeType::SNOW;
    if (baseHeight > params.seaLevel + 30 && tempCoarse < 0.45f) return BiomeType::SNOW;

    // Forest band: moist and moderately cool
    if (moistCoarse > params.forestMoistureThreshold * 0.9f && climate < 0.65f) return BiomeType::FOREST;

    // Desert: hot enough and dry
    if (climate > 0.68f && moistCoarse < (params.desertMoistureThreshold + 0.05f)) return BiomeType::DESERT;

    // Fallback plains
    return BiomeType::PLAINS;
}

// Spline control points: {continentalness, height}
static const std::vector<std::pair<float, float>> continentalnessSpline = {

    /* Ocean floor:
    (−1.0,0)

    Still ocean (flat lowland):
    (−0.8,0)

    First small rise (shallow shelf):
    (−0.7,77) ← (0.3 × 256 ≈ 77)

    Plateau (coastal plain):
    (−0.5,77)

    Steep rise (continental slope):
    (−0.3,179) ← (0.7 × 256 ≈ 179)

    Gentle slope upward (interior highlands):
    (0.2,230) ← (0.9 × 256 ≈ 230)

    Final plateau (mountains / inland plateau):
    (1.0,256) 
    */

    // VALUES REQUIRING TINKERING

    { -1.0f, 80.0f }, // Ocean floor
    { -0.9f,  40.0f }, // Deep ocean
    { -0.7f,  40.0f }, // Shallow ocean
    { -0.6f,  77.0f }, // First rise
    { -0.5f,  77.0f }, // Coastal plain
    { -0.3f, 179.0f }, // Continental slope
    { -0.28f,179.0f }, // Shelf edge
    { -0.2f, 185.0f }, // Transition zone
    {  0.2f, 200.0f }, // Highlands
    {  1.0f, 210.0f }  // Mountains / plateau

};

static const std::vector<std::pair<float, float>> erosionSpline = {
    // VALUES REQUIRING TINKERING
    { -1.00f, 256.0f },
    { -0.76f, 210.0f },
    { -0.44f, 170.0f },
    { -0.10f, 120.0f },
    {  0.03f, 135.0f },
    {  0.33f,  50.0f },
    {  0.55f,  45.0f },
    {  0.70f,  45.0f },
    {  0.72f,  65.0f },
    {  0.83f,  65.0f },
    {  0.86f,  45.0f },
    {  1.00f,  42.0f }

};

static const std::vector<std::pair<float, float>> peakValleySpline = {
    // VALUES REQUIRING TINKERING
    { -1.00f, 40.0f },
    { -0.76f, 60.0f },
    { -0.44f, 120.0f },
    { 0.10f, 200.0f },
    {  1.00f, 256.0f }
};

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

    const float frequency = 0.001f;
    const int octaves = 5;
    const float persistence = 0.5f;
    const float lacunarity = 2.0f;

    // Get the raw FBM value (already normalized by maxValue in your implementation)
    float fbm = baseNoise.fractalBrownianMotion2D(wx * frequency, wz * frequency,
                                               octaves, lacunarity, persistence);
    
    // Apply scaling factor to stretch the distribution (adjust as needed)
    float scaleFactor = 4.5f;  // Start with 2.5x and adjust based on results
    fbm *= scaleFactor;
    
    // Clamp to ensure we stay within [-1,1] after scaling
    float continentalness = glm::clamp(fbm, -1.0f, 1.0f);
    

    return continentalness;
}

float Chunk::getErosion(const TerrainGenerationParams& terrainParams, float wx, float wz) {
    Noise erosionNoise(terrainParams.seed + 237);

    const float frequency = 0.01f;
    const int octaves = 5;
    const float persistence = 0.5f;
    const float lacunarity = 2.0f;

    float erosion = erosionNoise.fractalBrownianMotion2D(wx * frequency, wz * frequency, octaves, lacunarity, persistence);
    // Scale and clamp to [-1, 1]
    float scaleFactor = 2.5f;
    erosion = glm::clamp(erosion * scaleFactor, -1.0f, 1.0f);
    return erosion;
}

float Chunk::getPV(const TerrainGenerationParams& terrainParams, float wx, float wz) {
    Noise peakValleyNoise(terrainParams.seed + 98789);

    const float frequency = 0.002f;
    const int octaves = 5;
    const float persistence = 0.5f;
    const float lacunarity = 2.0f;

    float peakValley = peakValleyNoise.fractalBrownianMotion2D(wx * frequency, wz * frequency, octaves, lacunarity, persistence);
    // Scale and clamp to [-1, 1]
    peakValley = glm::clamp(peakValley, -1.0f, 1.0f);

    return peakValley;
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

void Chunk::generate(const TerrainGenerationParams& terrainParams) {

    // local storage
    BlockStorage blocks;

    for (int x = 0; x < WIDTH; ++x) {
        for (int z = 0; z < DEPTH; ++z) {
            const int ix = x + WIDTH * z;
            const float wx = float(originX + x);
            const float wz = float(originZ + z);

            float continentalness = getContinentalness(terrainParams, wx, wz);
            float erosion = getErosion(terrainParams, wx, wz);

            float contF = surfaceNoiseTransformation(continentalness, 1);
            float eroF = surfaceNoiseTransformation(erosion, 2);
            float surfF = contF - (HEIGHT - eroF);
            int surfaceY = static_cast<int>(std::floor(surfF)); // round
            surfaceY = glm::clamp(surfaceY, 0, HEIGHT - 1);

            #ifndef NDEBUG
            if (surfaceY < 0 || surfaceY >= HEIGHT) {
                std::cerr << "Invalid surfaceY: " << surfaceY << " at world (" << wx << "," << wz << ")\n";
            }
            #endif

            BlockType top = BlockType::GRASS;
            BlockType fill = BlockType::DIRT;

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

            // // Water up to sea level
            // for (int y = surfaceY; y <= terrainParams.seaLevel && y < HEIGHT; ++y)
            //     blocks.at(x, y, z) = BlockType::WATER;

            // // surface replacement (biome dependent)
            // for (int y = std::max(terrainParams.bedrockLevel + 1, surfaceY - 3); y < surfaceY && y < HEIGHT; ++y)
            //     blocks.at(x, y, z) = fill;

            // blocks.at(x, surfaceY, z) = top;

        }
    }

    // encode palette and block data (same as before)
    blockIndices.encodeAll(blocks.getData(), palette, paletteMap);
}

// Dump a color-coded biome map as a PPM. Each column maps to a pixel.
void World::dumpBiomeMap(int centerChunkX, int centerChunkZ, int chunksX, int chunksZ, int downsample, const std::string &outPath) {
    TerrainGenerationParams& currentParams = getTerrainParams();
    if (downsample < 1) downsample = 1;

    const int chunkW = Chunk::WIDTH;
    const int chunkD = Chunk::DEPTH;
    const long worldW = static_cast<long>(chunksX) * chunkW;
    const long worldD = static_cast<long>(chunksZ) * chunkD;

    long startX = (static_cast<long>(centerChunkX) - chunksX/2) * chunkW;
    long startZ = (static_cast<long>(centerChunkZ) - chunksZ/2) * chunkD;

    const int outW = static_cast<int>((worldW + downsample - 1) / downsample);
    const int outH = static_cast<int>((worldD + downsample - 1) / downsample);

    std::vector<glm::u8vec3> img(outW * outH);

    Noise baseNoise(currentParams.seed + 1);
    Noise detailNoise(currentParams.seed + 2);
    Noise warpNoise(currentParams.seed + 3);
    Noise erosionNoise(currentParams.seed + 237);
    Noise weirdnessNoise(currentParams.seed + 98789);
    Noise moistureNoise(currentParams.seed + 54321);
    Noise riverNoise(currentParams.seed + 99999);
    Noise temperatureNoise(currentParams.seed + 424242);

    for (int oz = 0, wz = 0; wz < outH; ++wz, oz += downsample) {
        for (int ox = 0, wx = 0; wx < outW; ++wx, ox += downsample) {
            const float worldX = float(startX + ox);
            const float worldZ = float(startZ + oz);

            float baseH = Chunk::computeColumnHeight(currentParams, baseNoise, detailNoise, warpNoise, erosionNoise, weirdnessNoise, riverNoise, worldX, worldZ);
            BiomeType b = Chunk::computeColumnBiome(currentParams, baseNoise, detailNoise, warpNoise, erosionNoise, weirdnessNoise, riverNoise, temperatureNoise, moistureNoise, worldX, worldZ, baseH);

            glm::u8vec3 color(0,0,0);
            switch (b) {
                case BiomeType::PLAINS: color = glm::u8vec3(80,180,70); break; // green
                case BiomeType::OCEAN: color = glm::u8vec3(40,120,200); break; // blue water
                case BiomeType::DESERT: color = glm::u8vec3(230,200,120); break; // sand
                case BiomeType::FOREST: color = glm::u8vec3(20,120,40); break; // dark green
                case BiomeType::MOUNTAIN: color = glm::u8vec3(130,130,130); break; // grey
                case BiomeType::SNOW: color = glm::u8vec3(240,240,255); break; // white
                default: color = glm::u8vec3(0,0,0); break;
            }

            img[wx + wz * outW] = color;
        }
    }

    saveBiomePPM(outPath, img, outW, outH);
    std::cout << "Saved biome map: " << outPath << " (" << outW << "x" << outH << ")\n";
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
