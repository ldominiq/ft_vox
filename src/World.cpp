//
// Created by lucas on 7/1/25.
//

#include "World.hpp"
#include "Chunk.hpp"
#include "TerrainParams.hpp"
#include <fstream>
#include <string>

// helper to write PPM
static void saveHeightmapPPM(const std::string &path, const std::vector<float> &heightmap, int w, int h) {
    float minH = std::numeric_limits<float>::infinity();
    float maxH = -std::numeric_limits<float>::infinity();
    for (float v : heightmap) { minH = std::min(minH, v); maxH = std::max(maxH, v); }
    // avoid divide by zero
    float range = (maxH - minH);
    if (range < 1e-6f) range = 1.0f;

    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << w << " " << h << "\n255\n";
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            float v = heightmap[i + j * w];
            unsigned char c = static_cast<unsigned char>(glm::clamp((v - minH) / range, 0.0f, 1.0f) * 255.0f);
            unsigned char col[3] = { c, c, c };
            f.write(reinterpret_cast<char*>(col), 3);
        }
    }
    f.close();
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

// Dumps a world-sized heightmap by sampling noise without generating chunks
// centerChunkX/Z: center of the dump in chunk coordinates
// chunksX/chunksZ: number of chunks across and down (total image = chunksX*Chunk::WIDTH)
// downsample: sample every N world-voxels to reduce output resolution and cost
void World::dumpHeightmap(int centerChunkX, int centerChunkZ, int chunksX, int chunksZ, int downsample, int image) const {
    if (downsample < 1) downsample = 1;

    // compute world bounds in world-block coordinates
    constexpr int chunkW = Chunk::WIDTH;
    constexpr int chunkD = Chunk::DEPTH;
    const long worldW = static_cast<long>(chunksX) * chunkW;
    const long worldD = static_cast<long>(chunksZ) * chunkD;

    // top-left world coordinate (block space)
    const long startX = (static_cast<long>(centerChunkX) - chunksX/2) * chunkW;
    const long startZ = (static_cast<long>(centerChunkZ) - chunksZ/2) * chunkD;

    // output resolution after downsampling
    const int outW = static_cast<int>((worldW + downsample - 1) / downsample);
    const int outH = static_cast<int>((worldD + downsample - 1) / downsample);
    std::vector<float> img(outW * outH);
    std::vector<float> imgCont(outW * outH);
    std::vector<float> imgEro(outW * outH);
    std::vector<float> imgPV(outW * outH);
    std::vector<float> imgHumidity(outW * outH);
    std::vector<float> imgTemperature(outW * outH);

    for (int oz = 0, wz = 0; wz < outH; ++wz, oz += downsample) {
        for (int ox = 0, wx = 0; wx < outW; ++wx, ox += downsample) {
            const auto worldX = static_cast<float>(startX + ox);
            const auto worldZ = static_cast<float>(startZ + oz);

        	const float continentalness = Chunk::getContinentalness(terrainParams, worldX, worldZ);
        	const float erosion = Chunk::getErosion(terrainParams, worldX, worldZ);
        	const float pv = Chunk::getPV(terrainParams, worldX, worldZ);

            if (image == 0) {

                const int surfaceY = Chunk::computeTerrainHeight(terrainParams, worldX, worldZ);
                
                img[wx + wz * outW] = surfaceY;
                // imgCont[wx + wz * outW] = baseHeight;
                // imgEro[wx + wz * outW] = erosionDelta;
                // imgPV[wx + wz * outW] = pvFactor;
            } else if (image == 1) {
                imgCont[wx + wz * outW] = continentalness;
                imgEro[wx + wz * outW] = erosion;
                imgPV[wx + wz * outW] = pv;
            	imgHumidity[wx + wz * outW] = Chunk::getHumidity(terrainParams, worldX, worldZ);
            	imgTemperature[wx + wz * outW] = Chunk::getTemperature(terrainParams, worldX, worldZ);

            }
            
        }
    }
    if (image == 0) {
        saveHeightmapPPM("heightmap.ppm", img, outW, outH);
     //    saveHeightmapPPM("continentalnessHM.ppm", imgCont, outW, outH);
     //    saveHeightmapPPM("erosionHM.ppm", imgEro, outW, outH);
    	// saveHeightmapPPM("pvHM.ppm", imgPV, outW, outH);
    } else if (image == 1) {
        saveHeightmapPPM("continentalnessNoise.ppm", imgCont, outW, outH);
        saveHeightmapPPM("erosionNoise.ppm", imgEro, outW, outH);
    	saveHeightmapPPM("pvNoise.ppm", imgPV, outW, outH);
    	saveHeightmapPPM("humidNoise.ppm", imgHumidity, outW, outH);
    	saveHeightmapPPM("tempNoise.ppm", imgTemperature, outW, outH);
    }
}

// Dump a color-coded biome map as a PPM. Each column maps to a pixel.
void World::dumpBiomeMap(int centerChunkX, int centerChunkZ, int chunksX, int chunksZ, int downsample) {
    // Validate inputs
    if (downsample <= 0) downsample = 1;
    if (chunksX <= 0 || chunksZ <= 0) return;

    // Compute image dimensions with ceil-division to avoid 0 and off-by-one
    const int chunkW = Chunk::WIDTH;
    const int chunkD = Chunk::DEPTH;
    const int worldW = chunksX * chunkW;
    const int worldH = chunksZ * chunkD;
    const int imgW = (worldW + downsample - 1) / downsample;
    const int imgH = (worldH + downsample - 1) / downsample;

    // Guard against absurd sizes
    if (imgW <= 0 || imgH <= 0) return;

    // Pre-size output and only index inside [0, size)
    std::vector<glm::u8vec3> pixels;
    pixels.resize(static_cast<size_t>(imgW) * static_cast<size_t>(imgH));

    // Compute the starting world position (top-left) in block coords
    const int startChunkX = centerChunkX - chunksX / 2;
    const int startChunkZ = centerChunkZ - chunksZ / 2;
    const int startWorldX = startChunkX * chunkW;
    const int startWorldZ = startChunkZ * chunkD;

    // Iterate over world-space in steps of downsample and fill pixels
    for (int z = 0; z < worldH; z += downsample) {
        // Compute y (row) index using ceil-division consistent with imgH
        const int py = z / downsample;
        if (py >= imgH) break; // safety

        for (int x = 0; x < worldW; x += downsample) {
            const int px = x / downsample;
            if (px >= imgW) break; // safety

            // Map to world coordinates
            const float wx = static_cast<float>(startWorldX + x);
            const float wz = static_cast<float>(startWorldZ + z);

            // Sample your biome function (replace with your logic)
            const int height = Chunk::computeTerrainHeight(terrainParams, wx, wz);
            const BiomeType biome = Chunk::computeBiome(terrainParams, wx, wz, height);

            glm::u8vec3 color;
            switch (biome) {
                case BiomeType::PLAINS:  color = { 80, 200, 120 }; break;
                case BiomeType::DESERT:  color = { 210, 180, 80 }; break;
                case BiomeType::FOREST:  color = { 40, 160, 60 }; break;
                case BiomeType::TUNDRA:  color = { 200, 220, 230 }; break;
                case BiomeType::SWAMP:   color = { 150, 140, 45 }; break;
                case BiomeType::OCEAN:   color = { 40, 60, 160 }; break;
            	case BiomeType::MOUNTAIN: color = { 100, 100, 100 }; break;
                default:                  color = { 255, 0, 255 }; break;
            }

            const size_t idx = static_cast<size_t>(py) * static_cast<size_t>(imgW)
                             + static_cast<size_t>(px);
            if (idx < pixels.size()) {
                pixels[idx] = color;
            }
        }
    }


	std::ofstream out("biome.ppm", std::ios::binary);
	out << "P6\n" << imgW << " " << imgH << "\n255\n";
	for (size_t i = 0; i < pixels.size(); ++i) {
		const glm::u8vec3 c = pixels[i];
		char rgb[3] = { static_cast<char>(c.r),
						static_cast<char>(c.g),
						static_cast<char>(c.b) };
		out.write(rgb, 3);
	}
}


World::World() {
    std::mt19937 rng(time(nullptr));
    terrainParams.seed = rng();

	regionDirName = "region-" + std::to_string(terrainParams.seed);
	// std::filesystem::create_directories(regionDirName);
    std::cout << "World seed: " << terrainParams.seed << std::endl;
}

World::World(int seed) {
	regionDirName = "region-" + std::to_string(seed);
	// std::filesystem::create_directories(regionDirName);
    std::mt19937 rng(time(nullptr));
    terrainParams.seed = seed;
}

World::~World() {
}

ChunkPos World::toKey(int chunkX, int chunkZ) {
    return std::make_pair(chunkX, chunkZ);
}

std::shared_ptr<Chunk> World::getChunk(int chunkX, int chunkZ) {
    const ChunkPos key = toKey(chunkX, chunkZ);
    auto it = chunks.find(key);
    if (it == chunks.end())
        return nullptr;
    return it->second;
}

std::vector<std::weak_ptr<Chunk>> World::getRenderedChunks()
{
	return renderedChunks;
}

void World::globalCoordsToLocalCoords(int &x, int &y, int &z, int globalX, int globalY, int globalZ, int &chunkX, int &chunkZ)
{
	x = (globalX % Chunk::WIDTH + Chunk::WIDTH) % Chunk::WIDTH;
	z = (globalZ % Chunk::DEPTH + Chunk::DEPTH) % Chunk::DEPTH;
	y = globalY;

	chunkX = globalX / Chunk::WIDTH;
	if (globalX < 0 && globalX % Chunk::WIDTH != 0)
		chunkX--;

	chunkZ = globalZ / Chunk::DEPTH;
	if (globalZ < 0 && globalZ % Chunk::DEPTH != 0)
		chunkZ--;
}

BlockType World::getBlockWorld(glm::ivec3 globalCoords)
{
	int x, y, z;
	int chunkX, chunkZ;
	globalCoordsToLocalCoords(x, y, z, globalCoords.x, globalCoords.y, globalCoords.z, chunkX, chunkZ);

	auto it = chunks.find(std::make_pair(chunkX, chunkZ));
	if (it == chunks.end()) {
		return BlockType::AIR;
	}
	std::shared_ptr<Chunk> currChunk = it->second;
	return currChunk->getBlock(x, y, z);
}

void World::setBlockWorld(glm::ivec3 globalCoords, std::optional<glm::ivec3> faceNormal, BlockType type)
{
    // Offset the global coordinates in the direction of the face normal
    glm::ivec3 targetCoords = globalCoords;
    if (faceNormal.has_value()) {
        targetCoords += *faceNormal;
    }

    int x, y, z;
    int chunkX, chunkZ;
    globalCoordsToLocalCoords(x, y, z, 
        targetCoords.x, targetCoords.y, targetCoords.z, 
        chunkX, chunkZ);

    auto it = chunks.find(std::make_pair(chunkX, chunkZ));
    if (it == chunks.end())
        return;

    std::shared_ptr<Chunk> currChunk = it->second;
    currChunk->setBlock(x, y, z, type);
}

bool World::isBlockVisibleWorld(glm::ivec3 globalCoords)
{
	int x, y, z;
	int chunkX, chunkZ;
	globalCoordsToLocalCoords(x, y, z, globalCoords.x, globalCoords.y, globalCoords.z, chunkX, chunkZ);

	auto it = chunks.find(std::make_pair(chunkX, chunkZ));
	if (it == chunks.end()) {
		return false;
	}

	std::shared_ptr<Chunk> currChunk = it->second;
	return currChunk->isBlockVisible(glm::vec3(x, y ,z));
}

std::unordered_set<ChunkPos> World::linkNeighbors(int chunkX, int chunkZ, std::shared_ptr<Chunk> &chunk) {
	std::unordered_set<ChunkPos> chunksToBuild;

	if (!chunk) return chunksToBuild;

    const int dirX[] = { 0, 0, 1, -1 };
    const int dirZ[] = { 1, -1, 0, 0 };
    const int opp[]  = { SOUTH, NORTH, WEST, EAST };

    for (int dir = 0; dir < 4; ++dir) {
        int nx = chunkX + dirX[dir];
        int nz = chunkZ + dirZ[dir];

        std::shared_ptr<Chunk> neighbor = getChunk(nx, nz);

        chunk->setAdjacentChunks(static_cast<Direction>(dir), neighbor);
		if (chunk->hasAllAdjacentChunkLoaded())
			chunksToBuild.insert(toKey(chunkX, chunkZ));
        if (neighbor) {
            neighbor->setAdjacentChunks(opp[dir], chunk);

            if (neighbor->hasAllAdjacentChunkLoaded()) {
				chunksToBuild.insert(toKey(nx, nz));
            }
        }
    }
	return chunksToBuild;
}

void World::updateVisibleChunks(const glm::vec3& cameraPos, const glm::vec3& cameraDir) {
    // Unload distant chunks to free memory.  Chunks beyond (loadRadius + 2)
    // in a circular distance from the camera are removed.  We copy the keys
    // to a temporary list to avoid invalidating the iterator while erasing.

    const int currentChunkX = static_cast<int>(std::floor(cameraPos.x / Chunk::WIDTH));
    const int currentChunkZ = static_cast<int>(std::floor(cameraPos.z / Chunk::DEPTH));

	if (false) {
		try {
			updateRegionStreaming(currentChunkX, currentChunkZ);
		} catch (std::exception &e) {
			std::cerr << "Error in updateRegionStreaming: " << e.what() << " - possibly out of memory." << std::endl;

			outOfMemory = true;
		}
	} else {
		//remove chunks to not go out of memory;
		const int unloadRadius = loadRadius + 32;
		std::vector<ChunkPos> toRemove;
		for (const auto& entry : chunks) {
			const int cx = entry.first.first;
			const int cz = entry.first.second;
			const int dx = cx - currentChunkX;
			const int dz = cz - currentChunkZ;
			if (dx * dx + dz * dz > unloadRadius * unloadRadius) {
				toRemove.push_back(entry.first);
			}
		}
		for (auto k : toRemove){
			chunks.erase(k);
		}
	}

    // Determine which chunks we need within the circular radius.  For every
    // candidate coordinate we either mark it for generation or add it to the
    // rendered list.  We intentionally skip coordinates outside the circle to
    // approximate a circular load area.
    std::vector<std::tuple<int, int, float, float>> candidates;

    glm::vec2 camDir = glm::normalize(glm::vec2(cameraDir.x, cameraDir.z));
    float maxDist = static_cast<float>(loadRadius);

    for (int dx = -loadRadius; dx <= loadRadius; ++dx) {
        for (int dz = -loadRadius; dz <= loadRadius; ++dz) {
            if (dx * dx + dz * dz >= loadRadius * loadRadius)
                continue;

            float dist = std::sqrt(static_cast<float>(dx * dx + dz * dz));
            glm::vec2 offset(dx, dz);
            float dirScore = glm::dot(glm::normalize(offset), camDir); // [-1, 1]

            candidates.emplace_back(dx, dz, dist, dirScore);
        }
    }

    // Sort: closest first, then by direction (front first)
    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) {
            float distA = std::get<2>(a), distB = std::get<2>(b);
            if (distA != distB) return distA < distB; // nearer chunks first
            return std::get<3>(a) > std::get<3>(b);   // if same dist, prefer forward
        });
	
	std::unordered_set<ChunkPos> generatingChunks;
	uint amountOfConcurrentChunksBeingGenerated = 0;

    for (const auto& [dx, dz, dist, dirScore] : candidates) {
        const int cx = currentChunkX + dx;
        const int cz = currentChunkZ + dz;
        ChunkPos key = toKey(cx, cz);
        std::shared_ptr<Chunk> chunk = getChunk(cx, cz);

        if (!chunk && amountOfConcurrentChunksBeingGenerated < maxConcurrentGeneration) {
            generationFutures.push_back(std::async(std::launch::async, [=]() {
                std::shared_ptr<Chunk> newChunk = std::make_shared<Chunk>(cx, cz, terrainParams);
                return std::make_pair(key, newChunk);
            }));
            amountOfConcurrentChunksBeingGenerated++;
        }
        else if (chunk && chunk->preGenerated && amountOfConcurrentChunksBeingGenerated < maxConcurrentGeneration) 
		{
			generatingChunks.insert(key);
			amountOfConcurrentChunksBeingGenerated++;
			chunk->preGenerated = false;
		}
    }


    // Process a limited number of ready futures.  This spreads the cost of
    // inserting chunks into the world over multiple frames and avoids long
    // stalls while waiting for all chunks to generate at once.  We loop
    // through the futures vector, checking each for readiness with a
    // zero-duration wait. 

	// Set of chunks currently being generated asynchronously.  We use
    // ChunkKey pairs to avoid scheduling the same chunk multiple times.

	std::size_t processed = 0;
	for (auto it = generationFutures.begin(); it != generationFutures.end(); ) {
		std::future<std::pair<ChunkPos, std::shared_ptr<Chunk>>>& fut = *it;
		
		if (fut.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
			auto result = fut.get();
			generatingChunks.insert(result.first); //race condition?
			chunks[result.first] = result.second;

			it = generationFutures.erase(it);
			processed++;
		}
		else {
			it++;
		}
	}

	for (auto [chunkX, chunkZ] : generatingChunks) {
		std::shared_ptr<Chunk> currChunk = getChunk(chunkX, chunkZ);
		linkNeighbors(chunkX, chunkZ, currChunk);
	}

	std::vector<std::future<ChunkPos>> meshFutures;

	std::unordered_set<ChunkPos> chunksToBuild;
	for (auto [chunkX, chunkZ] : generatingChunks) {
		std::shared_ptr<Chunk> currChunk = getChunk(chunkX, chunkZ);
		chunksToBuild.merge(linkNeighbors(chunkX, chunkZ, currChunk));
	}

	for (auto [chunkX, chunkZ] : chunksToBuild) {
		std::shared_ptr<Chunk> currChunk = getChunk(chunkX, chunkZ);

		if (currChunk) 
		{
			meshFutures.push_back(std::async(std::launch::async, [chunkX, chunkZ, currChunk]() {
				currChunk->buildMeshData();
				return toKey(chunkX, chunkZ);
			}));
		}
	}

	for (auto it = meshFutures.begin(); it != meshFutures.end();) {
		ChunkPos pos = it->get();
		auto chunk = getChunk(pos.first, pos.second);
		if (chunk) {
			chunk->uploadMesh();
		}
		it = meshFutures.erase(it);
	}

    // Rebuild the renderedChunks list again after newly generated chunks may
    // have been inserted.  This ensures that chunks created this frame are
    // included in the rendering pass.  We simply iterate the same radius
    // again and collect loaded chunks.
    renderedChunks.clear();
    for (int dx = -loadRadius; dx <= loadRadius; ++dx) {
        for (int dz = -loadRadius; dz <= loadRadius; ++dz) {
            if (dx * dx + dz * dz >= loadRadius * loadRadius) {
                continue;
            }
            const int cx = currentChunkX + dx;
            const int cz = currentChunkZ + dz;
            std::shared_ptr<Chunk> chunk = getChunk(cx, cz);
            if (chunk) {
                renderedChunks.push_back(chunk);
            }
        }
    }
}

void World::render(const std::shared_ptr<Shader> &shaderProgram) const {
    int count = 0;
	for (auto& weakChunk : renderedChunks) {
		if (auto chunk = weakChunk.lock()) {
			chunk->draw(shaderProgram);
			count++;
		}
	}
}

// Return the number of chunks currently in the rendered list.
std::size_t World::getRenderedChunkCount() const {
    return renderedChunks.size();
}

// Return the total number of chunks currently loaded in the world (in memory).
std::size_t World::getTotalChunkCount() const {
    return chunks.size();
}

void World::saveRegionsOnExit()
{
    for (auto it = loadedRegions.begin(); it != loadedRegions.end();) {
        //saveRegion(it->first, it->second);
        it = loadedRegions.erase(it);
    }
}

void World::updateRegionStreaming(int currentChunkX, int currentChunkZ) {
    const int REGION_SIZE = 32;

    // Determine current region
    int regionX = floorDiv(currentChunkX, REGION_SIZE);
    int regionZ = floorDiv(currentChunkZ, REGION_SIZE);

    std::unordered_set<ChunkPos> regionsToKeep;

    // Always keep current region + 8 surrounding regions
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            ChunkPos neighbor(regionX + dx, regionZ + dz);
            regionsToKeep.insert(neighbor);

            if (!loadedRegions.count(neighbor)) {
                loadRegion(neighbor.first, neighbor.second);
                loadedRegions.insert(neighbor);
            }
        }
    }

    // Unload regions that are not in the 3x3 grid
    for (auto it = loadedRegions.begin(); it != loadedRegions.end();) {
        if (!regionsToKeep.count(*it)) {
            //saveRegion(it->first, it->second);
            it = loadedRegions.erase(it);
        } else {
            ++it;
        }
    }
}

//TODO handle the throws or change them to returns
void World::saveRegion(int regionX, int regionZ) {
    std::string filename = getRegionFilename(regionX, regionZ);
    std::ofstream out(filename, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot open region file for writing: " + filename);

    // --- Write metadata ---
    RegionFileMetadata metadata;
    out.write(reinterpret_cast<const char*>(&metadata), sizeof(metadata));

    // --- Reserve header space ---
    std::vector<ChunkEntry> header(REGION_SIZE * REGION_SIZE); // all zeroed
    out.write(reinterpret_cast<const char*>(header.data()), header.size() * sizeof(ChunkEntry));

    // --- Write chunks ---
	for (int x = regionX * REGION_SIZE; x < (regionX + 1) * REGION_SIZE; x++) {
		for (int z = regionZ * REGION_SIZE; z < (regionZ + 1) * REGION_SIZE; z++)
		{
			auto it = chunks.find(toKey(x, z));
			if (it == chunks.end()) continue ;
			
			std::streampos currPos = out.tellp();
			it->second->saveToStream(out);
			std::streampos newPos = out.tellp();

			ChunkEntry entry;
			entry.X = it->first.first;
			entry.Z = it->first.second;
			entry.offset = static_cast<std::uint32_t>(currPos);
			entry.size   = static_cast<std::uint32_t>(newPos - currPos);

			//int idx = (x % REGION_SIZE) * REGION_SIZE + z;
			int localX = x - regionX * REGION_SIZE;
			int localZ = z - regionZ * REGION_SIZE;
			int idx = localZ * REGION_SIZE + localX;

			header[idx] = entry;

			chunks.erase(it);
		}
	}


    // --- Rewrite header with correct entries ---
    out.seekp(sizeof(metadata));
    out.write(reinterpret_cast<const char*>(header.data()), header.size() * sizeof(ChunkEntry));
}


void World::loadRegion(int regionX, int regionZ) {
    std::string filename = getRegionFilename(regionX, regionZ);
    std::ifstream in(filename, std::ios::binary);
    if (!in) return ;

    // --- Read metadata ---
    RegionFileMetadata metadata;
    in.read(reinterpret_cast<char*>(&metadata), sizeof(metadata));
    if (std::strncmp(metadata.magic, "RGN1", 4) != 0)
        throw std::runtime_error("Invalid region file magic in " + filename);

    // --- Read header ---
    std::vector<ChunkEntry> header(REGION_SIZE * REGION_SIZE);
    in.read(reinterpret_cast<char*>(header.data()), header.size() * sizeof(ChunkEntry));

    // --- Load each chunk ---
    for (const auto& entry : header) {
        if (entry.size == 0 || entry.offset == 0) continue; // empty slot

        // Seek to the chunk data
        in.seekg(entry.offset);
        auto chunk = std::make_shared<Chunk>(entry.X, entry.Z, terrainParams, false);
        chunk->loadFromStream(in);

        // Insert into chunk map
        ChunkPos pos(entry.X, entry.Z);
        chunks[pos] = chunk;
    }
}

std::string World::getRegionFilename(int regionX, int regionZ) const {
	
    std::ostringstream ss;
    ss << regionDirName + "/r." << regionX << "." << regionZ << ".rg";
    return ss.str();
}
