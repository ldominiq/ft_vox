//
// Created by lucas on 7/1/25.
//

#include "World.hpp"

World::World() {
    std::mt19937 rng(time(nullptr));
    terrainParams.seed = rng();

	regionDirName = "region-" + std::to_string(terrainParams.seed);
	std::filesystem::create_directories(regionDirName);
    std::cout << "World seed: " << terrainParams.seed << std::endl;
}

World::World(int seed) {
	regionDirName = "region-" + std::to_string(seed);
	std::filesystem::create_directories(regionDirName);
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

	if (!outOfMemory) {
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
