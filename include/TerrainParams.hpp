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
    float biomeNoiseFreq = 0.0008f; // Large biome regions
    float baseNoiseFreq = 0.007f; // Main terrain features
    float detailNoiseFreq = 0.03f; // Small details

    // Sea level and base terrain
    int seaLevel = 64;
    int bedrockLevel = 5;

    // Continents/coasts
    float continentFreq = 0.0007f;   // slightly larger landmasses
    float warpFreq      = 0.0016f;
    float warpAmp       = 80.0f;

    // Detail
    float hillsFreq = 0.0032f;
    float ridgedFreq= 0.0050f;
    // Plains vs hills
    float plainsHillsAmp   = 3.0f;    // small undulation in flat areas
    float hillsAmp         = 6.0f;
    // Mountains
    float mtnBase          = 10.0f;   // base uplift in mountain regions
    float mtnAmp           = 60.0f;   // mountain height (peaks)

    // Climate
    float tempFreq    = 0.0011f;
    float moistFreq   = 0.0012f;
    float latitudeAmp      = 0.15f;   // modest latitude effect (optional)

    // Biome/beach/snow
    float beachWidth = 5.0f;     // wider beaches
    int   snowLine   = 170;      // snow only on high peaks (HEIGHT=256)


    // Mountain placement (regional)
    float mtnMaskFreq      = 0.0008f; // where mountains are allowed (big regions)
    float mtnMaskThreshold = 0.62f;   // higher → fewer mountain regions
    float mtnMaskSharpness = 0.10f;   // transition width

    // Climate cells to create cold/snowy plains
    float coldCellFreq     = 0.0005f; // very big cold regions
    float coldCellStrength = 0.35f;   // how much they lower temp
};

#endif //TERRAINPARAMS_HPP