//
// Created by lucas on 8/6/25.
//

#ifndef TERRAIN_PARAMS_HPP
#define TERRAIN_PARAMS_HPP

#include "Block.hpp"
#include <vector>
#include <cstdint>

// Terrain / generation tunables exposed to code / UI
struct TerrainGenerationParams {
    // deterministic seed
    int32_t seed = 1337;

    // basic levels
    int seaLevel = 62;
    int bedrockLevel = 0;

    // river handling
    // lower -> fewer rivers, riverStrength 0..1 controls flatten amount
    float riverThreshold = 0.005f;   // fewer rivers
    float riverStrength  = 0.25f;    // weaker flattening

    // mountain/hill amplification
    float mountainBoost  = 1.6f;     // stronger mountains inland
    float PVBoost        = 1.8f;

    // smoothing applied to heightmap (0..1). Lower preserves peaks.
    float smoothingStrength = 0.25f; // preserve peaks more

    // cliff detection: slope threshold (higher -> fewer cliffs)
    float cliffSlopeThreshold = 1.6f;

    // minimum elevation above sea to allow cliffs (prevents shoreline cliffs)
    int minCliffElevation = 24;

    // Shore smoothing:
    // radius (in columns) from water to smooth, slopeFactor: how many blocks per column
    // shoreSmoothStrength: how strongly to mix the height toward the ramp target (0..1)
    int shoreSmoothRadius = 10;
    float shoreSlopeFactor = 1.5f;
    float shoreSmoothStrength = 0.9f;

    // heightmap dump settings / helpers (used by World::dumpHeightmap)
    int genSize = 1000;     // default size for quick dumps
    int downsample = 16;    // output downsample factor for dumps

};

#endif // TERRAIN_PARAMS_HPP