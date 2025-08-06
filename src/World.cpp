//
// Created by lucas on 7/1/25.
//

#include "World.hpp"

World::World() {
	std::filesystem::create_directories("region");
    std::mt19937 rng(time(nullptr));
    terrainParams.seed = rng();
    std::cout << "World seed: " << terrainParams.seed << std::endl;
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

void World::setBlockWorld(glm::ivec3 globalCoords, BlockType type)
{
	int x, y, z;
	int chunkX, chunkZ;
	globalCoordsToLocalCoords(x, y, z, globalCoords.x, globalCoords.y, globalCoords.z, chunkX, chunkZ);

	auto it = chunks.find(std::make_pair(chunkX, chunkZ));
	if (it == chunks.end())
		return ;

	std::shared_ptr<Chunk> currChunk = it->second;
	return currChunk->setBlock(x, y, z, type);
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

void World::linkNeighbors(int chunkX, int chunkZ, std::shared_ptr<Chunk> &chunk) {
    if (!chunk)
		return ;

    const int dirX[] = { 0, 0, 1, -1 };
    const int dirZ[] = { 1, -1, 0, 0 };
    const int opp[]  = { SOUTH, NORTH, WEST, EAST };

    for (int dir = 0; dir < 4; ++dir) {
        int nx = chunkX + dirX[dir];
        int nz = chunkZ + dirZ[dir];

        std::shared_ptr<Chunk> neighbor = getChunk(nx, nz);

        chunk->setAdjacentChunks(static_cast<Direction>(dir), neighbor);
        if (neighbor) {
            neighbor->setAdjacentChunks(opp[dir], chunk);

            if (neighbor->hasAllAdjacentChunkLoaded()) {
				neighbor->updateChunk();
            }
        }
    }
}

void World::updateVisibleChunks(const glm::vec3& cameraPos, const glm::vec3& cameraDir) {
    // Unload distant chunks to free memory.  Chunks beyond (loadRadius + 2)
    // in a circular distance from the camera are removed.  We copy the keys
    // to a temporary list to avoid invalidating the iterator while erasing.
    const int unloadRadius = loadRadius + 2;
    std::vector<ChunkPos> toRemove;
    const int currentChunkX = static_cast<int>(std::floor(cameraPos.x / Chunk::WIDTH));
    const int currentChunkZ = static_cast<int>(std::floor(cameraPos.z / Chunk::DEPTH));

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
		// std::cout << chunks.find(k)->second.use_count() << std::endl;
		chunks.erase(k);
	}

    // Determine which chunks we need within the circular radius.  For every
    // candidate coordinate we either mark it for generation or add it to the
    // rendered list.  We intentionally skip coordinates outside the circle to
    // approximate a circular load area.
    std::vector<std::tuple<int, int, float>> candidates;

    glm::vec2 camDir = glm::normalize(glm::vec2(cameraDir.x, cameraDir.z));

    for (int dx = -loadRadius; dx <= loadRadius; ++dx) {
        for (int dz = -loadRadius; dz <= loadRadius; ++dz) {
            if (dx * dx + dz * dz >= loadRadius * loadRadius)
                continue;

            float dist = std::sqrt(dx * dx + dz * dz);
            glm::vec2 offset(dx, dz);
            float dirScore = glm::dot(glm::normalize(offset), camDir);
            float priority = dirScore - dist * 0.05f; // prefer closer chunks and chunks in view

            candidates.emplace_back(dx, dz, priority);
        }
    }

    // Sort by priority descending (chunks in front and closer come first)
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) {
                  return std::get<2>(a) > std::get<2>(b);
              });

    // Set of chunks currently being generated asynchronously.  We use
    // ChunkKey pairs to avoid scheduling the same chunk multiple times.
    std::unordered_set<ChunkPos> generatingChunks;
	
    for (const auto& [dx, dz, priority] : candidates) {
        const int cx = currentChunkX + dx;
        const int cz = currentChunkZ + dz;
        ChunkPos key = toKey(cx, cz);
        std::shared_ptr<Chunk> chunk = getChunk(cx, cz);

        if (!chunk) {
                if (generationFutures.size() < maxConcurrentGeneration) {
                    generatingChunks.insert(key);
                    generationFutures.push_back(std::async(std::launch::async, [=]() {
                        std::shared_ptr<Chunk> newChunk = std::make_shared<Chunk>(cx, cz, terrainParams);
                        return std::make_pair(key, newChunk);
                    }));
                }
        } else {
            renderedChunks.push_back(chunk);
        }
    }


    // Process a limited number of ready futures.  This spreads the cost of
    // inserting chunks into the world over multiple frames and avoids long
    // stalls while waiting for all chunks to generate at once.  We loop
    // through the futures vector, checking each for readiness with a
    // zero-duration wait. 

	std::size_t processed = 0;
	generatingChunks.clear();
	for (auto it = generationFutures.begin(); it != generationFutures.end() && processed < maxChunkProcessPerFrame; ) {
		std::future<std::pair<ChunkPos, std::shared_ptr<Chunk>>>& fut = *it;
		
		if (fut.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
			auto result = fut.get();
			chunks[result.first] = result.second;
			generatingChunks.insert(result.first);

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
		if (currChunk && currChunk->hasAllAdjacentChunkLoaded()) currChunk->updateChunk();
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

void World::saveRegion(int regionX, int regionZ) const {

	const std::string filename = getRegionFilename(regionX, regionZ);
	bool newFile = false;
	if (!std::filesystem::exists(filename)) newFile = true;

    std::ofstream out(filename, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Failed to open file for writing");

    // 1. Write file header if new file
	RegionFileHeader header;
	if (newFile) out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // 2. Prepare and reserve chunk table
    std::vector<ChunkEntry> entries(REGION_SIZE * REGION_SIZE);
    std::size_t currentOffset = sizeof(header) + REGION_SIZE * REGION_SIZE * sizeof(ChunkEntry);

    // 3. Write chunk data
	// for (auto [key, chunk] : chunks)
	// for (int cx = regionX; cx < regionX + 32; cx++)
	// {
	// 	for (int cz = regionZ; cz < regionZ + 32; cz++)
	// 	{
	// 		std::shared_ptr<Chunk> chunk = getChunk(cx, cz);
	// 		chunk->saveToStream(out);
	// 	}
	// }

    // for (int i = 0; i < NUM_CHUNKS; ++i) {
    //     if (!chunkData[i].empty()) {
    //         entries[i].offset = static_cast<std::uint32_t>(currentOffset);
    //         entries[i].size   = static_cast<std::uint32_t>(chunkData[i].size());

    //         out.write(chunkData[i].data(), chunkData[i].size());
    //         currentOffset += chunkData[i].size();
    //     }
    // }

    // 4. Go back and write the table
    out.seekp(sizeof(header), std::ios::beg);
    out.write(reinterpret_cast<const char*>(entries.data()), entries.size() * sizeof(ChunkEntry));
}

void World::loadRegion(int regionX, int regionZ) {
    static constexpr int REGION_CHUNKS = 32;
    static constexpr int HEADER_SIZE = 4096;
    static constexpr int SECTOR_SIZE = 4096;

	const std::string filename = getRegionFilename(regionX, regionZ);
    std::ifstream in(filename, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open region file for reading: " + filename);

    const int totalEntries = REGION_CHUNKS * REGION_CHUNKS;

    // For each entry in header (4 bytes per chunk)
    for (int cz = 0; cz < REGION_CHUNKS; ++cz) {
        for (int cx = 0; cx < REGION_CHUNKS; ++cx) {
            int entryIndex = cx + cz * REGION_CHUNKS;
            std::streampos entryOffset = entryIndex * 4;

            // 1. Read the 4-byte header entry
            in.seekg(entryOffset, std::ios::beg);
            unsigned char b[4];
            in.read(reinterpret_cast<char*>(b), 4);
            if (!in) continue; // skip if failed

            uint32_t sectorOff = (b[0] << 16) | (b[1] << 8) | b[2];
            uint32_t sectors   = b[3];

            if (sectorOff == 0 || sectors == 0) {
                continue; // this chunk was never saved
            }

            // 2. Compute byte offset in file
            std::streampos chunkPos = static_cast<std::streampos>(sectorOff) * SECTOR_SIZE;

            // 3. Seek to chunk data
            in.seekg(chunkPos, std::ios::beg);

            // 4. Load chunk data
            std::shared_ptr<Chunk> chunk = std::make_shared<Chunk>(regionX + cx, regionZ + cz, terrainParams, false);
            chunk->loadFromStream(in);

            // 5. Insert into world map
            chunks[{regionX + cx, regionZ + cz}] = chunk;
        }
    }
}

std::string World::getRegionFilename(int regionX, int regionZ) const {
	
    std::ostringstream ss;
    ss << "region/r." << regionX << "." << regionZ << ".rg";
    return ss.str();
}
