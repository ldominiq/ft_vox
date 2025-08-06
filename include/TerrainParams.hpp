//
// Created by lucas on 8/6/25.
//

#ifndef TERRAINPARAMS_HPP
#define TERRAINPARAMS_HPP

#include "Block.hpp"
#include <vector>

// ==BIOME PARAMETERS==
// scaleX/scaleZ = how stretched the biome is
// amplitude = how high the terrain will be
// offset = how much the terrain is offset from 0
// blendMin/blendMax = how much the biome blends with the next one
// topBlock = the block type at the top of the biome
struct BiomeParams {
    float scaleX, scaleZ;
    float amplitude;
    float offset;
    float blendMin, blendMax;
    BlockType topBlock;
    bool useBaseNoise;
};

struct TerrainGenerationParams {
    int seed = 1336;

    // Noise frequencies
    float biomeNoiseFreq = 0.001f;
    float baseNoiseFreq = 0.02f;
    float detailNoiseFreq = 0.1f;

    // Biomes
    std::vector<BiomeParams> biomes = {
        {2.0f, 2.0f, 3.0f, 15.0f, 0.00f, 0.30f, BlockType::SAND, true},   // Desert
        {1.2f, 1.2f, 4.0f, 25.0f, 0.20f, 0.50f, BlockType::GRASS, true}, // Plains
        {2.0f, 2.0f, 8.0f, 40.0f, 0.40f, 0.70f, BlockType::DIRT, false}, // Forest
        {0.8f, 0.8f, 35.0f, 80.0f, 0.60f, 0.90f, BlockType::STONE, false}, // Mountain
        {0.6f, 0.6f, 25.0f, 120.0f, 0.85f, 1.00f, BlockType::SNOW, false}  // Snow
    };

};

#endif //TERRAINPARAMS_HPP