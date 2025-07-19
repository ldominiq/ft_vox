//
// Created by lucas on 7/1/25.
//

#include "World.hpp"
// #include <utility>

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

void World::updateVisibleChunks(const glm::vec3& cameraPos) {
    chunksToGenerate.clear();
    renderedChunks.clear();
    constexpr int radius = 2; // Load chunks around the player

    const int currentChunkX = static_cast<int>(std::floor(cameraPos.x / Chunk::WIDTH));
    const int currentChunkZ = static_cast<int>(std::floor(cameraPos.z / Chunk::DEPTH));

    for (int x = -radius; x <= radius; ++x) {
        for (int z = -radius; z <= radius; ++z) {
			// if (x * x + z * z >= radius * radius)
            	// continue; // Skip chunks outside circular radius

            Chunk *chunk = getChunk(currentChunkX + x, currentChunkZ + z);

            if (!chunk)
                chunksToGenerate.emplace_back(std::make_pair(currentChunkX + x, currentChunkZ + z));
            else
                renderedChunks.emplace_back(chunk);
        }
    }

    std::vector<std::future<std::pair<ChunkKey, Chunk*>>> futures;

    for (auto [chunkX, chunkZ] : chunksToGenerate) {
        ChunkKey key = toKey(chunkX, chunkZ);

        futures.push_back(std::async(std::launch::async, [=]() {
            Chunk* chunk = new Chunk(chunkX, chunkZ);
            return std::make_pair(key, chunk);
        }));
    }

    // Wait for all threads to finish, insert into the map (single-threaded section)
    for (auto& future : futures) {
        auto [key, chunk] = future.get();
        chunks[key] = chunk;
    }

    // Now safely link neighbors
    for (auto [chunkX, chunkZ] : chunksToGenerate) {
        linkNeighbors(chunkX, chunkZ, getChunk(chunkX, chunkZ));
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
