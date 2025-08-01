//
// Created by lucas on 7/1/25.
//

#include "World.hpp"

World::World() {
}

World::~World() {
    for (const auto& pair : chunks) {
        delete pair.second;
    }
}

World::ChunkKey World::toKey(int chunkX, int chunkZ) {
    return std::make_pair(chunkX, chunkZ);
}

Chunk* World::getChunk(const int chunkX, const int chunkZ) {
    const ChunkKey key = toKey(chunkX, chunkZ);
    if (chunks.count(key) == 0) return nullptr;
    return chunks[key];
}

std::vector<Chunk*> World::getRenderedChunks()
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
	Chunk* currChunk = it->second;
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

	Chunk* currChunk = it->second;
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
	Chunk* currChunk = it->second;

	return currChunk->isBlockVisible(glm::vec3(x, y ,z));
}

void World::linkNeighbors(int chunkX, int chunkZ, Chunk* chunk) {
    if (!chunk)
		return ;

    const int dirX[] = { 0, 0, 1, -1 };
    const int dirZ[] = { 1, -1, 0, 0 };
    const int opp[]  = { SOUTH, NORTH, WEST, EAST };

    for (int dir = 0; dir < 4; ++dir) {
        int nx = chunkX + dirX[dir];
        int nz = chunkZ + dirZ[dir];

        Chunk* neighbor = getChunk(nx, nz);

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
    std::vector<ChunkKey> toRemove;
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
    for (const auto& key : toRemove) {
        // Remove from generatingChunks if present
        generatingChunks.erase(key);
        // Erase any pending future for this chunk (not strictly necessary since
        // finished futures will be ignored, but it cleans up the list)
        for (auto it = generationFutures.begin(); it != generationFutures.end(); ) {
            // We can’t peek into a future’s result without blocking, so we
            // simply compare the stored key if available using shared data.  The
            // lambda captured the key by value, so we need to get it from
            // future.get().  Instead, we skip erasing futures here and let
            // them complete; on completion we’ll discard the chunk if it
            // isn’t in the desired radius.
            ++it;
        }
        // Delete the chunk and remove from map
        auto it = chunks.find(key);
        if (it != chunks.end()) {
            delete it->second;
            chunks.erase(it);
        }
    }

    // Clear lists of chunks to generate and chunks to render.  We will
    // repopulate them based on the current camera position and loadRadius.
    chunksToGenerate.clear();
    renderedChunks.clear();



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

    for (const auto& [dx, dz, priority] : candidates) {
        const int cx = currentChunkX + dx;
        const int cz = currentChunkZ + dz;
        ChunkKey key = toKey(cx, cz);
        Chunk* chunk = getChunk(cx, cz);

        if (!chunk) {
            if (generatingChunks.find(key) == generatingChunks.end()) {
                if (generationFutures.size() < maxConcurrentGeneration) {
                    generatingChunks.insert(key);
                    generationFutures.push_back(std::async(std::launch::async, [=]() {
                        Chunk* newChunk = new Chunk(cx, cz);
                        return std::make_pair(key, newChunk);
                    }));
                }
            }
        } else {
            renderedChunks.push_back(chunk);
        }
    }


    // Process a limited number of ready futures.  This spreads the cost of
    // inserting chunks into the world over multiple frames and avoids long
    // stalls while waiting for all chunks to generate at once.  We loop
    // through the futures vector, checking each for readiness with a
    // zero-duration wait.  When a future is ready we retrieve the chunk and
    // insert it into the world, link neighbours, propagate light and update
    // chunks.
    std::size_t processed = 0;
    for (auto it = generationFutures.begin(); it != generationFutures.end() && processed < maxChunkProcessPerFrame; ) {
        std::future<std::pair<ChunkKey, Chunk*>>& fut = *it;
        if (fut.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto result = fut.get();
            ChunkKey key = result.first;
            Chunk* newChunk = result.second;
            // Remove from generating set since the task is complete
            generatingChunks.erase(key);
            // Determine if the chunk is still within the load radius.  If it
            // is far away (beyond unloadRadius) we discard it to avoid
            // accumulating memory for chunks that are no longer needed.
            int cx = key.first;
            int cz = key.second;
            int dxChunk = cx - currentChunkX;
            int dzChunk = cz - currentChunkZ;
            if (dxChunk * dxChunk + dzChunk * dzChunk > unloadRadius * unloadRadius) {
                // Outside the interest radius: delete and skip insertion
                delete newChunk;
            } else {
                // Insert into the map
                chunks[key] = newChunk;
                // Link neighbours and propagate lighting
                int chunkX = cx;
                int chunkZ = cz;
                linkNeighbors(chunkX, chunkZ, newChunk);
                newChunk->updateChunk();
            }
            // Remove this future from the list regardless of whether we
            // inserted the chunk
            it = generationFutures.erase(it);
            processed++;
        } else {
            ++it;
        }
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
            Chunk* chunk = getChunk(cx, cz);
            if (chunk) {
                renderedChunks.push_back(chunk);
            }
        }
    }

	for (auto [chunkX, chunkZ] : chunksToGenerate) {
		//makes chunk POSSIBLY visible instantly
		// Chunk* chunk = getChunk(chunkX, chunkZ);
		// if (chunk && chunk->hasAllAdjacentChunkLoaded()) {
		// 	chunk->updateChunk();
		// }

		// Check each of the 4 neighbors again now that this chunk exists to make sure there are no empty chunks. TODO: refactor.
		const int dirX[] = { 0, 0, 1, -1 };
		const int dirZ[] = { 1, -1, 0, 0 };

		for (int dir = 0; dir < 4; ++dir) {
			Chunk* neighbor = getChunk(chunkX + dirX[dir], chunkZ + dirZ[dir]);
			if (neighbor && neighbor->needsUpdate() && neighbor->hasAllAdjacentChunkLoaded()) {
				neighbor->updateChunk();
			}
		}
	}
}

void World::render(const Shader* shaderProgram) const {
    int count = 0;
    for (auto& pair : renderedChunks) {
        count++;
        pair->draw(shaderProgram);
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
